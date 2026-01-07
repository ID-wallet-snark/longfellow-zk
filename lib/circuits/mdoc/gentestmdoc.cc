#include <algorithm>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "../../ec/p256.h"
#include "../../util/crypto.h"

using namespace proofs;

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------

using Scalar = typename proofs::Fp256Scalar::Elt;
using Base = typename proofs::Fp256Base::Elt;

void print_blob(const char *name, const std::vector<uint8_t> &b) {
  std::cout << "static const uint8_t " << name << "[] = {";
  for (size_t i = 0; i < b.size(); ++i) {
    if (i % 16 == 0)
      std::cout << "\n  ";
    std::cout << "0x" << std::hex << std::setw(2) << std::setfill('0')
              << (int)b[i];
    if (i < b.size() - 1)
      std::cout << ", ";
  }
  std::cout << "\n};" << std::endl;
}

std::string buf_to_hex_string(const uint8_t *buf, size_t len) {
  std::stringstream ss;
  ss << "0x";
  for (size_t i = 0; i < len; ++i) {
    ss << std::hex << std::setw(2) << std::setfill('0') << (int)buf[i];
  }
  return ss.str();
}

std::string scalar_to_hex(const Scalar &s) {
  uint8_t buf[32];
  p256_scalar.to_bytes_field(buf, s);
  return buf_to_hex_string(buf, 32);
}

std::string base_to_hex(const Base &b) {
  uint8_t buf[32];
  p256_base.to_bytes_field(buf, b);
  return buf_to_hex_string(buf, 32);
}

Scalar gen_scalar() {
  uint8_t buf[32];
  while (true) {
    rand_bytes(buf, 32);
    auto opt = p256_scalar.of_bytes_field(buf);
    if (opt)
      return *opt;
  }
}

// -----------------------------------------------------------------------------
// CBOR Building Blocks
// -----------------------------------------------------------------------------

std::vector<uint8_t> cbor_null() { return {0xF6}; }

std::vector<uint8_t> cbor_bool(bool val) {
  return {val ? (uint8_t)0xF5 : (uint8_t)0xF4};
}

std::vector<uint8_t> cbor_int(uint64_t val) {
  // Minimal impl for small ints
  if (val < 24)
    return {(uint8_t)val};
  return {(uint8_t)0x18, (uint8_t)val}; // 1 byte int
}

std::vector<uint8_t> cbor_bytes(const std::vector<uint8_t> &b) {
  std::vector<uint8_t> out;
  size_t len = b.size();
  if (len < 24) {
    out.push_back(0x40 | len);
  } else if (len <= 0xFF) {
    out.push_back(0x58);
    out.push_back((uint8_t)len);
  } else { // 16-bit
    out.push_back(0x59);
    out.push_back((len >> 8) & 0xFF);
    out.push_back(len & 0xFF);
  }
  out.insert(out.end(), b.begin(), b.end());
  return out;
}

std::vector<uint8_t> cbor_utf8(const std::string &s) {
  std::vector<uint8_t> out;
  size_t len = s.length();
  if (len < 24) {
    out.push_back(0x60 | len);
  } else {
    out.push_back(0x78);
    out.push_back((uint8_t)len);
  }
  out.insert(out.end(), s.begin(), s.end());
  return out;
}

std::vector<uint8_t> cbor_tag(uint64_t tag,
                              const std::vector<uint8_t> &content) {
  std::vector<uint8_t> out;
  if (tag == 24) {
    out.push_back(0xD8);
    out.push_back(0x18);
  } else {
    // Minimal impl
    throw std::runtime_error("Tag not supported");
  }
  out.insert(out.end(), content.begin(), content.end());
  return out;
}

void cbor_map_add(std::vector<uint8_t> &map_content,
                  const std::vector<uint8_t> &key,
                  const std::vector<uint8_t> &val) {
  map_content.insert(map_content.end(), key.begin(), key.end());
  map_content.insert(map_content.end(), val.begin(), val.end());
}

// -----------------------------------------------------------------------------
// Logic
// -----------------------------------------------------------------------------

std::vector<uint8_t> sign_cose_sign1(const std::vector<uint8_t> &payload,
                                     Scalar key, bool detached) {
  // Protected Header: map { 1 (alg): -7 (ES256) } -> bstr
  std::vector<uint8_t> prot_map;
  prot_map.push_back(0x01);
  prot_map.push_back(0x26);
  std::vector<uint8_t> prot_map_wrapped = {0xA1};
  prot_map_wrapped.insert(prot_map_wrapped.end(), prot_map.begin(),
                          prot_map.end());
  std::vector<uint8_t> protected_header = cbor_bytes(prot_map_wrapped);

  // Sig Structure for hashing:
  // [ "Signature1", protected_header_bytes, external_aad(empty), payload_bytes
  // ]
  std::vector<uint8_t> sig_struct = {0x84};
  std::vector<uint8_t> txt = cbor_utf8("Signature1");
  sig_struct.insert(sig_struct.end(), txt.begin(), txt.end());
  sig_struct.insert(sig_struct.end(), protected_header.begin(),
                    protected_header.end());
  sig_struct.push_back(0x40); // empty bstr (external aad)

  // Payload usually bstr_bytes if not detached, or payload_bytes directly?
  // "The payload is the plaintext to be signed".
  // Sig_structure payload field is `bstr`.
  // So we wrap payload in bstr?
  // Yes.
  std::vector<uint8_t> payload_bstr = cbor_bytes(payload);
  sig_struct.insert(sig_struct.end(), payload_bstr.begin(), payload_bstr.end());

  // Digest SigStruct
  proofs::SHA256 sig_sha;
  sig_sha.Update(sig_struct.data(), sig_struct.size());
  uint8_t sig_digest[32];
  sig_sha.DigestData(sig_digest);

  // Sign
  Scalar k_sig = gen_scalar();
  auto R = p256.scalar_multf(p256.generator(), k_sig.n);

  auto rx_nat = p256_base.from_montgomery(R.x);
  Scalar r = p256_scalar.reduce<4>(rx_nat);

  auto e_nat = Fp256Nat::of_bytes(sig_digest);
  Scalar e = p256_scalar.reduce<4>(e_nat);

  Scalar rd = p256_scalar.mulf(r, key);
  Scalar erd = p256_scalar.addf(e, rd);
  Scalar k_inv = p256_scalar.invertf(k_sig);
  Scalar s = p256_scalar.mulf(k_inv, erd);

  // Signature bytes: r | s
  std::vector<uint8_t> signature;
  uint8_t r_bytes[32], s_bytes[32];
  p256_scalar.to_bytes_field(r_bytes, r);
  p256_scalar.to_bytes_field(s_bytes, s);
  signature.insert(signature.end(), r_bytes, r_bytes + 32);
  signature.insert(signature.end(), s_bytes, s_bytes + 32);

  // Unprotected Header: map {} -> 0xA0
  std::vector<uint8_t> unprotected_header = {0xA0};

  // Construct COSE_Sign1:
  // [ protected, unprotected, payload, signature ]
  std::vector<uint8_t> cose_sign1 = {0x84};
  cose_sign1.insert(cose_sign1.end(), protected_header.begin(),
                    protected_header.end());
  cose_sign1.insert(cose_sign1.end(), unprotected_header.begin(),
                    unprotected_header.end());

  if (detached) {
    cose_sign1.push_back(0xF6); // null
  } else {
    // If not detached, payload is bstr
    cose_sign1.insert(cose_sign1.end(), payload_bstr.begin(),
                      payload_bstr.end());
  }

  std::vector<uint8_t> sig_bstr_enc = cbor_bytes(signature);
  cose_sign1.insert(cose_sign1.end(), sig_bstr_enc.begin(), sig_bstr_enc.end());

  return cose_sign1;
}

int main() {
  // 1. Generate Issuer Key
  Scalar d = gen_scalar();
  auto Q = p256.scalar_multf(p256.generator(), d.n);

  // 2. Generate Device Key
  Scalar k_dev = gen_scalar();
  auto P_dev = p256.scalar_multf(p256.generator(), k_dev.n);

  // 3. Prepare Attribute "is_student"
  // Canonical Order (Length then Bytes):
  // random (6), digestID (8), elementValue (12), elementIdentifier (17)
  std::vector<uint8_t> attr_val = {0xA4}; // Map(4)

  // random
  std::vector<uint8_t> rnd_bytes(16);
  proofs::rand_bytes(rnd_bytes.data(), 16);
  cbor_map_add(attr_val, cbor_utf8("random"), cbor_bytes(rnd_bytes));

  // digestID
  cbor_map_add(attr_val, cbor_utf8("digestID"), cbor_int(0));

  // elementValue
  cbor_map_add(attr_val, cbor_utf8("elementValue"), cbor_bool(true));

  // elementIdentifier
  cbor_map_add(attr_val, cbor_utf8("elementIdentifier"),
               cbor_utf8("is_student"));
  proofs::SHA256 sha;
  sha.Update(attr_val.data(), attr_val.size());
  uint8_t digest[32];
  sha.DigestData(digest);
  std::vector<uint8_t> digest_vec(digest, digest + 32);

  // 4. Construct MSO (Mobile Security Object)
  // map {
  //   "version": "1.0",
  //   "digestAlgorithm": "SHA-256",
  //   "valueDigests": { "fr.gouv.education.1": { 0: digest } },
  //   "deviceKeyInfo": tagged(24, bstr(COSE_Key(P_dev))),
  //   "docType": "fr.gouv.education.1.student"
  //   "validityInfo": { "signed": tdate, "validFrom": tdate, "validUntil":
  //   tdate }
  // }

  // MSO Map Order (Canonical: Length match, then Lexicographical):
  // 1. docType (7 chars) -> len 8
  // 2. version (7 chars) -> len 8 (d < v)
  // 3. validityInfo (12 chars) -> len 13
  // 4. valueDigests (12 chars) -> len 13 (v ... i < u -> valIdity < valUe)
  // 5. deviceKeyInfo (13 chars) -> len 14
  // 6. digestAlgorithm (15 chars) -> len 16

  std::vector<uint8_t> mso_map;

  // 1. docType
  cbor_map_add(mso_map, cbor_utf8("docType"),
               cbor_utf8("fr.gouv.education.1.student"));

  // 2. version
  cbor_map_add(mso_map, cbor_utf8("version"), cbor_utf8("1.0"));

  // 3. validityInfo
  std::vector<uint8_t> val_map;
  cbor_map_add(val_map, cbor_utf8("signed"), cbor_utf8("2024-06-01T12:00:00Z"));
  cbor_map_add(val_map, cbor_utf8("validFrom"),
               cbor_utf8("2024-06-01T12:00:00Z"));
  cbor_map_add(val_map, cbor_utf8("validUntil"),
               cbor_utf8("2025-06-01T12:00:00Z"));
  std::vector<uint8_t> val_map_wrap = {0xA3};
  val_map_wrap.insert(val_map_wrap.end(), val_map.begin(), val_map.end());
  cbor_map_add(mso_map, cbor_utf8("validityInfo"), val_map_wrap);

  // 4. valueDigests
  std::vector<uint8_t> inner_map;
  inner_map.push_back(0x00);
  std::vector<uint8_t> dig_bytes = cbor_bytes(digest_vec);
  inner_map.insert(inner_map.end(), dig_bytes.begin(), dig_bytes.end());
  std::vector<uint8_t> ns_map_content = {0xA1};
  ns_map_content.insert(ns_map_content.end(), inner_map.begin(),
                        inner_map.end());

  std::vector<uint8_t> vd_map;
  cbor_map_add(vd_map, cbor_utf8("fr.gouv.education.1"), ns_map_content);
  std::vector<uint8_t> vd_content = {0xA1};
  vd_content.insert(vd_content.end(), vd_map.begin(), vd_map.end());
  cbor_map_add(mso_map, cbor_utf8("valueDigests"), vd_content);

  // 5. deviceKeyInfo
  std::vector<uint8_t> cose_key;
  cose_key.push_back(0x01);
  cose_key.push_back(0x02); // kty: EC2
  cose_key.push_back(0x03);
  cose_key.push_back(0x26); // alg: ES256
  cose_key.push_back(0x20);
  cose_key.push_back(0x01); // crv: P-256

  uint8_t x_buf[32], y_buf[32];
  p256_base.to_bytes_field(x_buf, P_dev.x);
  p256_base.to_bytes_field(y_buf, P_dev.y);
  // Convert LE to BE for COSE
  std::reverse(x_buf, x_buf + 32);
  std::reverse(y_buf, y_buf + 32);

  // Canonical order: -3 (y) < -2 (x)
  cose_key.push_back(0x22); // -3 (y)
  std::vector<uint8_t> y_vec(y_buf, y_buf + 32);
  std::vector<uint8_t> yb = cbor_bytes(y_vec);
  cose_key.insert(cose_key.end(), yb.begin(), yb.end());

  cose_key.push_back(0x21); // -2 (x)
  std::vector<uint8_t> x_vec(x_buf, x_buf + 32);
  std::vector<uint8_t> xb = cbor_bytes(x_vec);
  cose_key.insert(cose_key.end(), xb.begin(), xb.end());

  std::vector<uint8_t> mkey = {0xA5};
  mkey.insert(mkey.end(), cose_key.begin(), cose_key.end());

  // ParsedMdoc expects deviceKeyInfo -> Map -> deviceKey -> COSE_Key
  // It does NOT expect Tag 24 or bstr wrapping in the mdoc_witness.h logic.
  std::vector<uint8_t> wrapper_map;
  cbor_map_add(wrapper_map, cbor_utf8("deviceKey"), mkey);
  std::vector<uint8_t> wrapper_val = {0xA1};
  wrapper_val.insert(wrapper_val.end(), wrapper_map.begin(), wrapper_map.end());

  cbor_map_add(mso_map, cbor_utf8("deviceKeyInfo"), wrapper_val);

  // 6. digestAlgorithm
  cbor_map_add(mso_map, cbor_utf8("digestAlgorithm"), cbor_utf8("SHA-256"));

  // MSO Wrap
  std::vector<uint8_t> mso_bytes = {0xA6}; // map(6)
  mso_bytes.insert(mso_bytes.end(), mso_map.begin(), mso_map.end());
  // Manually construct bstr with 0x59 (16-bit length) to satisfy ParsedMdoc
  // assumption which skips 5 bytes (D8 18 59 LL LL).
  std::vector<uint8_t> mso_bstr;
  mso_bstr.push_back(0x59);
  mso_bstr.push_back((mso_bytes.size() >> 8) & 0xFF);
  mso_bstr.push_back(mso_bytes.size() & 0xFF);
  mso_bstr.insert(mso_bstr.end(), mso_bytes.begin(), mso_bytes.end());

  std::vector<uint8_t> tagged_mso = cbor_tag(24, mso_bstr);

  // 5. Sign MSO (IssuerSigned)
  std::vector<uint8_t> issuer_sign1 =
      sign_cose_sign1(tagged_mso, d, false); // NOT detached?
  // IssuerAuth is COSE_Sign1 where payload IS the MSO (tagged).
  // Wait, standard says IssuerAuth is COSE_Sign1.
  // Payload is MSO.
  // Is it detached? usually NO.
  // "The payload is the Mobile Security Object."
  // So we pass tagged_mso as payload.

  // 6. DeviceSigned and DeviceAuthentication
  // Session Transcript: [null, null, null]
  std::vector<uint8_t> session_transcript = {0x83, 0xF6, 0xF6,
                                             0xF6}; // Array(3) of nulls

  // DeviceNameSpacesBytes: bstr(map{}) = bstr(A0) = 41 A0?
  // Or A0.
  // DeviceSigned contains "nameSpaces".
  // "nameSpaces" is Bytes. The value these bytes decode to is a Map.
  // We use empty map.
  // DeviceNameSpacesBytes: tagged(24, bstr(map{}))
  std::vector<uint8_t> empty_map = {0xA0};
  std::vector<uint8_t> dev_ns_bytes_inner = cbor_bytes(empty_map);
  std::vector<uint8_t> dev_ns_bytes = cbor_tag(24, dev_ns_bytes_inner);

  // DeviceAuthentication array
  // [ "DeviceAuthentication", SessionTranscript, DocType, DeviceNameSpacesBytes
  // ]
  std::vector<uint8_t> dev_auth;
  dev_auth.push_back(0x84); // Array(4)
  std::vector<uint8_t> da_txt = cbor_utf8("DeviceAuthentication");
  dev_auth.insert(dev_auth.end(), da_txt.begin(), da_txt.end()); // 1

  // SessionTranscript is already encoded as Array.
  // We insert it as an element.
  // Wait, `SessionTranscript` structure IS a CBOR Array.
  // So we insert the bytes of the array.
  dev_auth.insert(dev_auth.end(), session_transcript.begin(),
                  session_transcript.end()); // 2

  std::vector<uint8_t> doc_type = cbor_utf8("fr.gouv.education.1.student");
  dev_auth.insert(dev_auth.end(), doc_type.begin(), doc_type.end()); // 3

  // DeviceNameSpacesBytes is bstr.
  dev_auth.insert(dev_auth.end(), dev_ns_bytes.begin(),
                  dev_ns_bytes.end()); // 4

  // Sign encoded DeviceAuthentication (as detached payload)
  // Payload must be DeviceAuthenticationBytes = #6.24(bstr .cbor
  // DeviceAuthentication)
  std::vector<uint8_t> dev_auth_bstr = cbor_bytes(dev_auth);
  std::vector<uint8_t> dev_auth_tagged = cbor_tag(24, dev_auth_bstr);

  std::vector<uint8_t> device_sign1 =
      sign_cose_sign1(dev_auth_tagged, k_dev, true); // Detached

  // 7. Output C++
  std::cout << "// Auto-generated by gentestmdoc" << std::endl << std::endl;

  std::cout << "static const StaticString kStudentIssuerPKX = StaticString(\""
            << base_to_hex(Q.x) << "\");" << std::endl;
  std::cout << "static const StaticString kStudentIssuerPKY = StaticString(\""
            << base_to_hex(Q.y) << "\");" << std::endl;
  // Print k_dev (unused in mdoc_examples.h but good for ref)

  // Reconstruct Mdoc map
  // docType
  // issuerSigned
  // deviceSigned

  // IssuerSigned
  std::vector<uint8_t> tagged_attr = cbor_tag(24, cbor_bytes(attr_val));
  std::vector<uint8_t> attr_list = {0x81};
  attr_list.insert(attr_list.end(), tagged_attr.begin(), tagged_attr.end());

  std::vector<uint8_t> ns_map_inner;
  cbor_map_add(ns_map_inner, cbor_utf8("fr.gouv.education.1"), attr_list);
  std::vector<uint8_t> ns_map_val = {0xA1};
  ns_map_val.insert(ns_map_val.end(), ns_map_inner.begin(), ns_map_inner.end());

  std::vector<uint8_t> is_map;
  // issuerAuth (10 chars) vs nameSpaces (10 chars)
  // i < n. So issuerAuth first.
  cbor_map_add(is_map, cbor_utf8("issuerAuth"),
               issuer_sign1); // COSE_Sign1 (not detached)
  cbor_map_add(is_map, cbor_utf8("nameSpaces"), ns_map_val);

  std::vector<uint8_t> is_val = {0xA2}; // Map(2)
  is_val.insert(is_val.end(), is_map.begin(), is_map.end());

  // DeviceSigned
  std::vector<uint8_t> ds_map;

  // deviceAuth < nameSpaces in canonical CBOR (0x64 < 0x6E)
  // DeviceAuth must be a Map { "deviceSignature": COSE_Sign1 }
  std::vector<uint8_t> dev_auth_map;
  cbor_map_add(dev_auth_map, cbor_utf8("deviceSignature"), device_sign1);
  std::vector<uint8_t> dam_wrap = {0xA1};
  dam_wrap.insert(dam_wrap.end(), dev_auth_map.begin(), dev_auth_map.end());

  cbor_map_add(ds_map, cbor_utf8("deviceAuth"), dam_wrap);
  cbor_map_add(ds_map, cbor_utf8("nameSpaces"), dev_ns_bytes);

  std::vector<uint8_t> ds_val = {0xA2};
  ds_val.insert(ds_val.end(), ds_map.begin(), ds_map.end());

  // Doc Map
  // Order: docType (8), deviceSigned (13, 'd'), issuerSigned (13, 'i')
  // 1. docType
  // 2. deviceSigned
  // 3. issuerSigned
  std::vector<uint8_t> doc_map;
  cbor_map_add(doc_map, cbor_utf8("docType"),
               cbor_utf8("fr.gouv.education.1.student"));
  cbor_map_add(doc_map, cbor_utf8("deviceSigned"), ds_val);
  cbor_map_add(doc_map, cbor_utf8("issuerSigned"), is_val);

  std::vector<uint8_t> doc_val = {0xA3}; // Map(3)
  doc_val.insert(doc_val.end(), doc_map.begin(), doc_map.end());

  // Wrap in DeviceResponse:
  // {
  //   "version": "1.0",
  //   "documents": [ doc_val ]
  // }
  std::vector<uint8_t> dr_map;

  // "documents" (do...) vs "version" (ve...). 'd' < 'v'.
  // documents
  std::vector<uint8_t> docs_arr = {0x81}; // Array(1)
  docs_arr.insert(docs_arr.end(), doc_val.begin(), doc_val.end());
  cbor_map_add(dr_map, cbor_utf8("documents"), docs_arr);

  // version
  cbor_map_add(dr_map, cbor_utf8("version"), cbor_utf8("1.0"));

  std::vector<uint8_t> dr_val = {0xA2}; // Map(2)
  dr_val.insert(dr_val.end(), dr_map.begin(), dr_map.end());

  print_blob("kStudentMdoc", dr_val);

  return 0;
}
