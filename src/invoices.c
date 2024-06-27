#include "../include/common.h"

static void upload_invoices(CsvRow **rows, size_t num_rows) {
    if (num_rows == 0)
        return;

    size_t nfields = rows[0]->numFields;
    if (nfields != 6) {
        fprintf(stderr, "CVS is expected to have 6 columns\n");
        exit(1);
    }

    //  ================= start a transaction ===================
    res = PQexec(conn, "BEGIN");
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        LOG_FATAL("begin transaction failed");
    }
    FreeResult();

    char *stmt = "INSERT INTO invoices (invoice_no, purchase_date, invoice_total, amount_paid,"
                 "supplier, cashier, balance)"
                 "VALUES ($1, $2, $3, $4, $5, $6, $3::bigint - $4::bigint)"
                 "  ON CONFLICT (invoice_no) DO UPDATE "
                 "SET "
                 "  purchase_date = EXCLUDED.purchase_date,"
                 "  invoice_total = EXCLUDED.invoice_total,"
                 "  amount_paid = EXCLUDED.amount_paid,"
                 "  supplier = EXCLUDED.supplier,"
                 "  cashier = EXCLUDED.cashier,"
                 "  balance = EXCLUDED.invoice_total - EXCLUDED.amount_paid";

    res = PQprepare(conn, "insert_invoices", stmt, 6, NULL);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        LOG_FATAL("Failed to prepare statement: %s", PQerrorMessage(conn));
    }
    FreeResult();

    ExecStatusType status;
    for (size_t i = 0; i < num_rows; i++) {
        char **fields = rows[i]->fields;
        const char *invoice_no = fields[0];
        const char *purchase_date = fields[1];
        const char *invoice_total = fields[2];
        const char *amount_paid = fields[3];
        const char *supplier = fields[4];
        const char *cashier = fields[5];

        // name, type, cost_price, dept, quantity, expiry_date
        // NAME,RATE,SELLING PRICE,Quantity,Expiry Date,Billable Type,Department
        const char *const paramValues[6] = {
            invoice_no, purchase_date, invoice_total, amount_paid, supplier, cashier,

        };

        res = PQexecPrepared(conn, "insert_invoices", 6, paramValues, NULL, NULL, 0);
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

    LOG_INFO("Uploaded %zu invoice(s)", num_rows);
}

// Subcommand for uploading invoices.
void upload_invoices_csv(Subcommand *cmd) {
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
    upload_invoices(rows, numrows);
    csvparser_free(parser);
}
