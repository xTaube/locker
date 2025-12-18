#include "attrs.h"
#include "locker.h"
#include "locker_db.h"
#include "locker_logs.h"
#include "locker_utils.h"
#include "sqlite3.h"
#include <stdbool.h>
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
  strcpy(sql, "SELECT i.item_key, i.description, i.type, i.content FROM items AS i WHERE 1=1");

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

  array_locker_item_t *items = malloc(sizeof(array_locker_item_t));
  if(!items) {
      perror("malloc");
      exit(EXIT_FAILURE);
  }
  init_item_array(items);

  while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
    locker_item_t item = {0};
    item.key = strdup((const char *)sqlite3_column_text(stmt, 0));

    const unsigned char *description = sqlite3_column_text(stmt, 1);
    item.description = description ? strdup((const char *)description) : NULL;

    item.type = sqlite3_column_int(stmt, 2);
    item.content_size = sqlite3_column_bytes(stmt, 3);

    const unsigned char *blob = sqlite3_column_blob(stmt, 3);
    item.content =
        malloc(sizeof(unsigned char) * item.content_size);

    if (!item.content) {
      perror("malloc");
      exit(EXIT_FAILURE);
    }
    memcpy(item.content, blob, item.content_size);
    locker_array_t_append(items, item);
  }
  handle_sqlite_rc(db, rc, "SQL step error");

  rc = sqlite3_finalize(stmt);
  handle_sqlite_rc(db, rc, "SQL finalize error");

  return items;
}

bool db_item_key_exists(sqlite3 *db, const char key[static 1]) {
  sqlite3_stmt *stmt;

  int rc = sqlite3_prepare_v2(
      db, "SELECT EXISTS(SELECT 1 FROM items WHERE item_key = ?1);", -1, &stmt, NULL);
  handle_sqlite_rc(db, rc, "SQL prepare error");

  rc = sqlite3_bind_text(stmt, 1, key, -1, SQLITE_TRANSIENT);
  handle_sqlite_rc(db, rc, "SQL bind error");

  rc = sqlite3_step(stmt);
  if(rc != SQLITE_ROW)
      handle_sqlite_rc(db, rc, "SQL step error");

  int exists = sqlite3_column_int(stmt, 0);

  rc = sqlite3_finalize(stmt);
  handle_sqlite_rc(db, rc, "SQL finalize error");

  return exists == 1;
}
