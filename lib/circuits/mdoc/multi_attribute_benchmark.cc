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

// Helper function to run a complete benchmark for N attributes
void RunScalabilityBenchmark(benchmark::State &state, int num_attrs,
                             const ZkSpecStruct *zk_spec,
                             const MdocTests *test_data,
                             const RequestedAttribute *attrs) {
  uint8_t *circuit = nullptr;
  size_t circuit_len = 0;
  uint8_t *zkproof = nullptr;
  size_t proof_len = 0;

  // Circuit Generation
  auto circuit_start = std::chrono::high_resolution_clock::now();
  CircuitGenerationErrorCode circuit_ret =
      generate_circuit(zk_spec, &circuit, &circuit_len);
  auto circuit_end = std::chrono::high_resolution_clock::now();

  if (circuit_ret != CIRCUIT_GENERATION_SUCCESS) {
    state.SkipWithError("Circuit generation failed");
    return;
  }

  auto circuit_duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
      circuit_end - circuit_start);

  // Proof Generation
  auto prover_start = std::chrono::high_resolution_clock::now();
  MdocProverErrorCode prover_ret = run_mdoc_prover(
      circuit, circuit_len, test_data->mdoc, test_data->mdoc_size,
      test_data->pkx.as_pointer, test_data->pky.as_pointer,
      test_data->transcript, test_data->transcript_size, attrs, num_attrs,
      (const char *)test_data->now, &zkproof, &proof_len, zk_spec);
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
  MdocVerifierErrorCode verifier_ret =
      run_mdoc_verifier(circuit, circuit_len, test_data->pkx.as_pointer,
                        test_data->pky.as_pointer, test_data->transcript,
                        test_data->transcript_size, attrs, num_attrs,
                        (const char *)test_data->now, zkproof, proof_len,
                        test_data->doc_type, zk_spec);
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
    // Dummy loop for benchmark framework
  }

  // Custom counters for detailed analysis
  state.counters["num_attributes"] =
      benchmark::Counter(num_attrs, benchmark::Counter::kDefaults);
  state.counters["circuit_size_bytes"] =
      benchmark::Counter(circuit_len, benchmark::Counter::kDefaults);
  state.counters["proof_size_bytes"] =
      benchmark::Counter(proof_len, benchmark::Counter::kDefaults);
  state.counters["circuit_gen_ns"] = benchmark::Counter(
      circuit_duration.count(), benchmark::Counter::kDefaults);
  state.counters["prover_ns"] = benchmark::Counter(
      prover_duration.count(), benchmark::Counter::kDefaults);
  state.counters["verifier_ns"] = benchmark::Counter(
      verifier_duration.count(), benchmark::Counter::kDefaults);
  state.counters["total_ns"] =
      benchmark::Counter(circuit_duration.count() + prover_duration.count() +
                             verifier_duration.count(),
                         benchmark::Counter::kDefaults);
  state.counters["proof_bytes_per_attr"] =
      benchmark::Counter(static_cast<double>(proof_len) / num_attrs,
                         benchmark::Counter::kAvgThreads);
  state.counters["prover_verifier_ratio"] = benchmark::Counter(
      static_cast<double>(prover_duration.count()) / verifier_duration.count(),
      benchmark::Counter::kAvgThreads);

  free(circuit);
  free(zkproof);
}

// Benchmark: 1 Attribute
void BM_Scalability_1Attr(benchmark::State &state) {
  const MdocTests *test_data = &mdoc_tests[3]; // Sprind-Funke
  RequestedAttribute attrs[] = {test::age_over_18};

  RunScalabilityBenchmark(state, 1, &kZkSpecs[0], test_data, attrs);
}
BENCHMARK(BM_Scalability_1Attr)->Unit(benchmark::kMillisecond)->Iterations(1);

// Benchmark: 2 Attributes
void BM_Scalability_2Attrs(benchmark::State &state) {
  const MdocTests *test_data = &mdoc_tests[3];
  RequestedAttribute attrs[] = {test::age_over_18, test::height_175};

  RunScalabilityBenchmark(state, 2, &kZkSpecs[1], test_data, attrs);
}
BENCHMARK(BM_Scalability_2Attrs)->Unit(benchmark::kMillisecond)->Iterations(1);

// Benchmark: 3 Attributes
void BM_Scalability_3Attrs(benchmark::State &state) {
  const MdocTests *test_data = &mdoc_tests[3];
  RequestedAttribute attrs[] = {test::age_over_18, test::height_175,
                                test::birthdate_1971_09_01};

  RunScalabilityBenchmark(state, 3, &kZkSpecs[2], test_data, attrs);
}
BENCHMARK(BM_Scalability_3Attrs)->Unit(benchmark::kMillisecond)->Iterations(1);

// Benchmark: 4 Attributes
void BM_Scalability_4Attrs(benchmark::State &state) {
  const MdocTests *test_data = &mdoc_tests[3];
  RequestedAttribute attrs[] = {test::age_over_18, test::height_175,
                                test::birthdate_1971_09_01,
                                test::familyname_mustermann};

  RunScalabilityBenchmark(state, 4, &kZkSpecs[3], test_data, attrs);
}
BENCHMARK(BM_Scalability_4Attrs)->Unit(benchmark::kMillisecond)->Iterations(1);

} // namespace
} // namespace proofs
