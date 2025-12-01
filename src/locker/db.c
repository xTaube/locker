#include "locker_db.h"
#include "locker_logs.h"
#include <stdlib.h>

void initdb(sqlite3 *db) {
  const char sql[] = "CREATE TABLE IF NOT EXISTS item_types ("
                     "id INTEGER PRIMARY KEY, name TEXT UNIQUE NOT NULL"
                     ");"
                     "INSERT INTO item_types (id, name) VALUES (0, 'account'), "
                     "(1, 'apikey'), (2, 'note');"
                     "CREATE TABLE IF NOT EXISTS items ("
                     "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                     "key TEXT UNIQUE NOT NULL,"
                     "description TEXT,"
                     "content BLOB NOT NULL,"
                     "created_at INTEGER NOT NULL,"
                     "updated_at INTEGER NOT NULL"
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

sqlite3_int64 dump_db(sqlite3 *db, unsigned char **buffer) {
  sqlite3_int64 size;
  (*buffer) = sqlite3_serialize(db, "main", &size, 0);
  return size;
}
