#include "locker_crypto.h"
#include "sodium/randombytes.h"
#include <string.h>

int derieve_key(const char *password, unsigned char *key_out, size_t key_len,
                const unsigned char *salt) {

  return crypto_pwhash(key_out, key_len, password, strlen(password), salt,
                       crypto_pwhash_OPSLIMIT_INTERACTIVE,
                       crypto_pwhash_MEMLIMIT_INTERACTIVE,
                       crypto_pwhash_ALG_ARGON2ID13);
}

void generate_salt(unsigned char salt[LOCKER_CRYPTO_SALT_LEN]) {
  randombytes_buf(salt, LOCKER_CRYPTO_SALT_LEN);
}

void generate_nonce(unsigned char nonce[LOCKER_CRYPTO_NONCE_LEN]) {
  randombytes_buf(nonce, LOCKER_CRYPTO_NONCE_LEN);
}
