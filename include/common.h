#ifndef D9C2C375_862B_4276_A8AE_DCB27A42339B
#define D9C2C375_862B_4276_A8AE_DCB27A42339B
#define _GNU_SOURCE 1

#include <assert.h>
#include <libpq-fe.h>
#include <solidc/arena.h>
#include <solidc/csvparser.h>
#include <solidc/flag.h>

#include "../include/log.h"
#include <stdlib.h>

extern PGconn *conn;
extern PGresult *res;
extern void FreeResult(void);
extern char *filename;
extern Arena *arena;
extern void runpsql_script(const char *filename);

// ================= Exported subcommands ===================
void upload_invoices_csv(Subcommand *cmd);
void upload_pricelist_csv(Subcommand *cmd);
void upload_user_accounts_csv(Subcommand *cmd);
void upload_diagnosis_categories(Subcommand *cmd);
void initialize_enums(Subcommand *cmd);
void create_superuser(Subcommand *cmd);

// Init functions
void initialize_schema(Subcommand *cmd);
void initialize_enums(Subcommand *cmd);
void initialize_selfrequests(Subcommand *cmd);
void parse_env_file(const char *env_file);
bool environment_valid(void);

#endif /* D9C2C375_862B_4276_A8AE_DCB27A42339B */
