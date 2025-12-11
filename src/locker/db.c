#include "locker_db.h"
#include "locker_logs.h"
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

long long db_list_items(sqlite3 *db, locker_item_t **items) {
  sqlite3_stmt *stmt;

  int rc =
      sqlite3_prepare_v2(db, "SELECT COUNT(id) FROM items;", -1, &stmt, NULL);
  handle_sqlite_rc(db, rc, "SQL prepare error");

  rc = sqlite3_step(stmt);
  if(rc != SQLITE_ROW)
      handle_sqlite_rc(db, rc, "SQL step error");

  long long count = sqlite3_column_int64(stmt, 0);
  sqlite3_finalize(stmt);

  *items = malloc(sizeof(locker_item_t) * count);
  if (!(*items)) {
    perror("malloc");
    exit(EXIT_FAILURE);
  }

  rc = sqlite3_prepare_v2(
      db, "SELECT i.item_key, i.description, i.type, i.content FROM items AS i;", -1,
      &stmt, NULL);
  handle_sqlite_rc(db, rc, "SQL prepare error");

  long long idx = 0;
  while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
    (*items)[idx].key = strdup((const char *)sqlite3_column_text(stmt, 0));
    (*items)[idx].description =
        strdup((const char *)sqlite3_column_text(stmt, 1));
    (*items)[idx].type = sqlite3_column_int(stmt, 2);
    (*items)[idx].content_size = sqlite3_column_bytes(stmt, 3);

    const unsigned char *blob = sqlite3_column_blob(stmt, 3);
    (*items)[idx].content =
        malloc(sizeof(unsigned char) * (*items)[idx].content_size);

    if (!(*items)[idx].content) {
      perror("malloc");
      exit(EXIT_FAILURE);
    }
    memcpy((*items)[idx].content, blob, (*items)[idx].content_size);

    idx++;
  }
  handle_sqlite_rc(db, rc, "SQL step error");

  rc = sqlite3_finalize(stmt);
  handle_sqlite_rc(db, rc, "SQL finalize error");

  return count;
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
