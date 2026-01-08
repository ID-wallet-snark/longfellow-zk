#ifndef PRIVACY_PROOFS_ZK_LIB_CIRCUITS_ANONCRED_PTRCRED_EU_RESIDENCY_H_
#define PRIVACY_PROOFS_ZK_LIB_CIRCUITS_ANONCRED_PTRCRED_EU_RESIDENCY_H_

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

// EU/EEA/Switzerland country codes (ISO 3166-1 alpha-2)
// EU: 27 countries + EEA (Iceland, Liechtenstein, Norway) + Switzerland
static constexpr const char *kEUEEACountryCodes[] = {
    // EU Member States (27)
    "AT", "BE", "BG", "HR", "CY", "CZ", "DK", "EE", "FI", "FR", "DE", "GR",
    "HU", "IE", "IT", "LV", "LT", "LU", "MT", "NL", "PL", "PT", "RO", "SK",
    "SI", "ES", "SE",
    // EEA (non-EU)
    "IS", "LI", "NO",
    // Switzerland
    "CH"};
static constexpr size_t kNumEUEEACountries = 31;

// Specialized ptr-credential circuit that verifies if a country code
// attribute belongs to the EU/EEA/Switzerland area.
template <class LogicCircuit, class Field, class EC, size_t kNumAttr>
class PtrCredEUResidency {
  using EltW = typename LogicCircuit::EltW;
  using Elt = typename LogicCircuit::Elt;
  using Nat = typename Field::N;
  using Ecdsa = VerifyCircuit<LogicCircuit, Field, EC>;
  using EcdsaWitness = typename Ecdsa::Witness;

  using v8 = typename LogicCircuit::v8;
  using v32 = typename LogicCircuit::v32;
  using BitW = typename LogicCircuit::BitW;
  static constexpr size_t kIndexBits = 5;
  static constexpr size_t kMaxSHABlocks = 9;
  static constexpr size_t kMaxMsoLen = kMaxSHABlocks * 64 - 9;

  using vind = typename LogicCircuit::template bitvec<kIndexBits>;
  using Flatsha = FlatSHA256Circuit<LogicCircuit, BitPlucker<LogicCircuit, 3>>;
  using RoutingL = Routing<LogicCircuit>;
  using ShaBlockWitness = typename Flatsha::BlockWitness;

  const LogicCircuit &lc_;
  const EC &ec_;
  const Nat &order_;

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

    void input(const LogicCircuit &lc) {
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
    void input(const LogicCircuit &lc) {
      ind = lc.template vinput<8>();
      len = lc.template vinput<8>();
      for (size_t j = 0; j < 32; ++j) {
        v1[j] = lc.template vinput<8>();
      }
    }
  };

  struct CountryAttribute {
    v8 ind; /* index of country code attribute */
    v8 len; /* length of attribute (should be 2 for ISO alpha-2) */
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

  void assert_credential(EltW pkX, EltW pkY, EltW hash_tr,
                         OpenedAttribute oa[/* NUM_ATTR */],
                         const CountryAttribute &country_attr,
                         const v8 now[/*kDateLen*/], const Witness &vw) const {
    Ecdsa ecc(lc_, ec_, order_);

#ifndef PTRCRED_SKIP_CRYPTO_CHECKS
    // Signature verification
    ecc.verify_signature3(pkX, pkY, vw.e_, vw.sig_);
    ecc.verify_signature3(vw.dpkx_, vw.dpky_, hash_tr, vw.dpk_sig_);
    sha_.assert_message(kMaxSHABlocks, vw.nb_, vw.in_, vw.sig_sha_);

    const Memcmp<LogicCircuit> CMP(lc_);
    // Date range checks
    lc_.assert1(CMP.leq(kDateLen, &vw.raw_[84], &now[0]));
    lc_.assert1(CMP.leq(kDateLen, &now[0], &vw.raw_[92]));

    EltW dpkx = repack(vw.raw_, 100);
    EltW dpky = repack(vw.raw_, 132);
    lc_.assert_eq(&dpkx, vw.dpkx_);
    lc_.assert_eq(&dpky, vw.dpky_);
#endif

    const v8 zz = lc_.template vbit<8>(0xff); // cannot appear in strings
    std::vector<v8> cmp_buf(32);

    // Verify regular attributes
    for (size_t ai = 0; ai < kNumAttr; ++ai) {
      r_.shift(oa[ai].ind, 32, &cmp_buf[0], kMaxMsoLen, vw.raw_, zz, 3);
      assert_attribute(32, oa[ai].len, &cmp_buf[0], &oa[ai].v1[0]);
    }

    // Verify country is in EU/EEA/Switzerland
    assert_country_in_eu(country_attr, vw);
  }

private:
  void assert_attribute(size_t max, const v8 &vlen, const v8 got[/*max*/],
                        const v8 want[/*max*/]) const {
    for (size_t j = 0; j < max; ++j) {
      auto ll = lc_.vlt(j, vlen);
      auto cmp = lc_.veq(&got[j], want[j]);
      lc_.assert_implies(&ll, cmp);
    }
  }

  void assert_country_in_eu(const CountryAttribute &country_attr,
                            const Witness &vw) const {
    // Assert country code length is 2 (ISO 3166-1 alpha-2)
    lc_.assert1(lc_.veq(country_attr.len, UINT64_C(2)));

    const v8 zz = lc_.template vbit<8>(0xff);
    std::vector<v8> country_code(32);
    r_.shift(country_attr.ind, 32, country_code.data(), kMaxMsoLen, vw.raw_, zz,
             3);

    // Extract the 2-byte country code
    v8 cc_byte1 = country_code[0];
    v8 cc_byte2 = country_code[1];

    // Check if country code matches any EU/EEA/CH code
    // We'll create a disjunction (OR) of all possible matches
    BitW is_eu_country = lc_.bit(0); // Start with false

    for (size_t i = 0; i < kNumEUEEACountries; ++i) {
      const char *code = kEUEEACountryCodes[i];

      // Check if both bytes match
      auto byte1_match = lc_.veq(cc_byte1, static_cast<uint8_t>(code[0]));
      auto byte2_match = lc_.veq(cc_byte2, static_cast<uint8_t>(code[1]));
      auto both_match = lc_.land(&byte1_match, byte2_match);

      // OR with previous matches
      is_eu_country = lc_.lor(&is_eu_country, both_match);
    }

    // Assert that the country is in EU/EEA/CH
    lc_.assert1(is_eu_country);
  }

  Flatsha sha_;
  RoutingL r_;
};

} // namespace proofs

#endif // PRIVACY_PROOFS_ZK_LIB_CIRCUITS_ANONCRED_PTRCRED_EU_RESIDENCY_H_
