#define MAX_SUBCOMMANDS 10

#include "../include/common.h"
#include <solidc/process.h>
#include <solidc/stdstreams.h>

// Global flags to the CLI.
char *env = ".env";          // dotenv file
bool csv_has_header = false; // CSV has a header

// Ignore \COPY and upload one by one to avoid unique constraints
// Only for diagnosis categories
bool incremental = false;

// The filename for a given subcommand.
// B'se its used by multiple flags its exported.
char *filename = NULL;

// Exported globals
Arena *arena = NULL;  // Arena for allocations
PGconn *conn = NULL;  // PGConn
PGresult *res = NULL; // Query/Exec result.

void FreeResult(void) {
    if (res != NULL) {
        PQclear(res);
        res = NULL;
    }
}

void runpsql_script(const char *filename) {
    assert(filename);

    const char *argv[] = {"psql", "-f", filename, NULL};
    const char *env[] = {NULL};

    Process psql;
    int status = -1;
    int ret;
    ret = process_create(&psql, "psql", argv, env);
    if (ret != 0) {
        LOG_FATAL("Failed to create process");
    }

    ret = process_wait(&psql, &status);
    if (ret != 0) {
        LOG_FATAL("Process failed with exit code: %d", status);
    }
}

static void start_psql_prompt(Subcommand *cmd) {
    (void)cmd;

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

void cleanup(void) {
    flag_destroy();

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
    flag_init();

    arena = arena_create(ARENA_DEFAULT_CHUNKSIZE, ARENA_DEFAULT_ALIGNMENT);
    if (!arena) {
        LOG_FATAL("error creating arena");
    }

    // Global dotenv flag
    global_add_flag(FLAG_STRING, "env", 'e', "dotenv file with pg env vars", &env, false);
    // ===================================================================================
    flag_add_subcommand("psql", "Start psql prompt session", start_psql_prompt);
    flag_add_subcommand("csu", "Create superuser", create_superuser);
    flag_add_subcommand("init", "Initialize self requests", initialize_selfrequests);
    // ===================================================================================
    Subcommand *uploadcmd = flag_add_subcommand(
        "pricelist", "Upload items to eclinichms inventory price list", upload_pricelist_csv);
    subcommand_add_flag(uploadcmd, FLAG_STRING, "file", 'f', "Price list file", &filename, true);
    // ===================================================================================
    Subcommand *invoices_cmd =
        flag_add_subcommand("invoices", "Upload invoices to eclinichms", upload_invoices_csv);
    subcommand_add_flag(invoices_cmd, FLAG_STRING, "file", 'f', "csv file for invoices", &filename,
                        true);
    // ===================================================================================
    Subcommand *users_cmd =
        flag_add_subcommand("users", "Upload user accounts", upload_user_accounts_csv);
    subcommand_add_flag(users_cmd, FLAG_STRING, "file", 'f', "user accounts csv", &filename, true);
    // ===================================================================================
    Subcommand *initcmd =
        flag_add_subcommand("schema", "Initialize the database schema", initialize_schema);
    subcommand_add_flag(initcmd, FLAG_STRING, "file", 'f', "Schema file", &filename, true);
    // ===================================================================================
    Subcommand *enumcmd =
        flag_add_subcommand("enums", "Initialize the database enums", initialize_enums);
    subcommand_add_flag(enumcmd, FLAG_STRING, "file", 'f', "Enums file", &filename, true);

    // ===================================================================================
    Subcommand *dxcatcmd =
        flag_add_subcommand("diagnoses", "Load diagnosis categories", upload_diagnosis_categories);
    subcommand_add_flag(dxcatcmd, FLAG_STRING, "file", 'f', "Diagnosis categories file", &filename,
                        true);
    subcommand_add_flag(dxcatcmd, FLAG_BOOL, "header", 'h', "CSV File contains header",
                        &csv_has_header, false);
    subcommand_add_flag(dxcatcmd, FLAG_BOOL, "incremental", 'i', "Incremental upload", &incremental,
                        false);

    // ==================== Parse the flags ==========================================
    Subcommand *subcmd = flag_parse(argc, argv);

    parse_env_file(env);
    connect_db();

    if (subcmd == NULL) {
        flag_print_usage(argv[0]);
        cleanup();
        return EXIT_FAILURE;
    }

    flag_invoke(subcmd);
    cleanup();
    return 0;
}
