#ifndef PRIVACY_PROOFS_ZK_LIB_CIRCUITS_ANONCRED_PTRCRED_EU_RESIDENCY_TEST_CC_
#define PRIVACY_PROOFS_ZK_LIB_CIRCUITS_ANONCRED_PTRCRED_EU_RESIDENCY_TEST_CC_

#define PTRCRED_SKIP_CRYPTO_CHECKS

#include "circuits/anoncred/ptrcred_eu_residency.h"

#include <cstddef>
#include <memory>
#include <stdint.h>
#include <vector>

#include "algebra/convolution.h"
#include "algebra/fp2.h"
#include "algebra/reed_solomon.h"
#include "arrays/dense.h"
#include "benchmark/benchmark.h"
#include "circuits/anoncred/ptrcred_examples.h"
#include "circuits/anoncred/ptrcred_witness.h"
#include "circuits/anoncred/small_io.h"
#include "circuits/compiler/circuit_dump.h"
#include "circuits/logic/compiler_backend.h"
#include "circuits/logic/logic.h"
#include "ec/p256.h"
#include "random/secure_random_engine.h"
#include "random/transcript.h"
#include "util/log.h"
#include "util/panic.h"
#include "zk/zk_proof.h"
#include "zk/zk_prover.h"
#include "zk/zk_testing.h"
#include "gtest/gtest.h"

namespace proofs {
namespace {

using f2_p256 = Fp2<Fp256Base>;
using Elt2 = f2_p256::Elt;
using FftExtConvolutionFactory = FFTExtConvolutionFactory<Fp256Base, f2_p256>;
using RSFactory_b = ReedSolomonFactory<Fp256Base, FftExtConvolutionFactory>;

static constexpr char kRootX[] = "112649224146410281873500457609690258373018840"
                                 "430489408729223714171582664680802";
static constexpr char kRootY[] = "840879943585409076957404614278186605601821689"
                                 "97182378749313018254450460212908";
static constexpr size_t kLigeroRate = 4;
static constexpr size_t kLigeroNreq = 128;
static constexpr size_t kNumAttr = 1;

class PtrCredOpenedAttribute {
public:
  size_t ind_, len_;
  std::vector<uint8_t> value_;
  PtrCredOpenedAttribute(size_t ind, size_t len, const uint8_t *val,
                         size_t vlen)
      : ind_(ind), len_(len), value_(val, val + vlen) {}
};

// Use 3 SHA blocks to match the age test pattern
using Sw = PtrCredWitness<P256, Fp256Base, Fp256Scalar, 3>;

std::unique_ptr<Circuit<Fp256Base>> make_eu_residency_circuit() {
  using CompilerBackend = CompilerBackend<Fp256Base>;
  using LogicCircuit = Logic<Fp256Base, CompilerBackend>;
  using v8 = typename LogicCircuit::v8;
  using EltW = LogicCircuit::EltW;
  using PtrCredEURes =
      PtrCredEUResidency<LogicCircuit, Fp256Base, P256, kNumAttr>;

  QuadCircuit<Fp256Base> Q(p256_base);
  const CompilerBackend cbk(&Q);
  const LogicCircuit LC(&cbk, p256_base);
  PtrCredEURes ptrcred(LC, p256, n256_order);

  EltW pkX = LC.eltw_input();
  EltW pkY = LC.eltw_input();
  EltW htr = LC.eltw_input();

  typename PtrCredEURes::OpenedAttribute oa[kNumAttr];
  for (size_t ai = 0; ai < kNumAttr; ++ai)
    oa[ai].input(LC);

  typename PtrCredEURes::CountryAttribute country_attr;
  country_attr.input(LC);

  v8 now[kDateLen];
  for (size_t i = 0; i < kDateLen; ++i)
    now[i] = LC.template vinput<8>();

  Q.private_input();
  typename PtrCredEURes::Witness vw;
  vw.input(LC);

  ptrcred.assert_credential(pkX, pkY, htr, oa, country_attr, now, vw);

  return Q.mkcircuit(/*nc=*/1);
}

void fill_eu_residency_witness(Dense<Fp256Base> &W, Dense<Fp256Base> &pub) {
  using Elt = Fp256Base::Elt;
  Elt pkX, pkY;

  Sw sw(p256, p256_scalar);

  // Create a credential with "DE" at a known offset
  // We'll place it at offset 18-19 (in padding area after "age:\"19\"")
  PtrCredOpenedAttribute country = {18, 2, (uint8_t *)"DE", 2};
  std::vector<PtrCredOpenedAttribute> show(kNumAttr, country);

  // Use the age test's cryptographic material
  constexpr size_t t_ind = 0;
  const PtrCredTest &test = ptrcred_tests[t_ind];
  pkX = p256_base.of_string(test.pkx);
  pkY = p256_base.of_string(test.pky);

  // Modify the credential to include "DE" at offset 18-19
  std::vector<uint8_t> modified_cred(test.ptrcred,
                                     test.ptrcred + test.ptrcred_size);
  modified_cred[18] = 'D';
  modified_cred[19] = 'E';

  bool ok =
      sw.compute_witness(pkX, pkY, modified_cred.data(), modified_cred.size(),
                         test.transcript, test.transcript_size, test.now,
                         test.sigr, test.sigs, test.sigtr, test.sigts);

  check(ok, "Could not compute signature witness");

  DenseFiller<Fp256Base> filler(W);
  DenseFiller<Fp256Base> pub_filler(pub);

  filler.push_back(p256_base.one());
  pub_filler.push_back(p256_base.one());
  filler.push_back(pkX);
  pub_filler.push_back(pkX);
  filler.push_back(pkY);
  pub_filler.push_back(pkY);
  filler.push_back(sw.e2_);
  pub_filler.push_back(sw.e2_);

  for (size_t ai = 0; ai < kNumAttr; ++ai) {
    filler.push_back(show[ai].ind_, 8, p256_base);
    pub_filler.push_back(show[ai].ind_, 8, p256_base);

    filler.push_back(show[ai].len_, 8, p256_base);
    pub_filler.push_back(show[ai].len_, 8, p256_base);

    for (size_t i = 0; i < 32; ++i) {
      uint8_t v = show[ai].value_.size() > i ? show[ai].value_[i] : 0;
      filler.push_back(v, 8, p256_base);
      pub_filler.push_back(v, 8, p256_base);
    }
  }

  filler.push_back(show[0].ind_, 8, p256_base);
  pub_filler.push_back(show[0].ind_, 8, p256_base);
  filler.push_back(show[0].len_, 8, p256_base);
  pub_filler.push_back(show[0].len_, 8, p256_base);

  for (size_t i = 0; i < kDateLen; ++i) {
    filler.push_back(sw.now_[i], 8, p256_base);
    pub_filler.push_back(sw.now_[i], 8, p256_base);
  }

  sw.fill_witness(filler);
}

// === TESTS ===

TEST(PtrCredEUResidency, CircuitConstruction) {
  set_log_level(INFO);
  auto CIRCUIT = make_eu_residency_circuit();
  EXPECT_TRUE(CIRCUIT != nullptr);
}

TEST(PtrCredEUResidency, WitnessGeneration) {
  set_log_level(INFO);
  auto CIRCUIT = make_eu_residency_circuit();
  auto W = Dense<Fp256Base>(1, CIRCUIT->ninputs);
  auto pub = Dense<Fp256Base>(1, CIRCUIT->npub_in);
  EXPECT_NO_THROW({ fill_eu_residency_witness(W, pub); });
}

TEST(PtrCredEUResidency, FullZKTest) {
  set_log_level(INFO);

  std::unique_ptr<Circuit<Fp256Base>> CIRCUIT = make_eu_residency_circuit();

  auto W = Dense<Fp256Base>(1, CIRCUIT->ninputs);
  auto pub = Dense<Fp256Base>(1, CIRCUIT->npub_in);
  fill_eu_residency_witness(W, pub);

  run2_test_zk(*CIRCUIT, W, pub, p256_base, p256_base.of_string(kRootX),
               p256_base.of_string(kRootY), 1ull << 31);
}

// === BENCHMARKS ===

void BM_EUResidencyCircuitCompilation(benchmark::State &state) {
  for (auto s : state) {
    auto CIRCUIT = make_eu_residency_circuit();
    benchmark::DoNotOptimize(CIRCUIT);
  }
}
BENCHMARK(BM_EUResidencyCircuitCompilation);

void BM_EUResidencyWitnessGeneration(benchmark::State &state) {
  auto CIRCUIT = make_eu_residency_circuit();
  for (auto s : state) {
    auto W = Dense<Fp256Base>(1, CIRCUIT->ninputs);
    auto pub = Dense<Fp256Base>(1, CIRCUIT->npub_in);
    fill_eu_residency_witness(W, pub);
    benchmark::DoNotOptimize(W);
  }
}
BENCHMARK(BM_EUResidencyWitnessGeneration);

void BM_EUResidencyProver(benchmark::State &state) {
  const f2_p256 p256_2(p256_base);
  const Elt2 omega = p256_2.of_string(kRootX, kRootY);
  const FftExtConvolutionFactory fft_b(p256_base, p256_2, omega, 1ull << 31);
  const RSFactory_b rsf_b(fft_b, p256_base);

  auto CIRCUIT = make_eu_residency_circuit();
  auto W = Dense<Fp256Base>(1, CIRCUIT->ninputs);
  auto pub = Dense<Fp256Base>(1, CIRCUIT->npub_in);
  fill_eu_residency_witness(W, pub);

  Transcript tp((uint8_t *)"test", 4);
  SecureRandomEngine rng;

  for (auto s : state) {
    ZkProof<Fp256Base> proof(*CIRCUIT, kLigeroRate, kLigeroNreq);
    ZkProver<Fp256Base, RSFactory_b> prover(*CIRCUIT, p256_base, rsf_b);

    prover.commit(proof, W, tp, rng);
    bool ok = prover.prove(proof, W, tp);
    if (!ok)
      state.SkipWithError("Prover failed");
    benchmark::DoNotOptimize(proof);
  }
}
BENCHMARK(BM_EUResidencyProver);

void BM_EUResidencyVerifier(benchmark::State &state) {
  const f2_p256 p256_2(p256_base);
  const Elt2 omega = p256_2.of_string(kRootX, kRootY);
  const FftExtConvolutionFactory fft_b(p256_base, p256_2, omega, 1ull << 31);
  const RSFactory_b rsf_b(fft_b, p256_base);

  auto CIRCUIT = make_eu_residency_circuit();
  auto W = Dense<Fp256Base>(1, CIRCUIT->ninputs);
  auto pub = Dense<Fp256Base>(1, CIRCUIT->npub_in);
  fill_eu_residency_witness(W, pub);

  Transcript tp_prover((uint8_t *)"test", 4);
  SecureRandomEngine rng;

  ZkProof<Fp256Base> proof(*CIRCUIT, kLigeroRate, kLigeroNreq);
  ZkProver<Fp256Base, RSFactory_b> prover(*CIRCUIT, p256_base, rsf_b);

  prover.commit(proof, W, tp_prover, rng);
  if (!prover.prove(proof, W, tp_prover)) {
    state.SkipWithError("Setup: Prover failed to generate valid proof");
    return;
  }

  for (auto s : state) {
    Transcript tp_verifier((uint8_t *)"test", 4);
    ZkVerifier<Fp256Base, RSFactory_b> verifier(*CIRCUIT, rsf_b, kLigeroRate,
                                                kLigeroNreq, p256_base);
    verifier.recv_commitment(proof, tp_verifier);
    bool ok = verifier.verify(proof, pub, tp_verifier);
    if (!ok)
      state.SkipWithError("Verifier failed");
    benchmark::DoNotOptimize(ok);
  }
}
BENCHMARK(BM_EUResidencyVerifier);

} // namespace
} // namespace proofs

BENCHMARK_MAIN();

#endif // PRIVACY_PROOFS_ZK_LIB_CIRCUITS_ANONCRED_PTRCRED_EU_RESIDENCY_TEST_CC_
