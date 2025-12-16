#ifndef LOCKER_H
#define LOCKER_H

#include "attrs.h"
#include "locker_crypto.h"
#include "locker_version.h"
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

typedef enum {
  LOCKER_OK = 0,
  LOCKER_NAME_TOO_LONG,
  LOCKER_NAME_EMPTY,
  LOCKER_NAME_FORBIDDEN_CHAR,
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
  /* technically might be overflowed but since it's only for
   * one person I guess it would not (the file must be around 17 exabytes huge
   * to exceed this)*/
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
  char *key;
  char *description;
  locker_item_type_t type;
  unsigned char *content;
  int content_size;
  /* content size cannot be larger then INT_MAX (4 bytes) - sqlite constraint */
} locker_item_t;

locker_result_t locker_create(const char locker_name[static 1],
                              const char passphrase[static 1]);

int lockers_list(char ***lockers);

ATTR_NODISCARD ATTR_ALLOC locker_t *
locker_open(const char locker_name[static 1], const char passphrase[static 1]);

locker_result_t save_locker(locker_t *locker);

locker_result_t close_locker(locker_t *locker);

locker_result_t locker_add_item(locker_t *locker, const char key[static 1],
                                const char description[static 1], const int content_size, const unsigned char content[content_size],
                                locker_item_type_t type);


locker_result_t locker_add_account(locker_t *locker, const char key[static 1], const char description[static 1], const char username[static 1], const char password[static 1], const char url[static 1]);

long long locker_get_items(locker_t *locker, locker_item_t **items);

void free_locker_items_list(long long n_items, locker_item_t items[n_items]);

#endif
