#include "../include/bcrypt.h"
#include "../include/common.h"
#include <solidc/stdstreams.h>
#include <string.h>

// Expected CSV Headers
// Username,  Title,  FirstName, LastName, Email
// johndoe, Mr, John, Doe,johndoes@gmail.com
static void upload_users(CsvRow **rows, size_t num_rows) {
    if (num_rows == 0)
        return;

    size_t nfields = rows[0]->numFields;
    if (nfields != 5) {
        fprintf(stderr, "CVS is expected to have 5 columns\n");
        exit(1);
    }

    //  ================= start a transaction ===================
    res = PQexec(conn, "BEGIN");
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        LOG_FATAL("begin transaction failed");
    }
    FreeResult();

    // Default password is the username. It must be changed by the user.
    char *stmt = "INSERT INTO users (username, title, first_name, last_name, email, password, "
                 "created_at, updated_at, is_superuser, active)"
                 "VALUES ($1, $2, $3, $4, $5, $6, NOW(), NOW(), false, true)";

    res = PQprepare(conn, "insert_users", stmt, 6, NULL);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        LOG_FATAL("Failed to prepare statement: %s", PQerrorMessage(conn));
    }
    FreeResult();

    ExecStatusType status;
    for (size_t i = 0; i < num_rows; i++) {
        char **fields = rows[i]->fields;
        const char *username = fields[0];
        const char *title = fields[1];
        const char *first_name = fields[2];
        const char *last_name = fields[3];
        const char *email = fields[4];

        // Hash the password
        char password[BCRYPT_HASHSIZE] = {0};
        if (!hash_password(username, password)) {
            LOG_FATAL("Failed to hash password for user %s", username);
        }

        const char *const paramValues[] = {
            username, title, first_name, last_name, email,
            password, // Hashed password
        };

        res = PQexecPrepared(conn, "insert_users", 6, paramValues, NULL, NULL, 0);
        status = PQresultStatus(res);
        if (status != PGRES_COMMAND_OK) {
            LOG_FATAL("%s", PQerrorMessage(conn));
        }
        FreeResult();
    }

    // Commit the transaction
    res = PQexec(conn, "COMMIT");
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        LOG_FATAL("commit transaction failed");
    }
    LOG_INFO("Uploaded %zu user account(s)", num_rows);
}

void upload_user_accounts_csv(Subcommand *cmd) {
    (void)cmd;
    assert(filename);

    CsvParser *parser = csvparser_new(filename);
    if (!parser) {
        LOG_FATAL("failed to initialize csv parser");
    }

    csvparser_setconfig(parser, (CsvConfig){.has_header = true, .skip_header = true});
    CsvRow **rows = csvparser_parse(parser);
    if (!rows) {
        LOG_FATAL("csvparser_parse() failed");
    }

    size_t numrows = csvparser_numrows(parser);
    upload_users(rows, numrows);
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

void create_superuser(Subcommand *cmd) {
    (void)cmd;

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
