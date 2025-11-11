// Copyright 2025 Google LLC.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Tool to generate cryptographically valid test vectors for ptrcred_age_over_18_test.

#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/evp.h>
#include <openssl/obj_mac.h>
#include <openssl/sha.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

void print_hex(const char* label, const uint8_t* data, size_t len) {
  printf("%s: 0x", label);
  for (size_t i = 0; i < len; ++i) {
    printf("%02x", data[i]);
  }
  printf("\n");
}

void sha256(const uint8_t* data, size_t len, uint8_t out[32]) {
  SHA256_CTX ctx;
  SHA256_Init(&ctx);
  SHA256_Update(&ctx, data, len);
  SHA256_Final(out, &ctx);
}

bool ecdsa_sign_p256(EC_KEY* key, const uint8_t hash[32], uint8_t r_out[32], uint8_t s_out[32]) {
  ECDSA_SIG* sig = ECDSA_do_sign(hash, 32, key);
  if (!sig) return false;

  const BIGNUM* r_bn = nullptr;
  const BIGNUM* s_bn = nullptr;
  ECDSA_SIG_get0(sig, &r_bn, &s_bn);

  memset(r_out, 0, 32);
  memset(s_out, 0, 32);
  BN_bn2bin(r_bn, r_out + (32 - BN_num_bytes(r_bn)));
  BN_bn2bin(s_bn, s_out + (32 - BN_num_bytes(s_bn)));

  ECDSA_SIG_free(sig);
  return true;
}

int main() {
  // Generate issuer key (P-256).
  EC_KEY* issuer_key = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
  if (!issuer_key || !EC_KEY_generate_key(issuer_key)) {
    fprintf(stderr, "Failed to generate issuer key\n");
    return 1;
  }

  const EC_POINT* issuer_pub = EC_KEY_get0_public_key(issuer_key);
  const EC_GROUP* group = EC_KEY_get0_group(issuer_key);
  BIGNUM* pkx_bn = BN_new();
  BIGNUM* pky_bn = BN_new();
  EC_POINT_get_affine_coordinates_GFp(group, issuer_pub, pkx_bn, pky_bn, nullptr);

  uint8_t pkx[32], pky[32];
  memset(pkx, 0, 32);
  memset(pky, 0, 32);
  BN_bn2bin(pkx_bn, pkx + (32 - BN_num_bytes(pkx_bn)));
  BN_bn2bin(pky_bn, pky + (32 - BN_num_bytes(pky_bn)));

  // Generate device key.
  EC_KEY* device_key = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
  if (!device_key || !EC_KEY_generate_key(device_key)) {
    fprintf(stderr, "Failed to generate device key\n");
    return 1;
  }

  const EC_POINT* device_pub = EC_KEY_get0_public_key(device_key);
  BIGNUM* dpkx_bn = BN_new();
  BIGNUM* dpky_bn = BN_new();
  EC_POINT_get_affine_coordinates_GFp(group, device_pub, dpkx_bn, dpky_bn, nullptr);

  uint8_t dpkx[32], dpky[32];
  memset(dpkx, 0, 32);
  memset(dpky, 0, 32);
  BN_bn2bin(dpkx_bn, dpkx + (32 - BN_num_bytes(dpkx_bn)));
  BN_bn2bin(dpky_bn, dpky + (32 - BN_num_bytes(dpky_bn)));

  // Build credential.
  uint8_t cred[170];
  memset(cred, 0, sizeof(cred));
  
  // Header: 1 attribute at offset 10.
  cred[0] = 0x01;
  cred[1] = 0x00;
  cred[2] = 0x0A;
  
  // Attribute: age:"19" at offset 10.
  memcpy(&cred[10], "age:\"19\"", 8);
  
  // ValidFrom/ValidUntil dates (offsets 84, 92).
  memcpy(&cred[84], "20241001", 8);
  memcpy(&cred[92], "20251001", 8);
  
  // Device public key (offsets 100, 132).
  memcpy(&cred[100], dpkx, 32);
  memcpy(&cred[132], dpky, 32);

  // Hash credential for issuer signature.
  uint8_t cred_hash[32];
  sha256(cred, sizeof(cred), cred_hash);

  uint8_t sigr[32], sigs[32];
  if (!ecdsa_sign_p256(issuer_key, cred_hash, sigr, sigs)) {
    fprintf(stderr, "Failed to sign credential\n");
    return 1;
  }

  // Build transcript and sign with device key.
  uint8_t transcript[32];
  // For simplicity, use a fixed test transcript.
  memcpy(transcript, "test_transcript_for_device_key!!", 32);

  uint8_t tr_hash[32];
  sha256(transcript, 32, tr_hash);

  uint8_t sigtr[32], sigts[32];
  if (!ecdsa_sign_p256(device_key, tr_hash, sigtr, sigts)) {
    fprintf(stderr, "Failed to sign transcript\n");
    return 1;
  }

  // Output test vectors in C++ format.
  printf("// Generated test vectors for ptrcred_age_over_18_test\n\n");
  
  print_hex("Issuer pkx", pkx, 32);
  print_hex("Issuer pky", pky, 32);
  print_hex("Credential signature r", sigr, 32);
  print_hex("Credential signature s", sigs, 32);
  print_hex("Device key signature r", sigtr, 32);
  print_hex("Device key signature s", sigts, 32);
  
  printf("\nTranscript: ");
  for (int i = 0; i < 32; ++i) {
    printf("0x%02x, ", transcript[i]);
  }
  printf("\n\nCredential (%zu bytes):\n", sizeof(cred));
  for (size_t i = 0; i < sizeof(cred); ++i) {
    if (i % 16 == 0) printf("  ");
    printf("0x%02x, ", cred[i]);
    if ((i + 1) % 16 == 0) printf("\n");
  }
  printf("\n");

  // Cleanup.
  BN_free(pkx_bn);
  BN_free(pky_bn);
  BN_free(dpkx_bn);
  BN_free(dpky_bn);
  EC_KEY_free(issuer_key);
  EC_KEY_free(device_key);

  return 0;
}
