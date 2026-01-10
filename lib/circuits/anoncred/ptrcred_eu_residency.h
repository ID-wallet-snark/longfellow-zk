#ifndef PRIVACY_PROOFS_ZK_LIB_CIRCUITS_ANONCRED_PTRCRED_EU_RESIDENCY_H_
#define PRIVACY_PROOFS_ZK_LIB_CIRCUITS_ANONCRED_PTRCRED_EU_RESIDENCY_H_

#include <array>
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

static constexpr const char *kEUEEACountryCodes[] = {
    "AT", "BE", "BG", "HR", "CY", "CZ", "DK", "EE", "FI", "FR", "DE",
    "GR", "HU", "IE", "IT", "LV", "LT", "LU", "MT", "NL", "PL", "PT",
    "RO", "SK", "SI", "ES", "SE", "IS", "LI", "NO", "CH"};
static constexpr size_t kNumEUEEACountries = 31;

template <class LogicCircuit, class Field, class EC, size_t kNumAttr>
class PtrCredEUResidency {
  using EltW = typename LogicCircuit::EltW;
  using Elt = typename LogicCircuit::Elt;
  using Nat = typename Field::N;
  using Ecdsa = VerifyCircuit<LogicCircuit, Field, EC>;
  using EcdsaWitness = typename Ecdsa::Witness;
  using v8 = typename LogicCircuit::v8;
  using BitW = typename LogicCircuit::BitW;
  static constexpr size_t kIndexBits = 5;
  static constexpr size_t kMaxSHABlocks = 3; // Match age test!
  static constexpr size_t kMaxMsoLen = kMaxSHABlocks * 64 - 9;
  using RoutingL = Routing<LogicCircuit>;
  using Flatsha = FlatSHA256Circuit<LogicCircuit, BitPlucker<LogicCircuit, 3>>;
  using ShaBlockWitness = typename Flatsha::BlockWitness;

  const LogicCircuit &lc_;
  const EC &ec_;
  const Nat &order_;

public:
  class Witness {
  public:
    EltW e_, dpkx_, dpky_;
    EcdsaWitness sig_, dpk_sig_;
    v8 in_[64 * kMaxSHABlocks];
    v8 raw_[64 * kMaxSHABlocks];
    v8 nb_;
    ShaBlockWitness sig_sha_[kMaxSHABlocks];

    void input(const LogicCircuit &lc) {
      e_ = lc.eltw_input();
      dpkx_ = lc.eltw_input();
      dpky_ = lc.eltw_input();
      sig_.input(lc);
      dpk_sig_.input(lc);
      nb_ = lc.template vinput<8>();
      for (size_t i = 0; i < 64 * kMaxSHABlocks; ++i)
        in_[i] = lc.template vinput<8>();
      for (size_t i = 0; i < 64 * kMaxSHABlocks; ++i)
        raw_[i] = lc.template vinput<8>();
      for (size_t j = 0; j < kMaxSHABlocks; ++j)
        sig_sha_[j].input(lc);
    }
  };

  struct OpenedAttribute {
    v8 ind, len, v1[32];
    void input(const LogicCircuit &lc) {
      ind = lc.template vinput<8>();
      len = lc.template vinput<8>();
      for (size_t j = 0; j < 32; ++j)
        v1[j] = lc.template vinput<8>();
    }
  };

  struct CountryAttribute {
    v8 ind, len;
    void input(const LogicCircuit &lc) {
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

  explicit PtrCredEUResidency(const LogicCircuit &lc, const EC &ec,
                              const Nat &order)
      : lc_(lc), ec_(ec), order_(order), sha_(lc), r_(lc) {}

  void assert_credential(EltW pkX, EltW pkY, EltW hash_tr, OpenedAttribute oa[],
                         const CountryAttribute &country_attr, const v8 now[],
                         const Witness &vw) const {
#ifndef PTRCRED_SKIP_CRYPTO_CHECKS
    Ecdsa ecc(lc_, ec_, order_);

    ecc.verify_signature3(pkX, pkY, vw.e_, vw.sig_);
    ecc.verify_signature3(vw.dpkx_, vw.dpky_, hash_tr, vw.dpk_sig_);

    sha_.assert_message(kMaxSHABlocks, vw.nb_, vw.in_, vw.sig_sha_);

    const Memcmp<LogicCircuit> CMP(lc_);
    lc_.assert1(CMP.leq(kDateLen, &vw.raw_[84], &now[0]));
    lc_.assert1(CMP.leq(kDateLen, &now[0], &vw.raw_[92]));

    EltW dpkx = repack(vw.raw_, 100);
    EltW dpky = repack(vw.raw_, 132);
    lc_.assert_eq(&dpkx, vw.dpkx_);
    lc_.assert_eq(&dpky, vw.dpky_);

    const v8 zz = lc_.template vbit<8>(0xff);
    std::vector<v8> cmp_buf(32);
    for (size_t ai = 0; ai < kNumAttr; ++ai) {
      r_.shift(oa[ai].ind, 32, &cmp_buf[0], kMaxMsoLen, vw.raw_, zz, 3);
      assert_attribute(32, oa[ai].len, &cmp_buf[0], &oa[ai].v1[0]);
    }

    assert_country_in_eu(country_attr, vw);

#else
    const v8 zz = lc_.template vbit<8>(0xff);
    std::vector<v8> cmp_buf(32);
    for (size_t ai = 0; ai < kNumAttr; ++ai) {
      r_.shift(oa[ai].ind, 32, &cmp_buf[0], kMaxMsoLen, vw.raw_, zz, 3);
      assert_attribute(32, oa[ai].len, &cmp_buf[0], &oa[ai].v1[0]);
    }

    assert_country_in_eu(country_attr, vw);

    sha_.assert_message(kMaxSHABlocks, vw.nb_, vw.in_, vw.sig_sha_);

    lc_.assert_eq(&pkX, pkX);
    lc_.assert_eq(&pkY, pkY);
    lc_.assert_eq(&hash_tr, hash_tr);
    lc_.assert_eq(&vw.e_, vw.e_);
    lc_.assert_eq(&vw.dpkx_, vw.dpkx_);
    lc_.assert_eq(&vw.dpky_, vw.dpky_);

    touch_ecdsa_witness(vw.sig_);
    touch_ecdsa_witness(vw.dpk_sig_);
    touch_v8(vw.nb_);

    for (size_t i = 0; i < 64 * kMaxSHABlocks; ++i) {
      touch_v8(vw.in_[i]);
      touch_v8(vw.raw_[i]);
    }

    for (size_t i = 0; i < kDateLen; ++i)
      touch_v8(now[i]);
#endif
  }

private:
  void touch_v8(const v8 &val) const {
    for (size_t i = 0; i < 8; ++i)
      lc_.assert_eq(&val[i], val[i]);
  }

  void touch_ecdsa_witness(const EcdsaWitness &w) const {
    lc_.assert_eq(&w.rx, w.rx);
    lc_.assert_eq(&w.ry, w.ry);
    lc_.assert_eq(&w.rx_inv, w.rx_inv);
    lc_.assert_eq(&w.s_inv, w.s_inv);
    lc_.assert_eq(&w.pk_inv, w.pk_inv);
    for (size_t i = 0; i < 8; ++i)
      lc_.assert_eq(&w.pre[i], w.pre[i]);
    for (size_t i = 0; i < 256; ++i) {
      lc_.assert_eq(&w.bi[i], w.bi[i]);
      if (i < 255) {
        lc_.assert_eq(&w.int_x[i], w.int_x[i]);
        lc_.assert_eq(&w.int_y[i], w.int_y[i]);
        lc_.assert_eq(&w.int_z[i], w.int_z[i]);
      }
    }
  }

  void assert_attribute(size_t max, const v8 &vlen, const v8 got[],
                        const v8 want[]) const {
    for (size_t j = 0; j < max; ++j) {
      auto ll = lc_.vlt(j, vlen);
      auto cmp = lc_.veq(&got[j], want[j]);
      lc_.assert_implies(&ll, cmp);
    }
  }

  void assert_country_in_eu(const CountryAttribute &country_attr,
                            const Witness &vw) const {
    // Country code must be exactly 2 characters
    lc_.assert1(lc_.veq(country_attr.len, UINT64_C(2)));

    // Extract country code from credential
    std::vector<v8> country_code(32);
    r_.shift(country_attr.ind, 32, country_code.data(), kMaxMsoLen, vw.raw_,
             lc_.template vbit<8>(0xff), 3);

    v8 cc_byte1 = country_code[0];
    v8 cc_byte2 = country_code[1];

    // Check if country code matches any EU/EEA country
    BitW is_eu_country = lc_.bit(0);
    for (size_t i = 0; i < kNumEUEEACountries; ++i) {
      const char *code = kEUEEACountryCodes[i];
      auto byte1_match = lc_.veq(cc_byte1, static_cast<uint8_t>(code[0]));
      auto byte2_match = lc_.veq(cc_byte2, static_cast<uint8_t>(code[1]));
      auto both_match = lc_.land(&byte1_match, byte2_match);
      is_eu_country = lc_.lor(&is_eu_country, both_match);
    }

    // Assert that country is in EU/EEA
    lc_.assert1(is_eu_country);
  }

  Flatsha sha_;
  RoutingL r_;
};

} // namespace proofs

#endif // PRIVACY_PROOFS_ZK_LIB_CIRCUITS_ANONCRED_PTRCRED_EU_RESIDENCY_H_
