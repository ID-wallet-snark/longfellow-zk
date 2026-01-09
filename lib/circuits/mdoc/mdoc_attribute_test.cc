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

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "circuits/mdoc/mdoc_attribute_ids.h"
#include "circuits/mdoc/mdoc_witness.h"
#include "circuits/mdoc/mdoc_examples.h"
#include "circuits/mdoc/mdoc_test_attributes.h"
#include "circuits/mdoc/mdoc_zk.h"
#include "util/log.h"
#include "gtest/gtest.h"

namespace proofs {
namespace {

// This test validates that the cbor encoding of an attribute is NOT a suffix
// of any other valid attribute id.  Therefore, finding the location of the
// cbor-encoded value of the attribute name is sufficient. We can be sure that
// the Prover is not able to forge an attribute by pointing to the suffix of
// another attribute id.
TEST(MdocAttributeTest, MdocAttributeIdsAreSuffixFree) {
  for (const auto& attr : kMdocAttributes) {
    // Form the cbor encoding of attr.
    size_t len = attr.identifier.size();
    std::vector<uint8_t> attr_enc;
    append_text_len(attr_enc, len);
    attr_enc.insert(attr_enc.end(), attr.identifier.begin(),
                    attr.identifier.end());
    std::string attr_enc_str(attr_enc.begin(), attr_enc.end());
    for (const auto& aj : kMdocAttributes) {
      bool ends_with = false;
      if (aj.identifier.size() >= attr_enc_str.size()) {
        ends_with = (aj.identifier.compare(
                         aj.identifier.size() - attr_enc_str.size(),
                         attr_enc_str.size(), attr_enc_str) == 0);
      }

      if (ends_with && aj.identifier != attr.identifier) {
        log(INFO, "identifier %s is a suffix of %s\n", aj.identifier.data(),
            attr.identifier.data());
      }
      EXPECT_TRUE(!ends_with || aj.identifier == attr.identifier);
    }
  }
}

TEST(MdocAttributeTest, DelimiterIsPresent) {
  // Verify that the delimiter "ier" never occurs in an attribute id.
  // Verify that elementValue never appears in any attribute id.
  for (const auto& attr : kMdocAttributes) {
    EXPECT_TRUE(attr.identifier.find("ier") == std::string::npos);
    EXPECT_TRUE(attr.identifier.find("elementValue") == std::string::npos);
  }
}

class SmartAgeTest : public testing::Test {
protected:
  static void SetUpTestCase() {
    if (circuit_ == nullptr) {
      generate_circuit(&kZkSpecs[0], &circuit_, &circuit_len_);
    }
  }

  static void TearDownTestCase() {
    if (circuit_) {
      free(circuit_);
      circuit_ = nullptr;
    }
  }

  static uint8_t *circuit_;
  static size_t circuit_len_;
};

uint8_t *SmartAgeTest::circuit_ = nullptr;
size_t SmartAgeTest::circuit_len_ = 0;

// Test 1: Verify user born in 1998 is <= 2008
// User (1998) is older than (2008 threshold), so their birthdate is smaller.
// 1998 <= 2008: TRUE
TEST_F(SmartAgeTest, VerifyAgeUnderLimit) {
  // mdoc_tests[2] has birth_date 1968-04-27
  const MdocTests *test_mdoc = &mdoc_tests[3];

  // RequestedAttribute: birth_date <= 2008-01-01
  // (verification_type = 1)
  RequestedAttribute attrs[] = {test::proof_age_over_18_limit_2008};

  uint8_t *zkproof = nullptr;
  size_t proof_len = 0;

  // Prover
  MdocProverErrorCode ret = run_mdoc_prover(
      circuit_, circuit_len_, test_mdoc->mdoc, test_mdoc->mdoc_size,
      test_mdoc->pkx.as_pointer, test_mdoc->pky.as_pointer,
      test_mdoc->transcript, test_mdoc->transcript_size, attrs, 1,
      (const char *)test_mdoc->now, &zkproof, &proof_len, &kZkSpecs[0]);

  EXPECT_EQ(ret, MDOC_PROVER_SUCCESS);
  if (ret != MDOC_PROVER_SUCCESS)
    return;

  // Verifier
  MdocVerifierErrorCode v_ret = run_mdoc_verifier(
      circuit_, circuit_len_, test_mdoc->pkx.as_pointer,
      test_mdoc->pky.as_pointer, test_mdoc->transcript,
      test_mdoc->transcript_size, attrs, 1, (const char *)test_mdoc->now,
      zkproof, proof_len, test_mdoc->doc_type, &kZkSpecs[0]);
  EXPECT_EQ(v_ret, MDOC_VERIFIER_SUCCESS);
  free(zkproof);
}

// Test 2: Verify user born in 1968 is <= 2008 (Still true, very old)
TEST_F(SmartAgeTest, VerifyAgeUnderLimitOlder) {
  // mdoc_tests[2] has birth_date 1968-04-27
  const MdocTests *test_mdoc = &mdoc_tests[3];

  RequestedAttribute attrs[] = {test::proof_age_over_18_limit_2008};

  uint8_t *zkproof = nullptr;
  size_t proof_len = 0;

  MdocProverErrorCode ret = run_mdoc_prover(
      circuit_, circuit_len_, test_mdoc->mdoc, test_mdoc->mdoc_size,
      test_mdoc->pkx.as_pointer, test_mdoc->pky.as_pointer,
      test_mdoc->transcript, test_mdoc->transcript_size, attrs, 1,
      (const char *)test_mdoc->now, &zkproof, &proof_len, &kZkSpecs[0]);

  EXPECT_EQ(ret, MDOC_PROVER_SUCCESS);
  if (zkproof)
    free(zkproof);
}

// Test 3: GEQ test. User born in 1968. Limit 1958.
// 1968 >= 1958: TRUE.
// Note: test::proof_age_under_65_limit_1958 uses Type 2 (GEQ).
TEST_F(SmartAgeTest, VerifyAgeOlderThanLimit) {
  // mdoc_tests[2] has birth_date 1968-04-27
  const MdocTests *test_mdoc = &mdoc_tests[3];

  RequestedAttribute attrs[] = {test::proof_age_under_65_limit_1958};

  uint8_t *zkproof = nullptr;
  size_t proof_len = 0;

  MdocProverErrorCode ret = run_mdoc_prover(
      circuit_, circuit_len_, test_mdoc->mdoc, test_mdoc->mdoc_size,
      test_mdoc->pkx.as_pointer, test_mdoc->pky.as_pointer,
      test_mdoc->transcript, test_mdoc->transcript_size, attrs, 1,
      (const char *)test_mdoc->now, &zkproof, &proof_len, &kZkSpecs[0]);

  ASSERT_EQ(ret, MDOC_PROVER_SUCCESS);

  MdocVerifierErrorCode v_ret = run_mdoc_verifier(
      circuit_, circuit_len_, test_mdoc->pkx.as_pointer,
      test_mdoc->pky.as_pointer, test_mdoc->transcript,
      test_mdoc->transcript_size, attrs, 1, (const char *)test_mdoc->now,
      zkproof, proof_len, test_mdoc->doc_type, &kZkSpecs[0]);
  EXPECT_EQ(v_ret, MDOC_VERIFIER_SUCCESS);
  free(zkproof);
}

}  // namespace
}  // namespace proofs
