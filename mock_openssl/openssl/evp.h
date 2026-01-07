#ifndef MOCK_EVP_H
#define MOCK_EVP_H

typedef struct {
} EVP_CIPHER_CTX;
typedef struct {
} EVP_CIPHER;

inline EVP_CIPHER_CTX *EVP_CIPHER_CTX_new() { return 0; }
inline void EVP_CIPHER_CTX_free(EVP_CIPHER_CTX *) {}
inline const EVP_CIPHER *EVP_aes_256_ecb() { return 0; }
inline int EVP_EncryptInit_ex(EVP_CIPHER_CTX *, const EVP_CIPHER *, void *,
                              const unsigned char *, void *) {
  return 1;
}
inline int EVP_EncryptUpdate(EVP_CIPHER_CTX *, unsigned char *, int *,
                             unsigned char *, int) {
  return 1;
}

#endif
