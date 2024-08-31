#include "../include/common.h"

/*
NAME,RATE,SELLING PRICE,Quantity,Expiry Date,Billable Type,Department
Inj Ceftriaxone 1g,2500,5000,100,2024-01-31,Investigation,pharmacy
Inj Dynapar 75mg,2500,5000,50,2023-06-30,Investigation,pharmacy
*/
void upload_invoices(CsvRow **rows, size_t num_rows) {
    if (num_rows == 0)
        return;

    size_t nfields = rows[0]->numFields;
    if (nfields != 7) {
        fprintf(stderr, "CVS is expected to have 7 columns\n");
        exit(1);
    }

    //  ================= start a transaction ===================
    res = PQexec(conn, "BEGIN");
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        LOG_FATAL("begin transaction failed");
    }
    FreeResult();

    char *stmt = "INSERT INTO inventory_items (name, type, cost_price, dept, quantity,"
                 "expiry_date, created_at)"
                 "VALUES ($1, $2, $3, $4, $5, $6, NOW())"
                 "  ON CONFLICT (name, type) DO UPDATE "
                 "SET "
                 "  cost_price = EXCLUDED.cost_price,"
                 "  dept = EXCLUDED.dept,"
                 "  quantity = EXCLUDED.quantity,"
                 "  expiry_date = EXCLUDED.expiry_date"
                 "  RETURNING id";

    res = PQprepare(conn, "insert_inventory_items", stmt, 6, NULL);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        LOG_FATAL("Failed to prepare statement: %s", PQerrorMessage(conn));
    }
    FreeResult();

    // Create prepared statement for prices table
    char *pstmt = "INSERT INTO prices (item_id, cash, uap,san_care, jubilee, prudential, aar,"
                  " saint_catherine, icea, liberty) VALUES ($1, $2, 0,0,0,0,0,0,0,0) "
                  " ON CONFLICT (item_id) DO UPDATE SET cash = $2 WHERE prices.item_id = $1;";

    res = PQprepare(conn, "insert_prices", pstmt, 2, NULL);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        LOG_FATAL("Failed to prepare statement: %s", PQerrorMessage(conn));
    }
    FreeResult();

    ExecStatusType status;
    for (size_t i = 0; i < num_rows; i++) {
        char **fields = rows[i]->fields;
        const char *name = fields[0];
        const char *type = fields[5];
        const char *cost_price = fields[1];
        const char *dept = fields[6];
        const char *quantity = fields[3];
        const char *expiry_date = fields[4];

        // name, type, cost_price, dept, quantity, expiry_date
        // NAME,RATE,SELLING PRICE,Quantity,Expiry Date,Billable Type,Department
        const char *const paramValues[6] = {
            name, type, cost_price, dept, quantity, expiry_date,
        };

        res = PQexecPrepared(conn, "insert_inventory_items", 6, paramValues, NULL, NULL, 0);
        status = PQresultStatus(res);
        if (status != PGRES_TUPLES_OK) {
            LOG_FATAL("%s", PQerrorMessage(conn));
        }

        // Get the id of the item to use when setting price.
        char *itemId = PQgetvalue(res, 0, 0);
        LOG_INFO("%s :ID: %s", name, itemId);

        // Insert price
        FreeResult();

        char *cashPrice = fields[2];
        if (atoi(cashPrice) > 0) {
            const char *const paramValuesPrice[2] = {itemId, cashPrice};
            res = PQexecPrepared(conn, "insert_prices", 2, paramValuesPrice, NULL, NULL, 0);
            status = PQresultStatus(res);
            if (status != PGRES_COMMAND_OK) {
                LOG_FATAL("%s", PQerrorMessage(conn));
            }
            FreeResult();
            LOG_INFO("Set price for %s to %s\n", name, cashPrice);
        }
    }

    // Commit the transaction
    res = PQexec(conn, "COMMIT");
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        LOG_FATAL("commit transaction failed");
    }
}

void upload_pricelist_csv(Subcommand *cmd) {
    (void)cmd;
    assert(filename);

    CsvParser *parser = csvparser_new(filename);
    if (!parser) {
        LOG_FATAL("failed to initialize csv parser");
    }

    csvparser_setconfig(parser, (CsvParserConfig){.has_header = true, .skip_header = true});
    CsvRow **rows = csvparser_parse(parser);
    if (!rows) {
        LOG_FATAL("csvparser_parse() failed");
    }

    size_t numrows = csvparser_numrows(parser);
    upload_invoices(rows, numrows);
    csvparser_free(parser);
}
