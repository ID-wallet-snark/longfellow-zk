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

#include <iostream>
#include <cstring>
#include "circuits/mdoc/mdoc_zk.h"
#include "circuits/mdoc/mdoc_test_attributes.h"
#include "util/log.h"

int main() {
  std::cout << "=== Longfellow ZK Circuit Generation Test ===" << std::endl;
  
  // Set log level
  proofs::set_log_level(proofs::INFO);
  
  // Test 1: Check ZkSpecs
  std::cout << "\n[Test 1] Available ZkSpecs:" << std::endl;
  for (int i = 0; i < kNumZkSpecs; ++i) {
    std::cout << "  Spec #" << i << ": " 
              << kZkSpecs[i].num_attributes << " attributes"
              << std::endl;
  }
  
  // Test 2: Try to generate a circuit for 1 attribute
  std::cout << "\n[Test 2] Generate circuit for 1 attribute..." << std::endl;
  
  const ZkSpecStruct* zk_spec = nullptr;
  for (int i = 0; i < kNumZkSpecs; ++i) {
    if (kZkSpecs[i].num_attributes == 1) {
      zk_spec = &kZkSpecs[i];
      std::cout << "  Found ZkSpec #" << i << std::endl;
      break;
    }
  }
  
  if (!zk_spec) {
    std::cerr << "  ERROR: No ZkSpec found for 1 attribute" << std::endl;
    return 1;
  }
  
  uint8_t* circuit_data = nullptr;
  size_t circuit_len = 0;
  
  std::cout << "  Calling generate_circuit()..." << std::endl;
  CircuitGenerationErrorCode ret = generate_circuit(zk_spec, &circuit_data, &circuit_len);
  
  if (ret != CIRCUIT_GENERATION_SUCCESS) {
    std::cerr << "  ERROR: Circuit generation failed with code: " << ret << std::endl;
    return 1;
  }
  
  if (!circuit_data || circuit_len == 0) {
    std::cerr << "  ERROR: Circuit data is empty" << std::endl;
    return 1;
  }
  
  std::cout << "  SUCCESS: Circuit generated (" << circuit_len << " bytes)" << std::endl;
  
  // Cleanup
  free(circuit_data);
  
  // Test 3: Create a RequestedAttribute
  std::cout << "\n[Test 3] Create RequestedAttribute..." << std::endl;
  
  RequestedAttribute attr;
  memset(&attr, 0, sizeof(attr));
  
  // Namespace: org.iso.18013.5.1
  const char* ns = "org.iso.18013.5.1";
  size_t ns_len = strlen(ns);
  memcpy(attr.namespace_id, ns, ns_len);
  attr.namespace_len = ns_len;
  
  // ID: age_over_18
  const char* id = "age_over_18";
  memcpy(attr.id, id, strlen(id));
  attr.id_len = strlen(id);
  
  // CBOR value: true (0xf5)
  attr.cbor_value[0] = 0xf5;
  attr.cbor_value_len = 1;
  
  std::cout << "  Namespace: " << std::string(reinterpret_cast<const char*>(attr.namespace_id), attr.namespace_len) << std::endl;
  std::cout << "  ID: " << std::string(reinterpret_cast<const char*>(attr.id), attr.id_len) << std::endl;
  std::cout << "  CBOR value length: " << attr.cbor_value_len << std::endl;
  
  // Test 4: Use predefined test attributes
  std::cout << "\n[Test 4] Test predefined attributes..." << std::endl;
  std::cout << "  proofs::test::age_over_18 namespace: " 
            << std::string(reinterpret_cast<const char*>(proofs::test::age_over_18.namespace_id), 
                          proofs::test::age_over_18.namespace_len) 
            << std::endl;
  std::cout << "  proofs::test::age_over_18 id: " 
            << std::string(reinterpret_cast<const char*>(proofs::test::age_over_18.id), 
                          proofs::test::age_over_18.id_len) 
            << std::endl;
  
  std::cout << "\n=== All tests passed! ===" << std::endl;
  return 0;
}