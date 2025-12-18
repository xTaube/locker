#include "locker.h"
#include "attrs.h"
#include "locker_db.h"
#include "locker_logs.h"
#include "locker_stringutils.h"
#include "locker_utils.h"
#include "locker_version.h"
#include "sodium/crypto_aead_xchacha20poly1305.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define LOCKER_MAGIC 0xCA80D4219AB3F102

ATTR_NODISCARD ATTR_ALLOC char *
generate_locker_filename(const char locker_name[static 1]) {
  char *locker_filename =
      malloc(sizeof(char) * (strlen(locker_name) + LOCKER_FILE_EXTENSION_LEN));

  memcpy(locker_filename, locker_name, strlen(locker_name));
  memcpy(locker_filename + strlen(locker_name), LOCKER_FILE_EXTENSION,
         LOCKER_FILE_EXTENSION_LEN);

  locker_filename[strlen(locker_name) + LOCKER_FILE_EXTENSION_LEN] = '\0';

  str_tolower(locker_filename);
  str_replace_spaces(locker_filename, '_');

  return locker_filename;
}

void write_locker_file(const char locker_name[static 1],
                       const locker_header_t *header,
                       const unsigned char encrypted_buffer[static 1]) {

  char *locker_filename = generate_locker_filename(locker_name);
  FILE *f = fopen(locker_filename, "wb");
  if (!f) {
    perror("fopen");
    exit(EXIT_FAILURE);
  }
  fwrite(header, sizeof(locker_header_t), 1, f);
  fwrite(encrypted_buffer, sizeof(unsigned char), header->locker_size, f);

  free(locker_filename);
  fclose(f);
}

locker_result_t locker_create(const char *restrict locker_name,
                              const char *restrict passphrase) {
  locker_header_t header = {0};
  header.file_version = LOCKER_FILE_VERSION;
  header.magic = LOCKER_MAGIC;

  if (!str_alphnum(locker_name)) {
    log_message("Locker name must contain only alpha numeric characters!");
    return LOCKER_NAME_FORBIDDEN_CHAR;
  }

  if (strlen(locker_name) > LOCKER_NAME_MAX_LEN) {
    log_message("Locker name is too long! Must be at most %d characters",
                LOCKER_NAME_MAX_LEN);
    return LOCKER_NAME_TOO_LONG;
  }

  if (strlen(locker_name) < 1) {
    log_message("Locker name cannot be empty.");
    return LOCKER_NAME_EMPTY;
  }

  strcpy(header.locker_name, locker_name); /* copy original name for display */

  locker_crypto_masterkey_t key[LOCKER_CRYPTO_MASTER_KEY_LEN];
  generate_salt(header.salt);

  int rc =
      derieve_key(passphrase, key, LOCKER_CRYPTO_MASTER_KEY_LEN, header.salt);
  if (rc != 0) {
    /* TODO: handle it more gently - currently I'm not sure what error is thrown
     * in each situation */
    perror("derieve_key");
    log_message("Could not derieve key from password.");
    exit(EXIT_FAILURE);
  }

  sqlite3 *db = get_empty_db();
  initdb(db);

  unsigned char *serialized_db;
  long long db_size = db_dump(db, &serialized_db);

  if (db_size < 0) {
    log_message("Negative database size. WTF");
    exit(EXIT_FAILURE);
  }

  generate_nonce(header.nonce);

  unsigned char *encrypted_db =
      malloc(sizeof(unsigned char) *
             (db_size + crypto_aead_xchacha20poly1305_IETF_ABYTES));

  crypto_aead_xchacha20poly1305_ietf_encrypt(encrypted_db, &header.locker_size,
                                             serialized_db, db_size, NULL, 0,
                                             NULL, header.nonce, key);

  write_locker_file(locker_name, &header, encrypted_db);

  free(encrypted_db);
  sqlite3_free(serialized_db);
  db_close(db);
  return LOCKER_OK;
}

bool has_extension(const char filename[static 1], const char ext[static 1]) {
  size_t lenf = strlen(filename);
  size_t lene = strlen(ext);

  if (lenf < lene)
    return false;

  return strcmp(filename + (lenf - lene), ext) == 0;
}

ATTR_ALLOC ATTR_NODISCARD
array_str_t *locker_files_lookup(const char path[static 1]) {
  DIR *dir = opendir(path);
  if (!dir) {
    perror("opendir");
    return NULL;
  }

  array_str_t *filenames = malloc(sizeof(array_str_t));
  init_item_array(filenames);

  struct dirent *entry;
  while ((entry = readdir(dir)) != NULL) {
    if (entry->d_type == DT_DIR)
      continue;
    if (has_extension(entry->d_name, ".locker")) {
        if (filenames->count >= LOCKER_MAX_LOCKER_FILE_NUMBER) {
          /* do not allow for cap overflow if there are somehow to many files */
          break;
        }
        locker_array_t_append(filenames, strdup(entry->d_name));
    }
  }

  closedir(dir);
  return filenames;
}

ATTR_NODISCARD ATTR_ALLOC locker_header_t *
read_locker_header(const char filename[static 1]) {
  FILE *f = fopen(filename, "rb");
  if (!f) {
    /*should not happen as check for locker files is done before */
    perror("fopen");
    exit(EXIT_FAILURE);
  }

  locker_header_t *header = malloc(sizeof(locker_header_t));
  fread(header, sizeof(locker_header_t), 1, f);

  if (header->magic != LOCKER_MAGIC) {
    log_message("%s file header is malformed. Locker Magic does not match.",
                filename);
    free(header);
    return NULL;
  }

  return header;
}

ATTR_ALLOC ATTR_NODISCARD
array_str_t *lockers_list() {
  char cwd[PATH_MAX];
  if (getcwd(cwd, sizeof(cwd)) == NULL) {
    perror("getcwd");
    exit(EXIT_FAILURE);
  }

  array_str_t *locker_files = locker_files_lookup(cwd);

  if (!locker_files) {
    exit(EXIT_FAILURE);
  }

  array_str_t *lockers = malloc(sizeof(array_str_t));
  init_item_array(lockers);

  if (locker_files->count == 0) {
    locker_array_t_free(locker_files, free);
    return lockers;
  }

  for (size_t i = 0; i < locker_files->count; i++) {
    locker_header_t *header = read_locker_header(locker_files->values[i]);
    if (!header) {
      continue;
    }
    locker_array_t_append(lockers, strdup(header->locker_name));
    free(header);
  }

  locker_array_t_free(locker_files, free);
  return lockers;
}

locker_result_t locker_open(locker_t **locker, const char locker_name[static 1], const char passphrase[static 1]) {
  const char *filename = generate_locker_filename(locker_name);
  FILE *f = fopen(filename, "rb");
  if (!f) {
    perror("fopen");
    return LOCKER_INVALID_LOCKER_FILE;
  }

  locker_header_t *header = malloc(sizeof(locker_header_t));
  if (!header) {
    perror("malloc");
    exit(EXIT_FAILURE);
  }

  fread(header, sizeof(locker_header_t), 1, f);

  if (header->magic != LOCKER_MAGIC) {
    free(header);
    fclose(f);

    log_message("%s file header is malformed. Locker Magic does not match.",
                filename);

    return LOCKER_MALFORMED_HEADER;
  }

  *locker = malloc(sizeof(locker_t));
  if (!(*locker)) {
    perror("malloc");
    exit(EXIT_FAILURE);
  }

  strncpy((*locker)->locker_name, locker_name, LOCKER_NAME_MAX_LEN);
  /* should read at most LOCKER_NAME_MAX_LEN chars */
  (*locker)->_header = header;

  int rc = derieve_key(passphrase, (*locker)->_key, LOCKER_CRYPTO_MASTER_KEY_LEN,
                       header->salt);
  if (rc != 0) {
    /* TODO: handle it more gently - currently I'm not sure what error is
     * thrown in each situation */
    perror("derieve_key");
    log_message("Could not derieve key from passphrase.");
    exit(EXIT_FAILURE);
  }

  unsigned char *encrypted_db =
      malloc(sizeof(unsigned char) * header->locker_size);
  if (!encrypted_db) {
    perror("malloc");
    exit(EXIT_FAILURE);
  }

  fread(encrypted_db, 1, header->locker_size, f);
  fclose(f);

  unsigned char *decrypted_db =
      malloc(sizeof(unsigned char) * header->locker_size -
             crypto_aead_xchacha20poly1305_IETF_ABYTES);
  if (!decrypted_db) {
    perror("malloc");
    exit(EXIT_FAILURE);
  }
  unsigned long long decrypted_len = 0;

  rc = crypto_aead_xchacha20poly1305_ietf_decrypt(decrypted_db, &decrypted_len, NULL, encrypted_db, header->locker_size, NULL, 0, header->nonce, (*locker)->_key);
  free(encrypted_db);

  if (rc != 0) {
    log_message("Given passphrase does not match original one.");
    free(decrypted_db);
    free((*locker)->_header);
    free(*locker);

    return LOCKER_INVALID_PASSPRHRASE;
  }

  sqlite3 *db = get_db(decrypted_len, decrypted_db);
  /* do not free decrypted_db buffer as it ownership was given to sqlite db */
  (*locker)->_db = db;

  return LOCKER_OK;
}

locker_result_t save_locker(locker_t *locker) {
    unsigned char *serialized_db;
    sqlite3_int64 db_size = db_dump(locker->_db, &serialized_db);

    if (db_size < 0) {
      log_message("Negative database size. WTF");
      exit(EXIT_FAILURE);
    }

    generate_nonce(locker->_header->nonce);

    unsigned char *encrypted_db =
        malloc(sizeof(unsigned char) *
               (db_size + crypto_aead_xchacha20poly1305_IETF_ABYTES));

    crypto_aead_xchacha20poly1305_ietf_encrypt(
        encrypted_db, &(locker->_header->locker_size), serialized_db, db_size,
        NULL, 0, NULL, locker->_header->nonce, locker->_key);

    write_locker_file(locker->locker_name, locker->_header, encrypted_db);

    sqlite3_free(serialized_db);
    free(encrypted_db);

    return LOCKER_OK;
}

locker_result_t close_locker(locker_t *locker) {
  db_close(locker->_db);
  free(locker);

  return LOCKER_OK;
}

locker_result_t locker_add_item(locker_t *locker, const char key[static 1],
                                const char description[static 1], const int content_size, const unsigned char content[content_size],
                                locker_item_type_t type) {
  if (strlen(key) > LOCKER_ITEM_KEY_MAX_LEN) {
    return LOCKER_ITEM_KEY_TOO_LONG;
  }

  if (db_item_key_exists(locker->_db, key)) {
    return LOCKER_ITEM_KEY_EXISTS;
  }

  if (strlen(description) > LOCKER_ITEM_DESCRIPTION_MAX_LEN) {
    return LOCKER_ITEM_DESCRIPTION_TOO_LONG;
  }

  if (content_size > LOCKER_ITEM_CONTENT_MAX_LEN) {
    return LOCKER_CONTENT_TOO_LONG;
  }

  db_add_item(locker->_db, key, description, content_size, content, type);

  return LOCKER_OK;
}

locker_result_t locker_add_account(locker_t *locker, const char key[static 1], const char description[static 1], const char username[static 1], const char password[static 1], const char url[static 1]) {
    if(strlen(username) > LOCKER_ITEM_ACCOUNT_USERNAME_MAX_LEN) {
        return LOCKER_ITEM_ACCOUNT_USERNAME_TOO_LONG;
    }
    if(strlen(password) > LOCKER_ITEM_ACCOUNT_PASSWORD_MAX_LEN) {
        return LOCKER_ITEM_ACCOUNT_PASSWORD_TOO_LONG;
    }
    if(strlen(url) > LOCKER_ITEM_ACCOUNT_URL_MAX_LEN) {
        return LOCKER_ITEM_ACCOUNT_URL_TOO_LONG;
    }

    char *content = calloc(LOCKER_ITEM_ACCOUNT_USERNAME_MAX_LEN+LOCKER_ITEM_ACCOUNT_PASSWORD_MAX_LEN+ LOCKER_ITEM_ACCOUNT_URL_MAX_LEN, sizeof(char));
    if(!content) {
        perror("calloc");
        exit(EXIT_FAILURE);
    }

    memcpy(content, username, strlen(username));
    memcpy(content+LOCKER_ITEM_ACCOUNT_USERNAME_MAX_LEN, password, strlen(password));
    memcpy(content+LOCKER_ITEM_ACCOUNT_USERNAME_MAX_LEN+LOCKER_ITEM_ACCOUNT_PASSWORD_MAX_LEN, url, strlen(url));

    int rc = locker_add_item(locker, key, description, LOCKER_ITEM_ACCOUNT_USERNAME_MAX_LEN+LOCKER_ITEM_ACCOUNT_PASSWORD_MAX_LEN+LOCKER_ITEM_ACCOUNT_URL_MAX_LEN, (const unsigned char *)content, LOCKER_ITEM_ACCOUNT);

    free(content);
    return rc;
}

ATTR_ALLOC ATTR_NODISCARD
array_locker_item_t *locker_get_items(locker_t *locker, const char query[LOCKER_ITEM_KEY_MAX_LEN]) {
  return db_list_items(locker->_db, query);
}

void locker_free_item(locker_item_t item) {
    free(item.key);
    free(item.description);
    free(item.content);
}
