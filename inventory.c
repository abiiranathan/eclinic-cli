#include "inventory.h"
#include "bcrypt.h"
#include "log.h"
#include <libpq-fe.h>
#include <stdlib.h>

extern PGconn *conn;
extern PGresult *res;

extern void FreeResult();

/*
NAME,RATE,SELLING PRICE,Quantity,Expiry Date,Billable Type,Department
Inj Ceftriaxone 1g,2500,5000,100,2024-01-31,Investigation,pharmacy
Inj Dynapar 75mg,2500,5000,50,2023-06-30,Investigation,pharmacy
*/

void save_inventory_items(CsvRow **rows, size_t num_rows) {
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

/*
Expected CSV Headers:
InvoiceNo, PurchaseDate, InvoiceTotal, AmountPaid, Supplier, Cashier
INV-001,2023-02-12,300000,300000,Abacus Pharma Ltd,John
*/
void upload_invoices(CsvRow **rows, size_t num_rows) {
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

// Expected CSV Headers:
// Username,  Title,  FirstName, LastName, Email
// johndoe, Mr, John, Doe,johndoes@gmail.com
void upload_users(CsvRow **rows, size_t num_rows) {
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