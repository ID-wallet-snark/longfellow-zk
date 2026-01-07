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

#include <benchmark/benchmark.h>
#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "mdoc_examples.h"
#include "mdoc_test_attributes.h"
#include "mdoc_zk.h"

namespace proofs {
namespace {

// Benchmark for Circuit Generation (age_over_18 - 1 attribute)
void BM_CircuitGeneration_AgeOver18(benchmark::State &state) {
  uint8_t *circuit = nullptr;
  size_t circuit_len = 0;

  for (auto _ : state) {
    if (circuit) {
      free(circuit);
      circuit = nullptr;
    }
    // kZkSpecs[0] is for 1 attribute
    CircuitGenerationErrorCode ret =
        generate_circuit(&kZkSpecs[0], &circuit, &circuit_len);
    if (ret != CIRCUIT_GENERATION_SUCCESS) {
      state.SkipWithError("Circuit generation failed");
      break;
    }
    benchmark::DoNotOptimize(circuit);
    benchmark::DoNotOptimize(circuit_len);
  }
  if (circuit)
    free(circuit);
}
BENCHMARK(BM_CircuitGeneration_AgeOver18);

// Benchmark for Prover (age_over_18)
void BM_Prover_AgeOver18(benchmark::State &state) {
  // Setup
  uint8_t *circuit = nullptr;
  size_t circuit_len = 0;
  if (generate_circuit(&kZkSpecs[0], &circuit, &circuit_len) !=
      CIRCUIT_GENERATION_SUCCESS) {
    state.SkipWithError("Circuit generation failed in setup");
    return;
  }

  // Use mdoc_tests[0] which contains age_over_18
  const MdocTests *test_data = &mdoc_tests[0];

  RequestedAttribute attrs[] = {test::age_over_18};
  uint8_t *zkproof = nullptr;
  size_t proof_len = 0;

  for (auto _ : state) {
    state.PauseTiming();
    if (zkproof) {
      free(zkproof);
      zkproof = nullptr;
    }
    state.ResumeTiming();

    MdocProverErrorCode ret = run_mdoc_prover(
        circuit, circuit_len, test_data->mdoc, test_data->mdoc_size,
        test_data->pkx.as_pointer, test_data->pky.as_pointer,
        test_data->transcript, test_data->transcript_size, attrs, 1,
        (const char *)test_data->now, &zkproof, &proof_len, &kZkSpecs[0]);

    if (ret != MDOC_PROVER_SUCCESS) {
      state.SkipWithError("Prover failed");
      break;
    }
    benchmark::DoNotOptimize(zkproof);
    benchmark::DoNotOptimize(proof_len);
  }

  if (zkproof)
    free(zkproof);
  if (circuit)
    free(circuit);
}
BENCHMARK(BM_Prover_AgeOver18);

// Benchmark for Verifier (age_over_18)
void BM_Verifier_AgeOver18(benchmark::State &state) {
  // Setup
  uint8_t *circuit = nullptr;
  size_t circuit_len = 0;
  if (generate_circuit(&kZkSpecs[0], &circuit, &circuit_len) !=
      CIRCUIT_GENERATION_SUCCESS) {
    state.SkipWithError("Circuit generation failed in setup");
    return;
  }

  const MdocTests *test_data = &mdoc_tests[0];

  RequestedAttribute attrs[] = {test::age_over_18};
  uint8_t *zkproof = nullptr;
  size_t proof_len = 0;

  // Generate one proof to verify
  MdocProverErrorCode ret = run_mdoc_prover(
      circuit, circuit_len, test_data->mdoc, test_data->mdoc_size,
      test_data->pkx.as_pointer, test_data->pky.as_pointer,
      test_data->transcript, test_data->transcript_size, attrs, 1,
      (const char *)test_data->now, &zkproof, &proof_len, &kZkSpecs[0]);

  if (ret != MDOC_PROVER_SUCCESS) {
    state.SkipWithError("Prover failed during setup");
    free(circuit);
    return;
  }

  for (auto _ : state) {
    MdocVerifierErrorCode v_ret = run_mdoc_verifier(
        circuit, circuit_len, test_data->pkx.as_pointer,
        test_data->pky.as_pointer, test_data->transcript,
        test_data->transcript_size, attrs, 1, (const char *)test_data->now,
        zkproof, proof_len, test_data->doc_type, &kZkSpecs[0]);

    if (v_ret != MDOC_VERIFIER_SUCCESS) {
      state.SkipWithError("Verifier failed");
      break;
    }
    benchmark::DoNotOptimize(v_ret);
  }

  if (zkproof)
    free(zkproof);
  if (circuit)
    free(circuit);
}
BENCHMARK(BM_Verifier_AgeOver18);

} // namespace
} // namespace proofs
