#include "../include/common.h"

void initialize_schema(Subcommand *cmd) {
    (void)cmd;
    runpsql_script(filename);
}

void initialize_enums(Subcommand *cmd) {
    (void)cmd;
    runpsql_script(filename);
}

void initialize_selfrequests(Subcommand *cmd) {
    (void)cmd;

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
