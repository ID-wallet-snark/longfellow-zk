---
title: "Creating a New Circuit"
linkTitle: "Creating a New Circuit"
weight: 3
description: >
  A step-by-step guide to creating a new circuit in the Longfellow ZK library, including mdoc identity circuits.
---

## Introduction

This tutorial will guide you through the process of creating a new circuit in the Longfellow ZK library. We will cover the basic structure of a circuit, how to define its logic, and how to integrate it with the existing framework. We will also cover the specifics of creating mdoc identity circuits.

## 1. Directory Structure

First, create a new directory for your circuit within the `lib/circuits/` directory. For this tutorial, we will create a circuit named `my_circuit`.

```bash
mkdir lib/circuits/my_circuit
```

## 2. Header File

Create a header file for your circuit, `my_circuit.h`, in the new directory. This file will define the circuit's class and its public interface.

```cpp
// lib/circuits/my_circuit/my_circuit.h

#ifndef PRIVACY_PROOFS_ZK_LIB_CIRCUITS_MY_CIRCUIT_H_
#define PRIVACY_PROOFS_ZK_LIB_CIRCUITS_MY_CIRCUIT_H_

#include "circuits/logic/logic.h"

namespace proofs {

template <class Logic>
class MyCircuit {
 public:
  using v8 = typename Logic::v8;

  explicit MyCircuit(const Logic& l) : l_(l) {}

  // Define your circuit logic here.
  void MyFunction(const v8& input, const v8& output) const;

 private:
  const Logic& l_;
};

}  // namespace proofs

#endif  // PRIVACY_PROOFS_ZK_LIB_CIRCUITS_MY_CIRCUIT_H_
```

## 3. Implementation File

Create an implementation file, `my_circuit.cc`, to define the logic of your circuit.

```cpp
// lib/circuits/my_circuit/my_circuit.cc

#include "circuits/my_circuit/my_circuit.h"

namespace proofs {

template <class Logic>
void MyCircuit<Logic>::MyFunction(const v8& input, const v8& output) const {
  // Implement your circuit logic here.
  // For example, let's assert that the input and output are equal.
  l_.vassert_eq(&input, &output);
}

}  // namespace proofs
```

## 4. CMake Integration

To integrate your new circuit into the build system, you need to create a `CMakeLists.txt` file in your circuit's directory.

```cmake
# lib/circuits/my_circuit/CMakeLists.txt

add_library(my_circuit INTERFACE)

target_include_directories(my_circuit INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}
)
```

Then, add your new circuit's directory to the main `CMakeLists.txt` file in `lib/circuits/`.

```cmake
# lib/circuits/CMakeLists.txt

add_subdirectory(my_circuit)
```

## 5. Writing Tests

It is crucial to write tests for your new circuit. Create a test file, `my_circuit_test.cc`, in your circuit's directory.

```cpp
// lib/circuits/my_circuit/my_circuit_test.cc

#include "circuits/my_circuit/my_circuit.h"
#include "testing/logic_backend_test.h"

namespace proofs {

INSTANTIATE_TEST_SUITE_P(MyCircuitTest, LogicBackendTest,
                         testing::ValuesIn(LogicBackendTest::all_backends));

TEST_P(LogicBackendTest, MyCircuit) {
  MyCircuit<Logic> circuit(l_);
  auto input = l_.vbit<8>(0b10101010);
  auto output = l_.vbit<8>(0b10101010);
  circuit.MyFunction(input, output);
  EXPECT_TRUE(l_.check());
}

}  // namespace proofs
```

Update your `CMakeLists.txt` to include the test.

```cmake
# lib/circuits/my_circuit/CMakeLists.txt

add_library(my_circuit INTERFACE)

target_include_directories(my_circuit INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}
)

add_executable(my_circuit_test my_circuit_test.cc)
target_link_libraries(my_circuit_test testing)
add_test(NAME my_circuit_test COMMAND my_circuit_test)
```

## Creating mdoc Identity Circuits

The Longfellow ZK library provides a high-level API for creating and verifying circuits related to mdoc identity documents. The core of this functionality is exposed through the C-style functions in `lib/circuits/mdoc/mdoc_zk.h`.

### Key Concepts

- **`ZkSpecStruct`**: This structure defines a specific version of the ZK system, including the circuit hash, the number of attributes, and other parameters.
- **`RequestedAttribute`**: This structure allows a verifier to specify which attributes and values the prover must claim.
- **`generate_circuit`**: This function generates a compressed byte representation of a circuit for a given number of attributes. This circuit can then be used by the prover and verifier.
- **`run_mdoc_prover`**: This function takes the generated circuit, an mdoc, the issuer's public key, a transcript, and a set of requested attributes, and produces a ZK proof.
- **`run_mdoc_verifier`**: This function verifies a ZK proof against the generated circuit, the issuer's public key, the transcript, and the requested attributes.

### Example Usage

Here is a high-level example of how you might use these functions to create and verify a proof for an mdoc attribute.

```cpp
#include "circuits/mdoc/mdoc_zk.h"

// 1. Define the ZK specification.
const ZkSpecStruct* zk_spec = &kZkSpecs[0]; // Use the first available spec.

// 2. Generate the circuit.
uint8_t* circuit_bytes;
size_t circuit_len;
CircuitGenerationErrorCode gen_err = generate_circuit(zk_spec, &circuit_bytes, &circuit_len);

// 3. Define the requested attributes.
RequestedAttribute attrs[] = {
  {
    .namespace_id = (uint8_t*)"org.iso.18013.5.1",
    .id = (uint8_t*)"age_over_18",
    .cbor_value = (uint8_t*)"\xf5", // CBOR encoding for true
    .namespace_len = 20,
    .id_len = 11,
    .cbor_value_len = 1,
  }
};

// 4. Run the prover.
uint8_t* proof;
size_t proof_len;
MdocProverErrorCode prover_err = run_mdoc_prover(
  circuit_bytes, circuit_len,
  mdoc_bytes, mdoc_len,
  pkx, pky,
  transcript_bytes, transcript_len,
  attrs, 1,
  "2023-11-02T09:00:00Z",
  &proof, &proof_len, zk_spec
);

// 5. Run the verifier.
MdocVerifierErrorCode verifier_err = run_mdoc_verifier(
  circuit_bytes, circuit_len,
  pkx, pky,
  transcript_bytes, transcript_len,
  attrs, 1,
  "2023-11-02T09:00:00Z",
  proof, proof_len, kDefaultDocType, zk_spec
);
```

By using this high-level API, you can create and verify mdoc-based identity proofs without needing to implement the underlying circuit logic from scratch.
