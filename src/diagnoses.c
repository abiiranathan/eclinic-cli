#include "../include/common.h"
#include <solidc/cstr.h>
#include <solidc/process.h>

void upload_diagnosis_categories(Subcommand *cmd) {
    assert(filename);

    bool *incremental = (bool *)flag_value(cmd, "incremental");
    if (incremental == NULL) {
        LOG_FATAL("incremental flag is a NULL pointer");
    }

    bool *has_header = (bool *)flag_value(cmd, "header");
    if (has_header == NULL) {
        LOG_FATAL("header flag is a NULL pointer");
    }

    if (*incremental) {
        CsvParser *parser = csvparser_new(filename);
        if (!parser) {
            LOG_FATAL("failed to initialize csv parser");
        }

        csvparser_setconfig(
            parser, (CsvParserConfig){.has_header = *has_header, .skip_header = *has_header});

        CsvRow **rows = csvparser_parse(parser);
        if (!rows) {
            LOG_FATAL("csvparser_parse() failed");
        }

        size_t numrows = csvparser_numrows(parser);
        char *query = "INSERT INTO diagnosis_categories(category) "
                      " VALUES($1) ON CONFLICT DO NOTHING";

        // Create a prepared statement
        res = PQprepare(conn, "insert_diagnosis_category", query, 1, NULL);
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
                        "\\COPY diagnosis_categories(category) FROM %s DELIMITER ',' %s", filename,
                        *has_header ? "CSV HEADER" : "CSV");

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
