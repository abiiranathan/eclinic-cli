#include <csvparser.h>
#include <libpq-fe.h>
#include <solidc/flag.h>
#include <solidc/process.h>
#include <solidc/stdstreams.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "inventory.h"
#include "log.h"

// Global flags to the CLI.
char *env = ".env";          // dotenv file
char *schema_file = NULL;    // schema.sql file
char *enums_file = NULL;     // enums.sql file.
char *dxcat_file = NULL;     // Diagnosis categories csv file.
bool csv_has_header = false; // CSV has a header
bool incremental = false;    // Ignore COPY and upload one by one to avoid unique constraints

// Global variables that need to be freed on success or error.
flag_ctx *ctx = NULL; // Flag context
Arena *arena = NULL;  // Arena for allocations
PGconn *conn = NULL;  // PGConn
PGresult *res = NULL; // exec result.

void cleanup(void);

void FreeResult() {
    if (res != NULL) {
        PQclear(res);
        res = NULL;
    }
}

static void parse_env_file(const char *env_file) {
    FILE *file = fopen(env_file, "r");
    if (file == NULL) {
        LOG_FATAL("Failed to open environment file: %s", env_file);
    }

    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    while ((read = getline(&line, &len, file)) != -1) {
        // Skip comments
        if (line[0] == '#') {
            continue;
        }

        // Remove newline character
        if (line[read - 1] == '\n') {
            line[read - 1] = '\0';
        }

        char *name = strtok(line, "=");
        char *value = strtok(NULL, "=");

        if (name == NULL || value == NULL) {
            LOG_ERROR("Invalid environment variable: %s", line);
            continue;
        }

        // trim leading and trailing whitespaces
        while (*name == ' ') {
            name++;
        }

        while (name[strlen(name) - 1] == ' ') {
            name[strlen(name) - 1] = '\0';
        }
        while (*value == ' ') {
            value++;
        }
        while (value[strlen(value) - 1] == ' ') {
            value[strlen(value) - 1] = '\0';
        }

        // set the environment variable
        setenv(name, value, 1);
    }

    free(line);
    fclose(file);
}

static void runpsql_script(const char *filename) {
    const char *argv[] = {"psql", "-f", filename, NULL};
    const char *env[] = {NULL};

    Process psql;
    int ret;
    ret = process_create(&psql, "psql", argv, env);
    if (ret != 0) {
        LOG_FATAL("Failed to create process");
    }

    process_wait(&psql, &ret);
    if (ret != 0) {
        LOG_FATAL("Process failed with exit code: %d", ret);
    }
}

static void start_psql_prompt(FlagArgs args) {
    (void)args;

    const char *argv[] = {"psql", NULL};
    const char *env[] = {NULL};

    Process psql;
    int ret;
    ret = process_create(&psql, "psql", argv, env);
    if (ret != 0) {
        LOG_FATAL("Failed to create process");
    }

    process_wait(&psql, &ret);
    if (ret != 0) {
        LOG_FATAL("Process failed with exit code: %d", ret);
    }
}

static void init_schema(FlagArgs args) {
    (void)args;
    if (schema_file == NULL) {
        LOG_FATAL("Schema file is required");
    }

    runpsql_script(schema_file);
}

static void init_enums(FlagArgs args) {
    (void)args;
    if (enums_file == NULL) {
        LOG_FATAL("Enums file is required");
    }

    runpsql_script(enums_file);
}

static void load_diagnosis_categories(FlagArgs args) {
    (void)args;
    if (dxcat_file == NULL) {
        LOG_FATAL("Diagnosis categories file is required");
    }

    if (incremental) {
        CsvParser *parser = csvparser_new(dxcat_file);
        if (!parser) {
            LOG_FATAL("failed to initialize csv parser");
        }
        csvparser_setconfig(
            parser, (CsvConfig){.has_header = csv_has_header, .skip_header = csv_has_header});
        CsvRow **rows = csvparser_parse(parser);
        if (!rows) {
            LOG_FATAL("csvparser_parse() failed");
        }

        size_t numrows = csvparser_numrows(parser);
        cstr *query = cstr_from(arena, "INSERT INTO diagnosis_categories(category) "
                                       "VALUES($1) ON CONFLICT DO NOTHING");

        // Create a prepared statement
        res = PQprepare(conn, "insert_diagnosis_category", query->data, 1, NULL);
        if (PQresultStatus(res) != PGRES_COMMAND_OK) {
            LOG_FATAL("Failed to prepare statement: %s", PQerrorMessage(conn));
        }
        FreeResult();

        for (size_t i = 0; i < numrows; ++i) {
            CsvRow *row = rows[i];
            assert(row->numFields == 1);

            // Execute the prepared statement
            const char *paramValues[1] = {row->fields[0]};
            res = PQexecPrepared(conn, "insert_diagnosis_category", 1, paramValues, NULL, NULL, 0);
            if (PQresultStatus(res) != PGRES_COMMAND_OK) {
                LOG_ERROR("Failed to execute statement: %s", PQerrorMessage(conn));
            }

            LOG_INFO("Inserted: %s", row->fields[0]);
            FreeResult();
        }
        csvparser_free(parser);
    } else {
        cstr *copy_cmd = cstr_new(arena, 512);
        cstr_append_fmt(arena, copy_cmd,
                        "\\COPY diagnosis_categories(category) FROM %s DELIMITER ',' %s",
                        dxcat_file, csv_has_header ? "CSV HEADER" : "CSV");

        LOG_INFO("%s\n", copy_cmd->data);
        // Load diagnosis categories as csv depending on whether the file has a
        // header
        const char *argv[6] = {"psql", "-c", copy_cmd->data, NULL};
        const char *env[] = {NULL};

        Process psql;
        int ret;
        ret = process_create(&psql, "psql", argv, env);
        if (ret != 0) {
            LOG_FATAL("Failed to create process");
        }

        process_wait(&psql, &ret);
        if (ret != 0) {
            LOG_FATAL("Process failed with exit code: %d", ret);
        }
    }
}

void upload_inventory(FlagArgs args) {
    (void)args;
    CsvParser *parser = csvparser_new(dxcat_file);
    if (!parser) {
        LOG_FATAL("failed to initialize csv parser");
    }

    csvparser_setconfig(parser, (CsvConfig){.has_header = true, .skip_header = true});
    CsvRow **rows = csvparser_parse(parser);
    if (!rows) {
        LOG_FATAL("csvparser_parse() failed");
    }

    size_t numrows = csvparser_numrows(parser);
    save_inventory_items(rows, numrows);
    csvparser_free(parser);
}

void read_value(const char *prompt, char *buffer, size_t size) {
    if (!readline(prompt, buffer, size)) {
        LOG_FATAL("Invalid %s Too long!!", prompt);
    }

    if (size > 0 && buffer[0] == '\0') {
        LOG_FATAL("%s Must not be empty!!", prompt);
    }
}

void create_superuser(FlagArgs args) {
    (void)args;

    char username[64] = {0}, password[64] = {0}, password_conform[64] = {0}, first_name[24] = {0},
         last_name[24] = {0}, email[64] = {0};

    read_value("Username: ", username, sizeof(username));
    read_value("First Name: ", first_name, sizeof(first_name));
    read_value("Last Name: ", last_name, sizeof(last_name));
    read_value("Email: ", email, sizeof(email));

    if (getpassword("Enter your password: ", password, sizeof(password)) == -1) {
        LOG_FATAL("Unable to read password");
    }

    if (strlen(password) < 8) {
        LOG_FATAL("Password must be at least 8 characters");
    }

    if (getpassword("Confirm your password: ", password_conform, sizeof(password_conform)) == -1) {
        LOG_FATAL("Unable to read password");
    }

    if (strcmp(password, password_conform) != 0) {
        LOG_FATAL("Passwords do not match");
    }

    // Display the values for confirmation
    printf("\n---------CONFIRM-------------------\n");
    printf("  Username:\t%s\n", username);
    printf("First Name:\t%s\n", first_name);
    printf(" Last Name:\t%s\n", last_name);
    printf("     Email:\t%s\n", email);
    printf("-----------------------------------\n");

    printf("Create superuser account? [Y/N]: ");
    char c;
    c = getchar();
    if (!(c == 'y' || c == 'Y')) {
        LOG_INFO("Operation Cancelled");
        exit(0);
    }

    const char *paramValues[7] = {username, password, first_name, last_name, email, "true", "true"};

    // Create a prepared statement
    const char *query = "INSERT INTO users(username, password, first_name, "
                        "last_name, email, active, "
                        "is_superuser, created_at, updated_at) "
                        "VALUES($1,$2,$3,$4,$5,$6,$7, NOW(), NOW())";

    res = PQprepare(conn, "create_superuser", query, 7, NULL);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        LOG_FATAL("Failed to prepare statement: %s", PQerrorMessage(conn));
    }
    FreeResult();

    // Execute the prepared statement
    res = PQexecPrepared(conn, "create_superuser", 7, paramValues, NULL, NULL, 0);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        LOG_FATAL("Failed to execute statement: %s", PQerrorMessage(conn));
    }
    LOG_INFO("Superuser account created successfully");
    FreeResult();
}

void initialize_selfrequests(FlagArgs args) {
    (void)args;

    // ==Fetch superuser created first ================================
    char *query = "SELECT id FROM users WHERE active=true AND is_superuser=true ORDER BY "
                  "created_at ASC LIMIT 1";
    int userId = -1;
    ExecStatusType status;
    res = PQexec(conn, query);
    status = PQresultStatus(res);
    if (status != PGRES_TUPLES_OK) {
        LOG_FATAL("No superuser account found. Create a superuser "
                  "account first and try again!");
    }

    char *userIdStr = PQgetvalue(res, 0, 0);
    if (userIdStr == NULL) {
        LOG_FATAL("userId is NULL");
    }

    userId = atoi(userIdStr);
    if (userId <= 0) {
        LOG_FATAL("Invalid superuser id: %d\n", userId);
    }
    FreeResult();

    // ===================== Self-request consultation inventory item =================
    query = "INSERT INTO inventory_items (name, type, dept, quantity, "
            "cost_price, created_at) "
            "VALUES($1, $2, $3, "
            "$4, $5, NOW()) ON CONFLICT DO NOTHING";

    const char *const paramValues[] = {"SELF REQUEST", "Consultation", "not_applicable", "1", "0"};

    res = PQexecParams(conn, query, 5, NULL, paramValues, NULL, NULL, 0);
    status = PQresultStatus(res);
    if (status != PGRES_COMMAND_OK) {
        LOG_FATAL("unable to insert SELF REQUEST consultation: %s", PQerrorMessage(conn));
    }

    LOG_INFO("SELF REQUEST Consultation created successfully");
    FreeResult();

    // ======================== Free consultation inventory item  ===========

    const char *const paramValues2[] = {"FREE CONSULTATION", "Consultation", "not_applicable", "1",
                                        "0"};
    res = PQexecParams(conn, query, 5, NULL, paramValues2, NULL, NULL, 0);
    status = PQresultStatus(res);
    if (status != PGRES_COMMAND_OK) {
        LOG_FATAL("unable to insert SELF REQUEST consultation: %s", PQerrorMessage(conn));
    }
    LOG_INFO("FREE CONSULTATION created successfully");
    FreeResult();

    // ========================== Self Request doctor ================

    query = "INSERT INTO users(first_name, last_name, username, permission, "
            "active,email,password, created_at, updated_at) "
            "VALUES('Self', 'Request', 'selfrequest', 8, false, "
            "'self-request@eclinichms.com', "
            "'self-request-password', NOW(), NOW()) ON CONFLICT DO NOTHING";
    res = PQexec(conn, query);
    status = PQresultStatus(res);
    if (status != PGRES_COMMAND_OK) {
        LOG_FATAL("unable to insert SELF REQUEST doctor: %s", PQerrorMessage(conn));
    }
    LOG_INFO("SELF REQUEST Doctor created successfully");
    FreeResult();

    // ========= OTC patient==========================================

    query = "INSERT INTO patients(name, birth_date, marital_status, "
            "registered_by, sex, created_at, updated_at, address, religion) "
            "VALUES($1, $2, $3, $4, $5, NOW(), NOW(), 'NOT APPLICABLE', 'Other') ON CONFLICT DO "
            "NOTHING";

    const char *const paramValues3[5] = {
        "OVER THE COUNTER", "2060-01-01", "Married", userIdStr, "Male",
    };

    res = PQexecParams(conn, query, 5, NULL, paramValues3, NULL, NULL, 0);
    status = PQresultStatus(res);
    if (status != PGRES_COMMAND_OK) {
        LOG_FATAL("unable to insert OTC patient: %s", PQerrorMessage(conn));
    }

    LOG_INFO("OTC patient created successfully");
    FreeResult();
}

// Initialize eclinic with self requests and over the counter patient.
void init_eclinic(FlagArgs args) { initialize_selfrequests(args); }

// Make sure all PG connection env variables are set.
static void validate_dotenv(void) {
    // Get the environment variable
    const char *db = secure_getenv("PGDATABASE");
    const char *host = secure_getenv("PGHOST");
    const char *user = secure_getenv("PGUSER");
    const char *password = secure_getenv("PGPASSWORD");
    const char *sslmode = secure_getenv("PGSSLMODE");
    const char *PGTZ = secure_getenv("PGTZ");

    if (db == NULL || host == NULL || user == NULL || password == NULL || sslmode == NULL ||
        PGTZ == NULL) {
        LOG_FATAL("Missing required environment variables");
    }
}

void cleanup(void) {
    if (ctx) {
        flag_context_destroy(ctx);
    }

    if (arena) {
        arena_destroy(arena);
    }

    if (res != NULL)
        FreeResult();

    if (conn) {
        PQfinish(conn);
    }
}

void connect_db(void) {
    const char *db = secure_getenv("PGDATABASE");
    const char *host = secure_getenv("PGHOST");
    const char *user = secure_getenv("PGUSER");
    const char *password = secure_getenv("PGPASSWORD");

    char *conninfo = NULL;
    asprintf(&conninfo, "postgres://%s:%s@%s:5432/%s?sslmode=disable", user, password, host, db);
    conn = PQconnectdb(conninfo);
    free(conninfo);

    if (PQstatus(conn) != CONNECTION_OK) {
        LOG_FATAL("Connection to database failed: %s", PQerrorMessage(conn));
    }
}

int main(int argc, char *argv[]) {
    ctx = flag_init();
    if (ctx == NULL) {
        LOG_FATAL("Failed to initialize flag context");
    }

    arena = arena_create(ARENA_DEFAULT_CHUNKSIZE, ARENA_DEFAULT_ALIGNMENT);
    if (!arena) {
        LOG_FATAL("error creating arena");
    }

    // Global dotenv flag
    flag_add_string(ctx, .name = "env", .value = &env, .desc = "dotenv file with pg env vars");

    // -----------------------------------
    flag_add_subcommand(ctx, "psql", "Initialize the database schema", start_psql_prompt, 1);

    // -----------------------------------
    subcommand *initcmd =
        flag_add_subcommand(ctx, "schema", "Initialize the database schema", init_schema, 1);
    subcommand_add_flag(initcmd, FLAG_STRING, "file", "Schema file", &schema_file, true);

    // -----------------------------------
    subcommand *enumcmd =
        flag_add_subcommand(ctx, "enums", "Initialize the database enums", init_enums, 1);
    subcommand_add_flag(enumcmd, FLAG_STRING, "file", "Enums file", &enums_file, true);
    // ------------------------------------
    subcommand *dxcatcmd = flag_add_subcommand(ctx, "dxcat", "Load diagnosis categories",
                                               load_diagnosis_categories, 3);
    subcommand_add_flag(dxcatcmd, FLAG_STRING, "file", "Diagnosis categories file", &dxcat_file,
                        true);
    subcommand_add_flag(dxcatcmd, FLAG_BOOL, "header", "CSV File contains header", &csv_has_header,
                        false);
    subcommand_add_flag(dxcatcmd, FLAG_BOOL, "incremental", "Incremental upload", &incremental,
                        false);

    // -------------------------------------

    flag_add_subcommand(ctx, "csu", "Create superuser", create_superuser, 1);

    // ---------------------------------------
    flag_add_subcommand(ctx, "init", "Initialize eclinic", init_eclinic, 1);

    // -------------------------------------------------------------
    subcommand *uploadcmd =
        flag_add_subcommand(ctx, "upload", "Upload items to inventory", upload_inventory, 1);
    subcommand_add_flag(uploadcmd, FLAG_STRING, "file", "Price list file", &dxcat_file, true);

    // ==================== Parse the flags ==========================================
    subcommand *subcmd = parse_flags(ctx, argc, argv);

    parse_env_file(env);
    validate_dotenv();
    connect_db();

    if (subcmd == NULL) {
        print_help_text(ctx, argv);
        cleanup();
        return EXIT_FAILURE;
    }

    // Invoke the subcommand
    flag_invoke_subcommand(subcmd, ctx);
    cleanup();
    return 0;
}
