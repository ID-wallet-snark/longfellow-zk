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

#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include "mdoc_examples.h"
#include "mdoc_test_attributes.h"
#include "mdoc_zk.h"

using namespace proofs;

int main(int argc, char **argv) {
  printf("DEBUG: Starting Student Verification Test...\n");
  uint8_t *zkproof = nullptr;

  // 1. Setup Circuit (Generic ZK Circuit for 1 attribute)
  uint8_t *circuit = nullptr;
  size_t circuit_len = 0;
  // kZkSpecs[0] is for 1 attribute
  if (generate_circuit(&kZkSpecs[0], &circuit, &circuit_len) !=
      CIRCUIT_GENERATION_SUCCESS) {
    std::cerr << "ERROR: Circuit generation failed." << std::endl;
    return 1;
  }
  std::cout << "Circuit generated. Size: " << circuit_len << std::endl;

  // 2. Find Student mDoc Data in Examples
  const MdocTests *test_data = nullptr;
  for (size_t i = 0; i < kMdocTestsCount; ++i) {
    const auto &t = mdoc_tests[i];
    if (std::string(t.doc_type) == "fr.gouv.education.1.student") {
      test_data = &t;
      break;
    }
  }

  if (!test_data) {
    std::cerr << "ERROR: Student mDoc data not found in mdoc_examples.h"
              << std::endl;
    return 1;
  }
  std::cout << "Found Student mDoc data." << std::endl;

  // 3. Define Claim: "is_student" == true
  RequestedAttribute attrs[] = {test::is_student};
  size_t proof_len = 0;

  // 4. Run Prover (Holder side)
  // This generates a Zero-Knowledge Proof that the mDoc is valid and contains
  // the attribute.
  std::cout << "Running Prover..." << std::endl;
  MdocProverErrorCode ret = run_mdoc_prover(
      circuit, circuit_len, test_data->mdoc, test_data->mdoc_size,
      test_data->pkx.as_pointer, test_data->pky.as_pointer,
      test_data->transcript, test_data->transcript_size, attrs, 1,
      (const char *)test_data->now, &zkproof, &proof_len, &kZkSpecs[0]);

  if (ret != MDOC_PROVER_SUCCESS) {
    std::cerr << "ERROR: Prover failed with error code: " << ret << std::endl;
    return 1;
  }
  std::cout << "Proof generated successfully. Size: " << proof_len << " bytes."
            << std::endl;

  // 5. Run Verifier (Verifier side)
  // This verifies the ZK Proof without seeing the mDoc content.
  std::cout << "Running Verifier..." << std::endl;
  MdocVerifierErrorCode v_ret = run_mdoc_verifier(
      circuit, circuit_len, test_data->pkx.as_pointer,
      test_data->pky.as_pointer, test_data->transcript,
      test_data->transcript_size, attrs, 1, (const char *)test_data->now,
      zkproof, proof_len, test_data->doc_type, &kZkSpecs[0]);

  if (v_ret != MDOC_VERIFIER_SUCCESS) {
    std::cerr << "ERROR: Verifier failed with error code: " << v_ret
              << std::endl;
    return 1;
  }

  std::cout << "VERIFICATION SUCCESS!" << std::endl;
  std::cout << "The Student Status has been crypto-graphically verified."
            << std::endl;

  // Cleanup
  if (zkproof)
    free(zkproof);
  if (circuit)
    free(circuit);

  return 0;
}
