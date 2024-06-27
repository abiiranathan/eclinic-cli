#include "../include/common.h"
#include <stdio.h>
#include <string.h>

void parse_env_file(const char *env_file) {
    FILE *file = fopen(env_file, "r");
    if (file == NULL) {
        LOG_INFO("Failed to open dotenv file: %s", env_file);
        LOG_INFO("Trying system environment variables");

        if (!environment_valid()) {
            LOG_FATAL("No environment variables configured for postgres");
        }
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

// Make sure all PG connection env variables are set.
bool environment_valid(void) {
    // Get the environment variable
    const char *db = secure_getenv("PGDATABASE");
    const char *host = secure_getenv("PGHOST");
    const char *user = secure_getenv("PGUSER");
    const char *password = secure_getenv("PGPASSWORD");
    const char *sslmode = secure_getenv("PGSSLMODE");
    const char *PGTZ = secure_getenv("PGTZ");

    return db && host && user && password && sslmode && PGTZ;
}
