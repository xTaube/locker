#ifndef LOCKER_CRYPTO_H
#define LOCKER_CRYPTO_H

#include "sodium/crypto_aead_xchacha20poly1305.h"
#include "sodium/crypto_pwhash.h"
#include <sodium/crypto_box.h>
#include <stddef.h>

#define LOCKER_CRYPTO_MASTER_KEY_LEN crypto_aead_xchacha20poly1305_ietf_KEYBYTES
#define LOCKER_CRYPTO_SALT_LEN crypto_pwhash_SALTBYTES
#define LOCKER_CRYPTO_NONCE_LEN crypto_aead_xchacha20poly1305_ietf_NPUBBYTES

typedef unsigned char locker_crypto_masterkey_t;

int derieve_key(const char *password, unsigned char *key_out, size_t key_len,
                const unsigned char *salt);

void generate_salt(unsigned char salt[LOCKER_CRYPTO_SALT_LEN]);

void generate_nonce(unsigned char nonce[LOCKER_CRYPTO_NONCE_LEN]);

#endif
