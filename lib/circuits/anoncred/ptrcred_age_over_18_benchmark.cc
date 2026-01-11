#include "circuits/anoncred/ptrcred_age_over_18.h"

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
#include "circuits/compiler/compiler.h"
#include "circuits/logic/compiler_backend.h"
#include "circuits/logic/logic.h"
#include "ec/p256.h"
#include "random/secure_random_engine.h"
#include "random/transcript.h"
#include "sumcheck/circuit.h"
#include "util/log.h"
#include "util/panic.h"
#include "zk/zk_proof.h"
#include "zk/zk_prover.h"
#include "zk/zk_testing.h"
#include "zk/zk_verifier.h"

namespace proofs {
namespace {

class PtrCredOpenedAttribute {
public:
  size_t ind_, len_;
  std::vector<uint8_t> value_;
  PtrCredOpenedAttribute(size_t ind, size_t len, const uint8_t *val,
                         size_t vlen)
      : ind_(ind), len_(len), value_(val, val + vlen) {}
};

using Sw = PtrCredWitness<P256, Fp256Base, Fp256Scalar>;
static constexpr size_t kNumAttr = 1;

// Define macro to skip crypto checks as done in the test for fair comparison of
// logic
#define PTRCRED_SKIP_CRYPTO_CHECKS

std::unique_ptr<Circuit<Fp256Base>> make_circuit() {
  using CompilerBackend = CompilerBackend<Fp256Base>;
  using LogicCircuit = Logic<Fp256Base, CompilerBackend>;
  using v8 = typename LogicCircuit::v8;
  using EltW = LogicCircuit::EltW;
  using PtrCredAgeOver18 =
      PtrCredAgeOver18<LogicCircuit, Fp256Base, P256, kNumAttr>;
  QuadCircuit<Fp256Base> Q(p256_base);
  const CompilerBackend cbk(&Q);
  const LogicCircuit LC(&cbk, p256_base);
  PtrCredAgeOver18 ptrcred(LC, p256, n256_order);

  EltW pkX = LC.eltw_input(), pkY = LC.eltw_input(), htr = LC.eltw_input();
  typename PtrCredAgeOver18::OpenedAttribute oa[kNumAttr];
  for (size_t ai = 0; ai < kNumAttr; ++ai) {
    oa[ai].input(LC);
  }

  typename PtrCredAgeOver18::AgeAttribute age_attr;
  age_attr.input(LC);

  v8 now[kDateLen];
  for (size_t i = 0; i < kDateLen; ++i) {
    now[i] = LC.template vinput<8>();
  }

  Q.private_input();

  typename PtrCredAgeOver18::Witness vwc;
  vwc.input(LC);

  ptrcred.assert_credential(pkX, pkY, htr, oa, age_attr, now, vwc);

  return Q.mkcircuit(/*nc=*/1);
}

void fill_witness(Dense<Fp256Base> &W, Dense<Fp256Base> &pub) {
  using Elt = Fp256Base::Elt;
  Elt pkX, pkY;

  Sw sw(p256, p256_scalar);
  PtrCredOpenedAttribute age = {15, 2, (uint8_t *)"19", 2};
  std::vector<PtrCredOpenedAttribute> show(kNumAttr, age);

  {
    constexpr size_t t_ind = 0;
    const PtrCredTest &test = ptrcred_tests[t_ind];
    pkX = p256_base.of_string(test.pkx);
    pkY = p256_base.of_string(test.pky);
    bool ok =
        sw.compute_witness(pkX, pkY, test.ptrcred, test.ptrcred_size,
                           test.transcript, test.transcript_size, test.now,
                           test.sigr, test.sigs, test.sigtr, test.sigts);
    check(ok, "Could not compute signature witness");
  }

  {
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

    // AgeAttribute pointing to digits only: offset 15 ('1'), 16 ('9')
    filler.push_back(15, 8, p256_base); // Index of first digit
    pub_filler.push_back(15, 8, p256_base);
    filler.push_back(2, 8, p256_base); // Length of digits
    pub_filler.push_back(2, 8, p256_base);

    for (size_t i = 0; i < kDateLen; ++i) {
      filler.push_back(sw.now_[i], 8, p256_base);
      pub_filler.push_back(sw.now_[i], 8, p256_base);
    }

    sw.fill_witness(filler);
  }
}

void BM_AnonCred_Prover(benchmark::State &state) {
  std::unique_ptr<Circuit<Fp256Base>> CIRCUIT = make_circuit();

  auto W = Dense<Fp256Base>(1, CIRCUIT->ninputs);
  auto pub = Dense<Fp256Base>(1, CIRCUIT->npub_in);

  fill_witness(W, pub);

  using f2_p256 = Fp2<Fp256Base>;
  using Elt2 = f2_p256::Elt;
  using FftExtConvolutionFactory = FFTExtConvolutionFactory<Fp256Base, f2_p256>;
  using RSFactory = ReedSolomonFactory<Fp256Base, FftExtConvolutionFactory>;

  static constexpr char kRootX[] =
      "112649224146410281873500457609690258373018840430489408729223714171582664"
      "680802";
  static constexpr char kRootY[] =
      "840879943585409076957404614278186605601821689971823787493130182544504602"
      "12908";

  const f2_p256 p256_2(p256_base);
  const Elt2 omega = p256_2.of_string(kRootX, kRootY);
  const FftExtConvolutionFactory fft_b(p256_base, p256_2, omega, 1ull << 31);
  const RSFactory rsf(fft_b, p256_base);

  Transcript tp((uint8_t *)"test", 4);
  SecureRandomEngine rng;

  for (auto s : state) {
    ZkProof<Fp256Base> zkpr(*CIRCUIT, 4, 128);
    ZkProver<Fp256Base, RSFactory> prover(*CIRCUIT, p256_base, rsf);
    prover.commit(zkpr, W, tp, rng);
    prover.prove(zkpr, W, tp);
  }
}
BENCHMARK(BM_AnonCred_Prover);

void BM_AnonCred_Verifier(benchmark::State &state) {
  std::unique_ptr<Circuit<Fp256Base>> CIRCUIT = make_circuit();

  auto W = Dense<Fp256Base>(1, CIRCUIT->ninputs);
  auto pub = Dense<Fp256Base>(1, CIRCUIT->npub_in);

  fill_witness(W, pub);

  using f2_p256 = Fp2<Fp256Base>;
  using Elt2 = f2_p256::Elt;
  using FftExtConvolutionFactory = FFTExtConvolutionFactory<Fp256Base, f2_p256>;
  using RSFactory = ReedSolomonFactory<Fp256Base, FftExtConvolutionFactory>;

  static constexpr char kRootX[] =
      "112649224146410281873500457609690258373018840430489408729223714171582664"
      "680802";
  static constexpr char kRootY[] =
      "840879943585409076957404614278186605601821689971823787493130182544504602"
      "12908";

  const f2_p256 p256_2(p256_base);
  const Elt2 omega = p256_2.of_string(kRootX, kRootY);
  const FftExtConvolutionFactory fft_b(p256_base, p256_2, omega, 1ull << 31);
  const RSFactory rsf(fft_b, p256_base);

  Transcript tp((uint8_t *)"test", 4);
  SecureRandomEngine rng;
  ZkProof<Fp256Base> zkpr(*CIRCUIT, 4, 128);
  ZkProver<Fp256Base, RSFactory> prover(*CIRCUIT, p256_base, rsf);
  prover.commit(zkpr, W, tp, rng);
  prover.prove(zkpr, W, tp);

  for (auto s : state) {
    Transcript tp_ver((uint8_t *)"test", 4);
    ZkVerifier<Fp256Base, RSFactory> verifier(*CIRCUIT, rsf, 4, 128, p256_base);
    verifier.recv_commitment(zkpr, tp_ver);
    bool ok = verifier.verify(zkpr, pub, tp_ver);
    benchmark::DoNotOptimize(ok);
  }
}
BENCHMARK(BM_AnonCred_Verifier);

} // namespace
} // namespace proofs

BENCHMARK_MAIN();
