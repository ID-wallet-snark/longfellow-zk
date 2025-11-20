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

#include "circuits/mdoc/mdoc_zk.h"

#include <stdio.h>
#include <sys/types.h>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>

#include "circuits/mdoc/mdoc_examples.h"
#include "circuits/mdoc/mdoc_test_attributes.h"
#include "util/log.h"
#include "gtest/gtest.h"

namespace proofs {
namespace {

// Test fixture for French License Verification
class FrenchLicenseTest : public testing::Test {
protected:
  FrenchLicenseTest() { set_log_level(INFO); }

  static void SetUpTestCase() {
    if (circuit_ == nullptr) {
      // We use the 2-attribute circuit spec (age + nationality/other)
      // This is generic enough to hold our "Validity" and "Type" checks.
      EXPECT_EQ(generate_circuit(&kZkSpecs[1], &circuit_, &circuit_len_),
                CIRCUIT_GENERATION_SUCCESS);
    }
  }

  static void TearDownTestCase() {
    if (circuit_ != nullptr) {
      free(circuit_);
      circuit_ = nullptr;
    }
  }

  static uint8_t *circuit_;
  static size_t circuit_len_;
};

uint8_t *FrenchLicenseTest::circuit_ = nullptr;
size_t FrenchLicenseTest::circuit_len_ = 0;

TEST_F(FrenchLicenseTest, VerifyFrenchLicense) {
  // ===========================================================================
  // SCENARIO: French Driver's License Verification
  // ===========================================================================
  // We want to prove:
  // 1. The license is VALID (not expired) -> Checked via 'issue_date' (and
  // implicitly validFrom/validUntil in the circuit)
  // 2. The license is of TYPE B (Car) -> Simulated by checking 'height' (175)
  //
  // In a real scenario, we would check 'driving_privileges' or
  // 'un_distinguishing_sign'. Since we are limited to existing signed data, we
  // use 'height' as a proxy for the "Type" attribute. The Zero-Knowledge
  // property ensures that we ONLY reveal that these attributes match, without
  // revealing the actual values if we didn't want to (though here we disclose
  // them).
  // ===========================================================================

  log(INFO, "============================================================");
  log(INFO, "    DEBUT DE LA VERIFICATION DU PERMIS DE CONDUIRE FRANCAIS");
  log(INFO, "============================================================");

  // 1. Setup the Claims (Attributs a verifier)
  constexpr int num_attrs = 2;
  const ZkSpecStruct &zk_spec = kZkSpecs[1];

  // We use mdoc_tests[3] which has 'issue_date' and 'height'.
  const struct MdocTests *test_data = &mdoc_tests[3];

  RequestedAttribute attributes[num_attrs] = {
      // Check 1: Validity (via Issue Date)
      // In a real app, we might just check the 'expiry_date' is in the future.
      test::issue_date_2024_03_15,

      // Check 2: License Type (Simulated via Height)
      // NOTE: We use 'height' (175) as a proxy for 'driving_privileges' (e.g.,
      // "B")
      // because we are using pre-signed mock data and cannot generate new
      // signatures
      // for custom attributes. The ZK mechanism (Selective Disclosure) is
      // identical.
      test::height_175};

  log(INFO, "[1] Configuration des criteres de verification:");
  log(INFO, "    - Critere 1: Date d'emission (Validite) -> 2024-03-15");
  log(INFO, "    - Critere 2: Type de Permis (Simule)    -> 175 (Code B)");

  // 2. Generate ZK Proof (Prover Side)
  // This happens on the user's device (Wallet).
  uint8_t *zkproof = nullptr;
  size_t proof_len = 0;

  log(INFO, "[2] Generation de la Preuve ZK (Cote Utilisateur)...");
  log(INFO, "    (Cela peut prendre quelques secondes pour les calculs "
            "cryptographiques)");

  MdocProverErrorCode prover_ret = run_mdoc_prover(
      circuit_, circuit_len_, test_data->mdoc, test_data->mdoc_size,
      test_data->pkx.as_pointer, test_data->pky.as_pointer,
      test_data->transcript, test_data->transcript_size, attributes, num_attrs,
      (const char *)test_data->now, &zkproof, &proof_len, &zk_spec);

  EXPECT_EQ(prover_ret, MDOC_PROVER_SUCCESS);

  if (prover_ret == MDOC_PROVER_SUCCESS) {
    log(INFO, "    Preuve generee avec succes! Taille: %zu octets", proof_len);
  } else {
    log(ERROR, "    Echec de la generation de preuve. Code: %d", prover_ret);
    return;
  }

  // 3. Verify ZK Proof (Verifier Side)
  // This happens on the verifier's device (Police / Rental Agency).
  log(INFO, "[3] Verification de la Preuve (Cote Verificateur)...");

  MdocVerifierErrorCode verifier_ret =
      run_mdoc_verifier(circuit_, circuit_len_, test_data->pkx.as_pointer,
                        test_data->pky.as_pointer, test_data->transcript,
                        test_data->transcript_size, attributes, num_attrs,
                        (const char *)test_data->now, zkproof, proof_len,
                        kDefaultDocType, &zk_spec);

  EXPECT_EQ(verifier_ret, MDOC_VERIFIER_SUCCESS);

  if (verifier_ret == MDOC_VERIFIER_SUCCESS) {
    log(INFO, "============================================================");
    log(INFO, " RESULTAT: PERMIS VALIDE ET TYPE CONFIRME");
    log(INFO, "   Le Zero-Knowledge Proof garantit que:");
    log(INFO, "   1. Le document est authentique (signe par l'autorite).");
    log(INFO, "   2. Les donnees n'ont pas ete alterees.");
    log(INFO, "   3. Les criteres (Date, Type) sont remplis.");
    log(INFO, "   Aucune autre donnee personnelle n'a ete exposee.");
    log(INFO, "============================================================");
  } else {
    log(ERROR, " RESULTAT: PERMIS INVALIDE. Code: %d", verifier_ret);
  }

  free(zkproof);
}

} // namespace
} // namespace proofs
