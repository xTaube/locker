#ifndef LOCKER_H
#define LOCKER_H

#include "attrs.h"
#include "locker_crypto.h"
#include "locker_version.h"
#include "locker_utils.h"
#include "sqlite3.h"

#define LOCKER_NAME_MAX_LEN 64
#define LOCKER_FILE_EXTENSION ".locker"
#define LOCKER_FILE_EXTENSION_LEN 7
#define LOCKER_MAX_LOCKER_FILE_NUMBER 2 << 4
#define LOCKER_PASSPHRASE_MAX_LEN 200
#define LOCKER_ITEM_KEY_MAX_LEN 2 << 9
#define LOCKER_ITEM_DESCRIPTION_MAX_LEN 2 << 9
#define LOCKER_ITEM_CONTENT_MAX_LEN 2 << 15
#define LOCKER_ITEM_ACCOUNT_USERNAME_MAX_LEN 512
#define LOCKER_ITEM_ACCOUNT_PASSWORD_MAX_LEN 512
#define LOCKER_ITEM_ACCOUNT_URL_MAX_LEN 512
#define LOCKER_ITEM_KEY_QUERY_MAX_LEN 128

typedef enum {
  LOCKER_OK = 0,
  LOCKER_NAME_TOO_LONG,
  LOCKER_NAME_EMPTY,
  LOCKER_NAME_FORBIDDEN_CHAR,
  LOCKER_INVALID_PASSPRHRASE,
  LOCKER_MALFORMED_HEADER,
  LOCKER_INVALID_LOCKER_FILE,
  LOCKER_CONTENT_TOO_LONG,
  LOCKER_ITEM_KEY_TOO_LONG,
  LOCKER_ITEM_DESCRIPTION_TOO_LONG,
  LOCKER_ITEM_KEY_EXISTS,
  LOCKER_ITEM_ACCOUNT_USERNAME_TOO_LONG,
  LOCKER_ITEM_ACCOUNT_PASSWORD_TOO_LONG,
  LOCKER_ITEM_ACCOUNT_URL_TOO_LONG,
} locker_result_t;

typedef struct {
  unsigned int file_version;
  unsigned long magic;
  char locker_name[LOCKER_NAME_MAX_LEN + 1];
  unsigned char salt[LOCKER_CRYPTO_SALT_LEN];
  unsigned char nonce[LOCKER_CRYPTO_NONCE_LEN];
  unsigned long long locker_size;
  /*
   * XChaCha20-Poly1305 can encrypt at max the file of size 2^64 bytes,
   * it's unlikly that someone has file of size 17 exabytes
   */
} locker_header_t;

typedef struct {
  char locker_name[LOCKER_NAME_MAX_LEN + 1];
  locker_header_t *_header;
  locker_crypto_masterkey_t _key[LOCKER_CRYPTO_MASTER_KEY_LEN];
  sqlite3 *_db;
} locker_t;

typedef enum {
  LOCKER_ITEM_ACCOUNT = 0,
  LOCKER_ITEM_APIKEY,
  LOCKER_ITEM_NOTE,
} locker_item_type_t;

typedef struct {
    sqlite_int64 id;
    char *key;
    locker_item_type_t type;
} locker_item_t;

DEFINE_LOCKER_ARRAY_T(locker_item_t, locker_item);

typedef struct {
    sqlite_int64 id;
    char *key;
    char *description;
    char *value;
} locker_item_apikey_t;

typedef struct {
    sqlite_int64 id;
    char *key;
    char *description;
    char *username;
    char *password;
    char *url;
} locker_item_account_t;

locker_result_t locker_create(const char locker_name[static 1],
                              const char passphrase[static 1]);

ATTR_ALLOC ATTR_NODISCARD array_str_t *lockers_list();

locker_result_t locker_open(locker_t **locker, const char locker_name[static 1], const char passphrase[static 1]);
locker_result_t save_locker(locker_t locker[static 1]);
locker_result_t close_locker(locker_t locker[static 1]);

locker_result_t locker_add_apikey(const locker_t locker[static 1], const locker_item_apikey_t item[static 1]);
locker_result_t locker_update_apikey(const locker_t locker[static 1], const locker_item_apikey_t item[static 1]);

locker_result_t locker_add_account(const locker_t locker[static 1], const locker_item_account_t account[static 1]);
locker_result_t locker_update_account(const locker_t locker[static 1], const locker_item_account_t account[static 1]);

locker_result_t locker_delete_item(const locker_t locker[static 1], const locker_item_t item[static 1]);

ATTR_ALLOC ATTR_NODISCARD array_locker_item_t *locker_get_items(locker_t locker[static 1], const char query[LOCKER_ITEM_KEY_MAX_LEN]);
ATTR_ALLOC ATTR_NODISCARD locker_item_apikey_t *locker_get_apikey(const locker_t locker[static 1], sqlite_int64 item_id);
ATTR_ALLOC ATTR_NODISCARD locker_item_account_t *locker_get_account(const locker_t locker[static 1], sqlite_int64 item_id);

void locker_free_item(locker_item_t item);
void locker_free_apikey(locker_item_apikey_t item[static 1]);
void locker_free_account(locker_item_account_t item[static 1]);

#endif
