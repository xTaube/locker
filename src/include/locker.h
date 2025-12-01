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

typedef enum {
  LOCKER_OK = 0,
  LOCKER_NAME_TOO_LONG,
  LOCKER_NAME_EMPTY,
  LOCKER_NAME_FORBIDDEN_CHAR,
} locker_result_t;

typedef struct {
  char version[VERSION_MAX_LEN + 1];
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

locker_result_t create_locker(const char *restrict locker_name,
                              const char *restrict passphrase);

int lockers_list(char ***lockers);

ATTR_NODISCARD ATTR_ALLOC locker_t *
open_locker(const char locker_name[static 1], const char passphrase[static 1]);

void save_and_close_locker(locker_t *locker);

#endif
