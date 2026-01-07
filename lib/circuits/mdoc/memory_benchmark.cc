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
#include <sys/resource.h>

#include "mdoc_examples.h"
#include "mdoc_test_attributes.h"
#include "mdoc_zk.h"

namespace proofs {
namespace {

// Get current RSS (Resident Set Size) in bytes
size_t get_current_rss() {
  struct rusage usage;
  getrusage(RUSAGE_SELF, &usage);
#ifdef __APPLE__
  return usage.ru_maxrss; // bytes on macOS
#else
  return usage.ru_maxrss * 1024; // KB on Linux, convert to bytes
#endif
}

// Benchmark memory usage for different phases
void BM_Memory_CircuitGeneration_1Attr(benchmark::State &state) {
  size_t rss_before = get_current_rss();

  uint8_t *circuit = nullptr;
  size_t circuit_len = 0;

  CircuitGenerationErrorCode ret =
      generate_circuit(&kZkSpecs[0], &circuit, &circuit_len);

  size_t rss_after = get_current_rss();

  if (ret != CIRCUIT_GENERATION_SUCCESS) {
    state.SkipWithError("Circuit generation failed");
    return;
  }

  for (auto _ : state) {
    // Dummy loop
  }

  state.counters["rss_before_mb"] =
      benchmark::Counter(rss_before / 1024.0 / 1024.0);
  state.counters["rss_after_mb"] =
      benchmark::Counter(rss_after / 1024.0 / 1024.0);
  state.counters["rss_delta_mb"] =
      benchmark::Counter((rss_after - rss_before) / 1024.0 / 1024.0);
  state.counters["circuit_size_mb"] =
      benchmark::Counter(circuit_len / 1024.0 / 1024.0);

  free(circuit);
}
BENCHMARK(BM_Memory_CircuitGeneration_1Attr)->Iterations(1);

void BM_Memory_Prover_1Attr(benchmark::State &state) {
  // Setup
  uint8_t *circuit = nullptr;
  size_t circuit_len = 0;
  generate_circuit(&kZkSpecs[0], &circuit, &circuit_len);

  const MdocTests *test_data = &mdoc_tests[3];
  RequestedAttribute attrs[] = {test::age_over_18};

  size_t rss_before = get_current_rss();

  uint8_t *zkproof = nullptr;
  size_t proof_len = 0;

  MdocProverErrorCode ret = run_mdoc_prover(
      circuit, circuit_len, test_data->mdoc, test_data->mdoc_size,
      test_data->pkx.as_pointer, test_data->pky.as_pointer,
      test_data->transcript, test_data->transcript_size, attrs, 1,
      (const char *)test_data->now, &zkproof, &proof_len, &kZkSpecs[0]);

  size_t rss_after = get_current_rss();

  if (ret != MDOC_PROVER_SUCCESS) {
    state.SkipWithError("Prover failed");
    free(circuit);
    return;
  }

  for (auto _ : state) {
    // Dummy loop
  }

  state.counters["rss_before_mb"] =
      benchmark::Counter(rss_before / 1024.0 / 1024.0);
  state.counters["rss_after_mb"] =
      benchmark::Counter(rss_after / 1024.0 / 1024.0);
  state.counters["rss_delta_mb"] =
      benchmark::Counter((rss_after - rss_before) / 1024.0 / 1024.0);
  state.counters["proof_size_mb"] =
      benchmark::Counter(proof_len / 1024.0 / 1024.0);

  free(circuit);
  free(zkproof);
}
BENCHMARK(BM_Memory_Prover_1Attr)->Iterations(1);

void BM_Memory_Verifier_1Attr(benchmark::State &state) {
  // Setup
  uint8_t *circuit = nullptr;
  size_t circuit_len = 0;
  generate_circuit(&kZkSpecs[0], &circuit, &circuit_len);

  const MdocTests *test_data = &mdoc_tests[3];
  RequestedAttribute attrs[] = {test::age_over_18};

  uint8_t *zkproof = nullptr;
  size_t proof_len = 0;
  run_mdoc_prover(circuit, circuit_len, test_data->mdoc, test_data->mdoc_size,
                  test_data->pkx.as_pointer, test_data->pky.as_pointer,
                  test_data->transcript, test_data->transcript_size, attrs, 1,
                  (const char *)test_data->now, &zkproof, &proof_len,
                  &kZkSpecs[0]);

  size_t rss_before = get_current_rss();

  MdocVerifierErrorCode ret = run_mdoc_verifier(
      circuit, circuit_len, test_data->pkx.as_pointer,
      test_data->pky.as_pointer, test_data->transcript,
      test_data->transcript_size, attrs, 1, (const char *)test_data->now,
      zkproof, proof_len, test_data->doc_type, &kZkSpecs[0]);

  size_t rss_after = get_current_rss();

  if (ret != MDOC_VERIFIER_SUCCESS) {
    state.SkipWithError("Verifier failed");
    free(circuit);
    free(zkproof);
    return;
  }

  for (auto _ : state) {
    // Dummy loop
  }

  state.counters["rss_before_mb"] =
      benchmark::Counter(rss_before / 1024.0 / 1024.0);
  state.counters["rss_after_mb"] =
      benchmark::Counter(rss_after / 1024.0 / 1024.0);
  state.counters["rss_delta_mb"] =
      benchmark::Counter((rss_after - rss_before) / 1024.0 / 1024.0);

  free(circuit);
  free(zkproof);
}
BENCHMARK(BM_Memory_Verifier_1Attr)->Iterations(1);

// Test with 4 attributes for comparison
void BM_Memory_Complete_4Attrs(benchmark::State &state) {
  size_t rss_start = get_current_rss();

  uint8_t *circuit = nullptr;
  size_t circuit_len = 0;
  generate_circuit(&kZkSpecs[3], &circuit, &circuit_len);

  size_t rss_after_circuit = get_current_rss();

  const MdocTests *test_data = &mdoc_tests[3];
  RequestedAttribute attrs[] = {test::age_over_18, test::height_175,
                                test::birthdate_1971_09_01,
                                test::familyname_mustermann};

  uint8_t *zkproof = nullptr;
  size_t proof_len = 0;
  run_mdoc_prover(circuit, circuit_len, test_data->mdoc, test_data->mdoc_size,
                  test_data->pkx.as_pointer, test_data->pky.as_pointer,
                  test_data->transcript, test_data->transcript_size, attrs, 4,
                  (const char *)test_data->now, &zkproof, &proof_len,
                  &kZkSpecs[3]);

  size_t rss_after_prover = get_current_rss();

  run_mdoc_verifier(circuit, circuit_len, test_data->pkx.as_pointer,
                    test_data->pky.as_pointer, test_data->transcript,
                    test_data->transcript_size, attrs, 4,
                    (const char *)test_data->now, zkproof, proof_len,
                    test_data->doc_type, &kZkSpecs[3]);

  size_t rss_final = get_current_rss();

  for (auto _ : state) {
    // Dummy loop
  }

  state.counters["rss_start_mb"] =
      benchmark::Counter(rss_start / 1024.0 / 1024.0);
  state.counters["rss_after_circuit_mb"] =
      benchmark::Counter(rss_after_circuit / 1024.0 / 1024.0);
  state.counters["rss_after_prover_mb"] =
      benchmark::Counter(rss_after_prover / 1024.0 / 1024.0);
  state.counters["rss_final_mb"] =
      benchmark::Counter(rss_final / 1024.0 / 1024.0);
  state.counters["peak_rss_mb"] =
      benchmark::Counter(rss_final / 1024.0 / 1024.0);
  state.counters["total_delta_mb"] =
      benchmark::Counter((rss_final - rss_start) / 1024.0 / 1024.0);

  free(circuit);
  free(zkproof);
}
BENCHMARK(BM_Memory_Complete_4Attrs)->Iterations(1);

} // namespace
} // namespace proofs
