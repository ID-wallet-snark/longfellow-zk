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

#ifndef PRIVACY_PROOFS_ZK_LIB_CIRCUITS_MDOC_SEX_TEST_ATTRIBUTES_H_
#define PRIVACY_PROOFS_ZK_LIB_CIRCUITS_MDOC_SEX_TEST_ATTRIBUTES_H_

#include "circuits/mdoc/mdoc_zk.h"

namespace proofs {
namespace test {

// Definition of sex attribute for Male
// Follows ISO 18013-5.1 standard where sex is represented as "M" or "F"
static const RequestedAttribute sex_male = {
    .namespace_id = {'o', 'r', 'g', '.', 'i', 's', 'o', '.', '1', '8', '0', '1',
                     '3', '.', '5', '.', '1'},
    .id = {'s', 'e', 'x'},
    // CBOR text string "M": 0x61 (text of length 1) + 'M'
    .cbor_value = {0x61, 'M'},
    .namespace_len = 17,
    .id_len = 3,
    .cbor_value_len = 2
};

// Definition of sex attribute for Female
static const RequestedAttribute sex_female = {
    .namespace_id = {'o', 'r', 'g', '.', 'i', 's', 'o', '.', '1', '8', '0', '1',
                     '3', '.', '5', '.', '1'},
    .id = {'s', 'e', 'x'},
    // CBOR text string "F": 0x61 (text of length 1) + 'F'
    .cbor_value = {0x61, 'F'},
    .namespace_len = 17,
    .id_len = 3,
    .cbor_value_len = 2
};

}  // namespace test
}  // namespace proofs

#endif  // PRIVACY_PROOFS_ZK_LIB_CIRCUITS_MDOC_SEX_TEST_ATTRIBUTES_H_
