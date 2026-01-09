#include "circuits/mdoc/mdoc_attribute_ids.h"
#include "circuits/mdoc/mdoc_test_attributes.h"
#include "circuits/mdoc/mdoc_zk.h"
#include "circuits/mdoc/mdoc_examples.h"
#include <benchmark/benchmark.h>
#include <string>

namespace proofs {
namespace {

static uint8_t* g_circuit = nullptr;
static size_t g_circuit_len = 0;

static void EnsureCircuit() {
    if (g_circuit == nullptr) {
         generate_circuit(&kZkSpecs[0], &g_circuit, &g_circuit_len);
    }
}

static void BM_StudentCard_CreateProof(benchmark::State& state) {
    EnsureCircuit();
    
    const MdocTests* test_mdoc = &mdoc_tests[3]; 
    RequestedAttribute attrs[] = { test::age_over_18 };
    
    for (auto _ : state) {
        uint8_t* zkproof = nullptr;
        size_t proof_len = 0;
        
        MdocProverErrorCode ret = run_mdoc_prover(
            g_circuit, g_circuit_len, 
            test_mdoc->mdoc, test_mdoc->mdoc_size,
            test_mdoc->pkx.as_pointer, test_mdoc->pky.as_pointer,
            test_mdoc->transcript, test_mdoc->transcript_size,
            attrs, 1, 
            (const char*)test_mdoc->now,
            &zkproof, &proof_len, &kZkSpecs[0]
        );
        
        if (ret != MDOC_PROVER_SUCCESS) {
            state.SkipWithError("Prover failed");
        }
        
        free(zkproof); 
    }
}
BENCHMARK(BM_StudentCard_CreateProof);

static void BM_StudentCard_VerifyProof(benchmark::State& state) {
    EnsureCircuit();
    
    const MdocTests* test_mdoc = &mdoc_tests[3]; 
    RequestedAttribute attrs[] = { test::age_over_18 };
    
    uint8_t* zkproof = nullptr;
    size_t proof_len = 0;
    MdocProverErrorCode ret = run_mdoc_prover(
        g_circuit, g_circuit_len, 
        test_mdoc->mdoc, test_mdoc->mdoc_size,
        test_mdoc->pkx.as_pointer, test_mdoc->pky.as_pointer,
        test_mdoc->transcript, test_mdoc->transcript_size,
        attrs, 1, 
        (const char*)test_mdoc->now,
        &zkproof, &proof_len, &kZkSpecs[0]
    );
     if (ret != MDOC_PROVER_SUCCESS) {
        state.SkipWithError("Setup prover failed");
        return;
    }

    for (auto _ : state) {
        MdocVerifierErrorCode v_ret = run_mdoc_verifier(
            g_circuit, g_circuit_len,
            test_mdoc->pkx.as_pointer, test_mdoc->pky.as_pointer,
            test_mdoc->transcript, test_mdoc->transcript_size,
            attrs, 1,
            (const char*)test_mdoc->now,
            zkproof, proof_len, test_mdoc->doc_type, &kZkSpecs[0]
        );
        
        if (v_ret != MDOC_VERIFIER_SUCCESS) {
            state.SkipWithError("Verification failed");
        }
    }
    
    free(zkproof);
}
BENCHMARK(BM_StudentCard_VerifyProof);

}  // namespace
}  // namespace proofs

BENCHMARK_MAIN();
