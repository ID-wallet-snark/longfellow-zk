---
title: "Creating a New Circuit"
linkTitle: "Creating a New Circuit"
weight: 3
description: >
  A step-by-step guide to creating a new circuit in the Longfellow ZK library.
---

## Introduction

This tutorial will guide you through the process of creating a new circuit in the Longfellow ZK library. We will cover the basic structure of a circuit, how to define its logic, and how to integrate it with the existing framework.

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

By following these steps, you can create and integrate a new circuit into the Longfellow ZK library.
