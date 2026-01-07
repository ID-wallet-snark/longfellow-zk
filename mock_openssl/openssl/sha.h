#ifndef MOCK_SHA_H
#define MOCK_SHA_H
#include <stddef.h>

typedef struct {
} SHA256_CTX;
inline int SHA256_Init(SHA256_CTX *) { return 1; }
inline int SHA256_Update(SHA256_CTX *, const void *, size_t) { return 1; }
inline int SHA256_Final(unsigned char *, SHA256_CTX *) { return 1; }

#endif
