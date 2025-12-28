#ifndef LOCKER_DB_H
#define LOCKER_DB_H

#include "attrs.h"
#include "locker.h"
#include "locker_utils.h"
#include "sqlite3.h"
#include <stdbool.h>

ATTR_NODISCARD ATTR_ALLOC sqlite3 *get_empty_db();

ATTR_NODISCARD ATTR_ALLOC sqlite3 *get_db(sqlite3_int64 size,
                                          unsigned char buffer[size]);

void db_close(sqlite3 *db);

sqlite3_int64 db_dump(sqlite3 *db, unsigned char **buffer);

void initdb(sqlite3 *db);

void db_add_item(sqlite3 *db, const char key[static 1],
                 const char description[static 1], int content_size,
                 const unsigned char content[content_size],
                 locker_item_type_t item_type);

ATTR_ALLOC ATTR_NODISCARD array_locker_item_t *db_list_items(sqlite3 *db, const char query[LOCKER_ITEM_KEY_QUERY_MAX_LEN]);
ATTR_ALLOC ATTR_NODISCARD locker_item_apikey_t *db_get_apikey(sqlite3 *db, long long item_id);
ATTR_ALLOC ATTR_NODISCARD locker_item_account_t *db_get_account(sqlite3 *db, long long item_id);

bool db_item_key_exists(sqlite3 *db, long long item_id, const char key[static 1]);

void db_item_update(
    sqlite3 *db,
    long long item_id,
    const char key[static 1],
    const char description[static 1],
    const int content_size,
    const unsigned char content[content_size]
);

void db_item_delete(sqlite3 *db, long long item_id);

#endif
