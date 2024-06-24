#ifndef EC840C83_BDA7_484F_90EA_3A212D13F9A1
#define EC840C83_BDA7_484F_90EA_3A212D13F9A1

// Dependencies:
// sudo pacman -S libxcrypt

// Hash user passwords using bcrypt
#include "libbcrypt/bcrypt.h"
#include <stdbool.h>

// Hash user passwords using bcrypt.
// The hash is stored in the hash buffer. Returns true if successful.
bool hash_password(const char *password, char hash[BCRYPT_HASHSIZE]);

// Check if the password matches the hash
bool check_password(const char *password, const char *hash);

#endif /* EC840C83_BDA7_484F_90EA_3A212D13F9A1 */
