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

#ifndef PRIVACY_PROOFS_ZK_LIB_CIRCUITS_ANONCRED_PTRCRED_WITNESS_H_
#define PRIVACY_PROOFS_ZK_LIB_CIRCUITS_ANONCRED_PTRCRED_WITNESS_H_

#include <stddef.h>
#include <string.h>

#include <cstdint>
#include <vector>

#include "algebra/static_string.h"
#include "arrays/dense.h"
#include "circuits/anoncred/small_io.h"
#include "circuits/ecdsa/verify_witness.h"
#include "circuits/logic/bit_plucker_encoder.h"
#include "circuits/sha/flatsha256_witness.h"
#include "util/crypto.h"

namespace proofs {

template <class Nat> Nat nat_from_be(const uint8_t be[]) {
  uint8_t tmp[Nat::kBytes];
  for (size_t i = 0; i < Nat::kBytes; ++i)
    tmp[i] = be[Nat::kBytes - i - 1];
  return Nat::of_bytes(tmp);
}

template <typename Nat> Nat nat_from_hash(const uint8_t data[], size_t len) {
  constexpr size_t kSHA256DigestSize = 32;
  uint8_t hash[kSHA256DigestSize];
  SHA256 sha;
  sha.Update(data, len);
  sha.DigestData(hash);
  Nat ne = nat_from_be<Nat>(hash);
  return ne;
}

template <typename EC, typename Field, class ScalarField,
          size_t kMaxSHABlocks = 3>
class PtrCredWitness {
  using Elt = typename Field::Elt;
  using Nat = typename Field::N;
  using EcdsaWitness = VerifyWitness3<EC, ScalarField>;

public:
  const EC ec_;
  Elt e_, e2_, dpkx_, dpky_;
  EcdsaWitness ew_, dkw_;
  uint8_t now_[kDateLen];

  FlatSHA256Witness::BlockWitness bw_[kMaxSHABlocks];
  uint8_t signed_bytes_[kMaxSHABlocks * 64];
  uint8_t raw_bytes_[kMaxSHABlocks * 64];
  uint8_t numb_;

  explicit PtrCredWitness(const EC &ec, const ScalarField &Fn)
      : ec_(ec), ew_(Fn, ec), dkw_(Fn, ec) {}

  void fill_sha(DenseFiller<Field> &filler,
                const FlatSHA256Witness::BlockWitness &bw) const {
    BitPluckerEncoder<Field, 3> BPENC(ec_.f_);
    for (size_t k = 0; k < 48; ++k)
      filler.push_back(BPENC.mkpacked_v32(bw.outw[k]));
    for (size_t k = 0; k < 64; ++k) {
      filler.push_back(BPENC.mkpacked_v32(bw.oute[k]));
      filler.push_back(BPENC.mkpacked_v32(bw.outa[k]));
    }
    for (size_t k = 0; k < 8; ++k)
      filler.push_back(BPENC.mkpacked_v32(bw.h1[k]));
  }

  void fill_witness(DenseFiller<Field> &filler) const {
    filler.push_back(e_);
    filler.push_back(dpkx_);
    filler.push_back(dpky_);
    ew_.fill_witness(filler);
    dkw_.fill_witness(filler);
    filler.push_back(numb_, 8, ec_.f_);
    for (size_t i = 0; i < kMaxSHABlocks * 64; ++i)
      filler.push_back(signed_bytes_[i], 8, ec_.f_);
    for (size_t i = 0; i < kMaxSHABlocks * 64; ++i)
      filler.push_back(raw_bytes_[i], 8, ec_.f_);
    for (size_t j = 0; j < kMaxSHABlocks; j++)
      fill_sha(filler, bw_[j]);
  }

  bool compute_witness(Elt pkX, Elt pkY, const uint8_t cred[], size_t len,
                       const uint8_t transcript[], size_t tlen,
                       const uint8_t tnow[], const uint8_t r[32],
                       const uint8_t s[32], const uint8_t dr[32],
                       const uint8_t ds[32]) {
    Nat nr = nat_from_be<Nat>(r);
    Nat ns = nat_from_be<Nat>(s);
    Nat nr2 = nat_from_be<Nat>(dr);
    Nat ns2 = nat_from_be<Nat>(ds);
    return compute_witness_internal(pkX, pkY, cred, len, transcript, tlen, tnow,
                                    nr, ns, nr2, ns2);
  }

  // Legacy overload for StaticString (keeps existing code working if any)
  bool compute_witness(Elt pkX, Elt pkY, const uint8_t cred[], size_t len,
                       const uint8_t transcript[], size_t tlen,
                       const uint8_t tnow[], const StaticString &r,
                       const StaticString &s, const StaticString &dr,
                       const StaticString &ds) {
    Nat nr(r);
    Nat ns(s);
    Nat nr2(dr);
    Nat ns2(ds);
    return compute_witness_internal(pkX, pkY, cred, len, transcript, tlen, tnow,
                                    nr, ns, nr2, ns2);
  }

private:
  bool compute_witness_internal(Elt pkX, Elt pkY, const uint8_t cred[],
                                size_t len, const uint8_t transcript[],
                                size_t tlen, const uint8_t tnow[],
                                const Nat &nr, const Nat &ns, const Nat &nr2,
                                const Nat &ns2) {
    Nat ne = nat_from_hash<Nat>(cred, len);
    e_ = ec_.f_.to_montgomery(ne);
    ew_.compute_witness(pkX, pkY, ne, nr, ns);

    Nat ne2 = nat_from_hash<Nat>(transcript, tlen);
    dpkx_ = ec_.f_.to_montgomery(nat_from_be<Nat>(&cred[100]));
    dpky_ = ec_.f_.to_montgomery(nat_from_be<Nat>(&cred[132]));
    e2_ = ec_.f_.to_montgomery(ne2);
    dkw_.compute_witness(dpkx_, dpky_, ne2, nr2, ns2);

    memset(raw_bytes_, 0, sizeof(raw_bytes_));
    size_t copy_len = len < sizeof(raw_bytes_) ? len : sizeof(raw_bytes_);
    memcpy(raw_bytes_, cred, copy_len);
    FlatSHA256Witness::transform_and_witness_message(len, cred, kMaxSHABlocks,
                                                     numb_, signed_bytes_, bw_);
    memcpy(now_, tnow, kDateLen);
    return true;
  }
};

} // namespace proofs

#endif // PRIVACY_PROOFS_ZK_LIB_CIRCUITS_ANONCRED_PTRCRED_WITNESS_H_
