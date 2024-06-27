# eclinic

An implementation for eclinichms CLI in C with libpq and psql.

```bash

Usage: ./bin/eclinic [global flags] <subcommand> [flags]
Global Flags:
  --help | -h: Print help text and exit
  --env | -e: dotenv file with pg env vars

Subcommands:
  psql: Start psql prompt session

  csu: Create superuser

  init: Initialize self requests

  pricelist: Upload items to eclinichms inventory price list
    --file | -f: Price list file

  invoices: Upload invoices to eclinichms
    --file | -f: csv file for invoices

  users: Upload user accounts
    --file | -f: user accounts csv

  schema: Initialize the database schema
    --file | -f: Schema file

  enums: Initialize the database enums
    --file | -f: Enums file

  diagnoses: Load diagnosis categories
    --file | -f: Diagnosis categories file
    --header | -h: CSV File contains header
    --incremental | -i: Incremental upload

```

**Build Project**

```bash
make
```

> For portability, a python script will copy all shared objects to bin/libs directory. You can copy them to the same path as your binary and the linker should find them.
> They are copied automatically for you when you run make.