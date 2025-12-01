#ifndef LOCKER_DB_H
#define LOCKER_DB_H

#include "attrs.h"
#include "sqlite3.h"

ATTR_NODISCARD ATTR_ALLOC sqlite3 *get_empty_db();

ATTR_NODISCARD ATTR_ALLOC sqlite3 *get_db(sqlite3_int64 size,
                                          unsigned char buffer[size]);

void db_close(sqlite3 *db);

sqlite3_int64 dump_db(sqlite3 *db, unsigned char **buffer);

void initdb(sqlite3 *db);
#endif
