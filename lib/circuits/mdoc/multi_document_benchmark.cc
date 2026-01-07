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
#include <chrono>
#include <gtest/gtest.h>

#include "mdoc_examples.h"
#include "mdoc_test_attributes.h"
#include "mdoc_zk.h"

namespace proofs {
namespace {

// Helper to benchmark a specific document
void BenchmarkDocument(benchmark::State &state, const MdocTests *test_data,
                       const char *doc_name, const RequestedAttribute *attr) {
  uint8_t *circuit = nullptr;
  size_t circuit_len = 0;
  uint8_t *zkproof = nullptr;
  size_t proof_len = 0;

  // Circuit Generation
  auto circuit_start = std::chrono::high_resolution_clock::now();
  CircuitGenerationErrorCode circuit_ret =
      generate_circuit(&kZkSpecs[0], &circuit, &circuit_len);
  auto circuit_end = std::chrono::high_resolution_clock::now();

  if (circuit_ret != CIRCUIT_GENERATION_SUCCESS) {
    state.SkipWithError("Circuit generation failed");
    return;
  }

  auto circuit_duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
      circuit_end - circuit_start);

  // Document Parsing (implicit in prover)
  auto parse_start = std::chrono::high_resolution_clock::now();

  // Proof Generation
  auto prover_start = std::chrono::high_resolution_clock::now();
  MdocProverErrorCode prover_ret = run_mdoc_prover(
      circuit, circuit_len, test_data->mdoc, test_data->mdoc_size,
      test_data->pkx.as_pointer, test_data->pky.as_pointer,
      test_data->transcript, test_data->transcript_size, attr, 1,
      (const char *)test_data->now, &zkproof, &proof_len, &kZkSpecs[0]);
  auto prover_end = std::chrono::high_resolution_clock::now();

  if (prover_ret != MDOC_PROVER_SUCCESS) {
    state.SkipWithError("Prover failed");
    free(circuit);
    return;
  }

  auto prover_duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
      prover_end - prover_start);

  // Verification
  auto verifier_start = std::chrono::high_resolution_clock::now();
  MdocVerifierErrorCode verifier_ret = run_mdoc_verifier(
      circuit, circuit_len, test_data->pkx.as_pointer,
      test_data->pky.as_pointer, test_data->transcript,
      test_data->transcript_size, attr, 1, (const char *)test_data->now,
      zkproof, proof_len, test_data->doc_type, &kZkSpecs[0]);
  auto verifier_end = std::chrono::high_resolution_clock::now();

  if (verifier_ret != MDOC_VERIFIER_SUCCESS) {
    state.SkipWithError("Verifier failed");
    free(circuit);
    free(zkproof);
    return;
  }

  auto verifier_duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
      verifier_end - verifier_start);

  // Report metrics
  for (auto _ : state) {
    // Dummy loop
  }

  // Counters
  state.counters["circuit_gen_ns"] =
      benchmark::Counter(circuit_duration.count());
  state.counters["prover_ns"] = benchmark::Counter(prover_duration.count());
  state.counters["verifier_ns"] = benchmark::Counter(verifier_duration.count());
  state.counters["total_ns"] =
      benchmark::Counter(circuit_duration.count() + prover_duration.count() +
                         verifier_duration.count());
  state.counters["mdoc_size_bytes"] = benchmark::Counter(test_data->mdoc_size);
  state.counters["proof_size_bytes"] = benchmark::Counter(proof_len);

  free(circuit);
  free(zkproof);
}

// Benchmark: Playground Document (mdoc_tests[2])
void BM_Document_Playground(benchmark::State &state) {
  const MdocTests *test_data = &mdoc_tests[2];
  RequestedAttribute attr = test::age_over_18;
  BenchmarkDocument(state, test_data, "Playground", &attr);
}
BENCHMARK(BM_Document_Playground)->Unit(benchmark::kMillisecond)->Iterations(1);

// Benchmark: Sprind-Funke Document (mdoc_tests[3])
void BM_Document_SprindFunke(benchmark::State &state) {
  const MdocTests *test_data = &mdoc_tests[3];
  RequestedAttribute attr = test::age_over_18;
  BenchmarkDocument(state, test_data, "SprindFunke", &attr);
}
BENCHMARK(BM_Document_SprindFunke)
    ->Unit(benchmark::kMillisecond)
    ->Iterations(1);

// Benchmark: Student Document (mdoc_tests[0])
void BM_Document_Student(benchmark::State &state) {
  const MdocTests *test_data = &mdoc_tests[0];
  RequestedAttribute attr = test::age_over_18;
  BenchmarkDocument(state, test_data, "Student", &attr);
}
BENCHMARK(BM_Document_Student)->Unit(benchmark::kMillisecond)->Iterations(1);

} // namespace
} // namespace proofs
