#include "../include/bcrypt.h"

// Hash user passwords using bcrypt.
// The hash is stored in the hash buffer. Returns true if successful.
bool hash_password(const char *password, char hash[BCRYPT_HASHSIZE]) {
    char salt[BCRYPT_HASHSIZE];
    if (bcrypt_gensalt(12, salt) != 0)
        return false;
    if (bcrypt_hashpw(password, salt, hash) != 0) {
        return false;
    }
    return true;
}

// Check if the password matches the hash
bool check_password(const char *password, const char *hash) {
    return bcrypt_checkpw(password, hash) == 0;
}
