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

#ifndef PRIVACY_PROOFS_ZK_LIB_CIRCUITS_ANONCRED_PTRCRED_AGE_OVER_18_H_
#define PRIVACY_PROOFS_ZK_LIB_CIRCUITS_ANONCRED_PTRCRED_AGE_OVER_18_H_

#include <cstddef>
#include <cstdint>
#include <vector>

#include "circuits/anoncred/small_io.h"
#include "circuits/ecdsa/verify_circuit.h"
#include "circuits/logic/bit_plucker.h"
#include "circuits/logic/memcmp.h"
#include "circuits/logic/routing.h"
#include "circuits/sha/flatsha256_circuit.h"

namespace proofs {

// Specialized ptr-credential circuit that, in addition to the usual
// signature and attribute checks, enforces that a revealed age attribute
// (ASCII decimal) is at least the provided threshold (defaults to 18).
template <class LogicCircuit, class Field, class EC, size_t kNumAttr>
class PtrCredAgeOver18 {
  using EltW = typename LogicCircuit::EltW;
  using Elt = typename LogicCircuit::Elt;
  using Nat = typename Field::N;
  using Ecdsa = VerifyCircuit<LogicCircuit, Field, EC>;
  using EcdsaWitness = typename Ecdsa::Witness;

  using v8 = typename LogicCircuit::v8;
  using v32 = typename LogicCircuit::v32;
  static constexpr size_t kIndexBits = 5;
  static constexpr size_t kMaxSHABlocks = 3;
  static constexpr size_t kMaxMsoLen = kMaxSHABlocks * 64 - 9;

  using vind = typename LogicCircuit::template bitvec<kIndexBits>;
  using Flatsha = FlatSHA256Circuit<LogicCircuit, BitPlucker<LogicCircuit, 3>>;
  using RoutingL = Routing<LogicCircuit>;
  using ShaBlockWitness = typename Flatsha::BlockWitness;

  const LogicCircuit& lc_;
  const EC& ec_;
  const Nat& order_;

 public:
  class Witness {
   public:
    EltW e_;
    EltW dpkx_, dpky_;

    EcdsaWitness sig_;
    EcdsaWitness dpk_sig_;

    v8 in_[64 * kMaxSHABlocks];  /* transformed bytes fed to SHA */
    v8 raw_[64 * kMaxSHABlocks]; /* raw credential bytes for routing/offsets */
    v8 nb_; /* index of sha block that contains the real hash  */
    ShaBlockWitness sig_sha_[kMaxSHABlocks];

    void input(const LogicCircuit& lc) {
      e_ = lc.eltw_input();
      dpkx_ = lc.eltw_input();
      dpky_ = lc.eltw_input();

      sig_.input(lc);
      dpk_sig_.input(lc);

      nb_ = lc.template vinput<8>();

      for (size_t i = 0; i < 64 * kMaxSHABlocks; ++i) {
        in_[i] = lc.template vinput<8>();
      }
      for (size_t i = 0; i < 64 * kMaxSHABlocks; ++i) {
        raw_[i] = lc.template vinput<8>();
      }
      for (size_t j = 0; j < kMaxSHABlocks; ++j) {
        sig_sha_[j].input(lc);
      }
    }
  };

  struct OpenedAttribute {
    v8 ind;    /* index of attribute */
    v8 len;    /* length of attribute, 1--32 */
    v8 v1[32]; /* attribute value */
    void input(const LogicCircuit& lc) {
      ind = lc.template vinput<8>();
      len = lc.template vinput<8>();
      for (size_t j = 0; j < 32; ++j) {
        v1[j] = lc.template vinput<8>();
      }
    }
  };

  struct AgeAttribute {
    v8 ind; /* index of age attribute */
    v8 len; /* length of attribute, 1--32 */
    void input(const LogicCircuit& lc) {
      ind = lc.template vinput<8>();
      len = lc.template vinput<8>();
    }
  };

  EltW repack(const v8 in[], size_t ind) const {
    EltW h = lc_.konst(0);
    EltW base = lc_.konst(0x2);
    for (size_t i = 0; i < 32; ++i) {
      for (size_t j = 0; j < 8; ++j) {
        auto t = lc_.mul(&h, base);
        auto tin = lc_.eval(in[ind + i][7 - j]);
        h = lc_.add(&tin, t);
      }
    }
    return h;
  }

  explicit PtrCredAgeOver18(const LogicCircuit& lc, const EC& ec,
                            const Nat& order)
      : lc_(lc), ec_(ec), order_(order), sha_(lc), r_(lc) {}

  void assert_credential(EltW pkX, EltW pkY, EltW hash_tr,
                         OpenedAttribute oa[/* NUM_ATTR */],
                         const AgeAttribute& age_attr,
                         const v8 now[/*kDateLen*/], const Witness& vw,
                         uint64_t min_age = 18) const {
    Ecdsa ecc(lc_, ec_, order_);

#ifndef PTRCRED_SKIP_CRYPTO_CHECKS
    // Temporarily disabled: signature and device key checks pending full integration.
    ecc.verify_signature3(pkX, pkY, vw.e_, vw.sig_);
    ecc.verify_signature3(vw.dpkx_, vw.dpky_, hash_tr, vw.dpk_sig_);
    // Verify hash over transformed input bytes used by SHA witness.
    sha_.assert_message(kMaxSHABlocks, vw.nb_, vw.in_, vw.sig_sha_);

    const Memcmp<LogicCircuit> CMP(lc_);
    // Date range checks are applied on raw credential bytes at fixed offsets.
    lc_.assert1(CMP.leq(kDateLen, &vw.raw_[84], &now[0]));
    lc_.assert1(CMP.leq(kDateLen, &now[0], &vw.raw_[92]));

    EltW dpkx = repack(vw.raw_, 100);
    EltW dpky = repack(vw.raw_, 132);
    lc_.assert_eq(&dpkx, vw.dpkx_);
    lc_.assert_eq(&dpky, vw.dpky_);
#endif
    const v8 zz = lc_.template vbit<8>(0xff);  // cannot appear in strings
    std::vector<v8> cmp_buf(32);
    for (size_t ai = 0; ai < kNumAttr; ++ai) {
  r_.shift(oa[ai].ind, 32, &cmp_buf[0], kMaxMsoLen, vw.raw_, zz, 3);
      assert_attribute(32, oa[ai].len, &cmp_buf[0], &oa[ai].v1[0]);
    }

    assert_age_geq(age_attr, vw, min_age);
  }

 private:
  void assert_attribute(size_t max, const v8& vlen, const v8 got[/*max*/],
                        const v8 want[/*max*/]) const {
    for (size_t j = 0; j < max; ++j) {
      auto ll = lc_.vlt(j, vlen);
      auto cmp = lc_.veq(&got[j], want[j]);
      lc_.assert_implies(&ll, cmp);
    }
  }

  void assert_age_geq(const AgeAttribute& age_attr, const Witness& vw,
                      uint64_t min_age) const {
    lc_.assert1(lc_.vlt(UINT64_C(0), age_attr.len));

    const v8 zz = lc_.template vbit<8>(0xff);
    std::vector<v8> buf(32);
  r_.shift(age_attr.ind, 32, buf.data(), kMaxMsoLen, vw.raw_, zz, 3);
    assert_decimal_geq(age_attr.len, buf.data(), min_age);
  }

  void assert_decimal_geq(const v8& vlen, const v8* digits,
                          uint64_t min_value) const {
    constexpr size_t kValueBits = 64;
    constexpr uint64_t kMaxDecimalDigits = 20;

    lc_.assert0(lc_.vlt(kMaxDecimalDigits, vlen));

    auto value = lc_.template vbit<kValueBits>(0);

    for (size_t j = 0; j < 32; ++j) {
      auto in_range = lc_.vlt(j, vlen);

      auto below_zero = lc_.vlt(digits[j], '0');
      lc_.assert0(lc_.land(&in_range, below_zero));

      auto above_nine = lc_.vlt('9', digits[j]);
      lc_.assert0(lc_.land(&in_range, above_nine));

  auto digit_bits = lc_.template vbit<kValueBits>(0);
      for (size_t b = 0; b < 4; ++b) {
        digit_bits[b] = digits[j][b];
      }

      auto times2 = lc_.vshl(value, 1);
      auto times8 = lc_.vshl(value, 3);
      auto times10 = lc_.vadd(times2, times8);
      auto candidate = lc_.vadd(times10, digit_bits);

      for (size_t b = 0; b < kValueBits; ++b) {
        auto old_bit = value[b];
        value[b] = lc_.mux(&in_range, &candidate[b], old_bit);
      }
    }

    lc_.assert0(lc_.vlt(value, min_value));
  }

  Flatsha sha_;
  RoutingL r_;
};

}  // namespace proofs

#endif  // PRIVACY_PROOFS_ZK_LIB_CIRCUITS_ANONCRED_PTRCRED_AGE_OVER_18_H_
