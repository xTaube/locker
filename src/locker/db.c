#include "attrs.h"
#include "locker.h"
#include "locker_db.h"
#include "locker_logs.h"
#include "locker_utils.h"
#include "sqlite3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define handle_sqlite_rc(db, rc, message)                                        \
    do {                                                                         \
        if (!((rc) == SQLITE_OK || (rc) == SQLITE_DONE)) {                       \
            log_message("%s:%d %s: %s",                                          \
                        __FILE__, __LINE__, (message), sqlite3_errmsg(db));      \
            exit(EXIT_FAILURE);                                                  \
        }                                                                        \
    } while (0)

void initdb(sqlite3 *db) {
  const char sql[] = "CREATE TABLE IF NOT EXISTS item_types ("
                     "id INTEGER PRIMARY KEY, name TEXT UNIQUE NOT NULL"
                     ");"
                     "INSERT INTO item_types (id, name) VALUES (0, 'account'), "
                     "(1, 'apikey'), (2, 'note');"
                     "CREATE TABLE IF NOT EXISTS items ("
                     "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                     "item_key TEXT UNIQUE NOT NULL,"
                     "description TEXT,"
                     "content BLOB NOT NULL,"
                     "type INTEGER NOT NULL,"
                     "created_at INTEGER NOT NULL,"
                     "updated_at INTEGER NOT NULL,"
                     "FOREIGN KEY (type) REFERENCES item_types(id)"
                     ");";

  char *errmsg = NULL;
  if (sqlite3_exec(db, sql, NULL, NULL, &errmsg) != SQLITE_OK) {
    log_message("SQL error: %s", errmsg);
    sqlite3_free(errmsg);
    sqlite3_close(db);
    exit(EXIT_FAILURE);
  }

  log_message("Database bootstrap succeed.");
}

ATTR_NODISCARD ATTR_ALLOC sqlite3 *get_empty_db() {
  sqlite3 *db;

  if (sqlite3_open(":memory:", &db) != SQLITE_OK) {
    log_message("Cannot open database: %s", sqlite3_errmsg(db));
    exit(EXIT_FAILURE);
  }
  sqlite3_exec(db, "PRAGMA foreign_keys = ON;", NULL, NULL, NULL);

  return db;
}

ATTR_NODISCARD ATTR_ALLOC sqlite3 *get_db(sqlite3_int64 size,
                                          unsigned char buffer[size]) {
  sqlite3 *db = get_empty_db();
  sqlite3_deserialize(db, "main", buffer, size, size,
                      SQLITE_DESERIALIZE_FREEONCLOSE |
                          SQLITE_DESERIALIZE_RESIZEABLE);

  return db;
}

void db_close(sqlite3 *db) { sqlite3_close(db); }

sqlite3_int64 db_dump(sqlite3 *db, unsigned char **buffer) {
  sqlite3_int64 size;
  (*buffer) = sqlite3_serialize(db, "main", &size, 0);
  return size;
}

/* sqlite BLOB size is at max INT_MAX (4 bytes) */
void db_add_item(sqlite3 *db, const char key[static 1],
                 const char description[static 1], const int content_size,
                 const unsigned char content[content_size],
                 locker_item_type_t item_type) {

  sqlite3_stmt *stmt;
  int rc = sqlite3_prepare_v2(
      db,
      "INSERT INTO items (item_key, description, content, type, "
      "created_at, updated_at) VALUES (?1, ?2, ?3, ?4, strftime('%s','now'), strftime('%s','now'));",
      -1, &stmt, NULL);

  handle_sqlite_rc(db, rc, "SQL prepare error");

  rc = sqlite3_bind_text(stmt, 1, key, -1, SQLITE_TRANSIENT);
  handle_sqlite_rc(db, rc, "SQL bind error");

  rc = sqlite3_bind_text(stmt, 2, description, -1, SQLITE_TRANSIENT);
  handle_sqlite_rc(db, rc, "SQL bind error");

  rc = sqlite3_bind_blob(stmt, 3, content, content_size, SQLITE_TRANSIENT);
  handle_sqlite_rc(db, rc, "SQL bind error");

  rc = sqlite3_bind_int(stmt, 4, item_type);

  rc = sqlite3_step(stmt);
  handle_sqlite_rc(db, rc, "SQL step error");

  rc = sqlite3_finalize(stmt);
  handle_sqlite_rc(db, rc, "SQL finalize error");
}

ATTR_ALLOC ATTR_NODISCARD
array_locker_item_t *db_list_items(sqlite3 *db, const char query[LOCKER_ITEM_KEY_QUERY_MAX_LEN]) {
  char sql[512];
  strcpy(sql, "SELECT i.id, i.item_key, i.type FROM items AS i WHERE 1=1");

  if(strlen(query) > 0) {
      strcat(sql, " AND i.item_key LIKE ?1");
  }

  strcat(sql, " ORDER BY i.item_key ASC;");

  sqlite3_stmt *stmt;
  int rc = sqlite3_prepare_v2(
      db, sql, -1,
      &stmt, NULL);
  handle_sqlite_rc(db, rc, "SQL prepare error");

  if(strlen(query) > 0) {
    char like_query[LOCKER_ITEM_KEY_QUERY_MAX_LEN+3];
    snprintf(like_query, sizeof(like_query), "%%%s%%", query);

    rc = sqlite3_bind_text(stmt, 1, like_query, -1, SQLITE_TRANSIENT);
    handle_sqlite_rc(db, rc, "SQL bind error");
  }

  array_locker_item_t *items = malloc(sizeof(array_str_t));
  if(!items) {
      perror("malloc");
      exit(EXIT_FAILURE);
  }
  init_item_array(items);

  while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
    locker_item_t item;
    item.id = sqlite3_column_int64(stmt, 0);
    item.key = strdup((const char *)sqlite3_column_text(stmt, 1));
    item.type = sqlite3_column_int(stmt, 2);

    locker_array_append(items, item);
  }

  handle_sqlite_rc(db, rc, "SQL step error");

  rc = sqlite3_finalize(stmt);
  handle_sqlite_rc(db, rc, "SQL finalize error");

  return items;
}

ATTR_ALLOC ATTR_NODISCARD locker_item_apikey_t *db_get_apikey(sqlite3 *db, long long item_id) {
    sqlite3_stmt *stmt;

    int rc = sqlite3_prepare_v2(
        db,
        "SELECT i.id, i.item_key, i.description, i.content FROM items AS i WHERE i.id = ?1;",
        -1,
        &stmt,
        NULL
    );
    handle_sqlite_rc(db, rc, "SQL prepare error");

    rc = sqlite3_bind_int64(stmt, 1, item_id);
    handle_sqlite_rc(db, rc, "SQL bind error");

    rc = sqlite3_step(stmt);
    if(rc != SQLITE_ROW)
        handle_sqlite_rc(db, rc, "SQL step error");

    locker_item_apikey_t *apikey = malloc(sizeof(locker_item_apikey_t));
    if(!apikey) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    apikey->id = sqlite3_column_int64(stmt, 0);
    apikey->key = strdup((const char *)sqlite3_column_text(stmt, 1));
    apikey->description = strdup((const char *)sqlite3_column_text(stmt, 2));

    int content_size = sqlite3_column_bytes(stmt, 3);
    apikey->apikey = malloc((content_size+1)*sizeof(char));
    const void *content = sqlite3_column_blob(stmt, 3);

    memcpy(apikey->apikey, (char *)content, content_size);
    apikey->apikey[content_size] = '\0';

    rc = sqlite3_finalize(stmt);
    handle_sqlite_rc(db, rc, "SQL finalize error");

    return apikey;
}

ATTR_ALLOC ATTR_NODISCARD locker_item_account_t *db_get_account(sqlite3 *db, long long item_id) {
    sqlite3_stmt *stmt;

    int rc = sqlite3_prepare_v2(
        db,
        "SELECT i.id, i.item_key, i.description, i.content FROM items AS i WHERE i.id = ?1;",
        -1,
        &stmt,
        NULL
    );
    handle_sqlite_rc(db, rc, "SQL prepare error");

    rc = sqlite3_bind_int64(stmt, 1, item_id);
    handle_sqlite_rc(db, rc, "SQL bind error");

    rc = sqlite3_step(stmt);
    if(rc != SQLITE_ROW)
        handle_sqlite_rc(db, rc, "SQL step error");

    locker_item_account_t *account = malloc(sizeof(locker_item_account_t));
    if(!account) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    account->id = sqlite3_column_int64(stmt, 0);
    account->key = strdup((const char *)sqlite3_column_text(stmt, 1));

    const char *description = (const char *)sqlite3_column_text(stmt, 2);
    if(description)
        account->description = strdup(description);
    else
        account->description = NULL;

    const char *content = sqlite3_column_blob(stmt, 3);
    account->username = strdup(content);
    account->password = strdup(content+LOCKER_ITEM_ACCOUNT_USERNAME_MAX_LEN);
    account->url = strdup(content+LOCKER_ITEM_ACCOUNT_USERNAME_MAX_LEN+LOCKER_ITEM_ACCOUNT_PASSWORD_MAX_LEN);

    rc = sqlite3_finalize(stmt);
    handle_sqlite_rc(db, rc, "SQL finalize error");

    return account;
}

bool db_item_key_exists(sqlite3 *db, long long item_id, const char key[static 1]) {
  sqlite3_stmt *stmt;

  int rc = sqlite3_prepare_v2(
      db, "SELECT EXISTS(SELECT 1 FROM items WHERE id != ?1 AND item_key = ?2);", -1, &stmt, NULL);
  handle_sqlite_rc(db, rc, "SQL prepare error");

  rc = sqlite3_bind_int64(stmt, 1, item_id);
  handle_sqlite_rc(db, rc, "SQL bind error");

  rc = sqlite3_bind_text(stmt, 2, key, -1, SQLITE_TRANSIENT);
  handle_sqlite_rc(db, rc, "SQL bind error");

  rc = sqlite3_step(stmt);
  if(rc != SQLITE_ROW)
      handle_sqlite_rc(db, rc, "SQL step error");

  int exists = sqlite3_column_int(stmt, 0);

  rc = sqlite3_finalize(stmt);
  handle_sqlite_rc(db, rc, "SQL finalize error");

  return exists == 1;
}


void db_item_update(
    sqlite3 *db,
    long long item_id,
    const char key[static 1],
    const char description[static 1],
    const int content_size,
    const unsigned char content[content_size]
) {
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(
        db,
        "UPDATE items SET item_key=?1, description=?2, content=?3, updated_at=strftime('%s', 'now') WHERE id=?4;",
        -1, &stmt, NULL);

    handle_sqlite_rc(db, rc, "SQL prepare error");

    rc = sqlite3_bind_text(stmt, 1, key, -1, SQLITE_TRANSIENT);
    handle_sqlite_rc(db, rc, "SQL bind error");

    rc = sqlite3_bind_text(stmt, 2, description, -1, SQLITE_TRANSIENT);
    handle_sqlite_rc(db, rc, "SQL bind error");

    rc = sqlite3_bind_blob(stmt, 3, content, content_size, SQLITE_TRANSIENT);
    handle_sqlite_rc(db, rc, "SQL bind error");

    rc = sqlite3_bind_int64(stmt, 4, item_id);

    rc = sqlite3_step(stmt);
    handle_sqlite_rc(db, rc, "SQL step error");

    rc = sqlite3_finalize(stmt);
    handle_sqlite_rc(db, rc, "SQL finalize error");
}
