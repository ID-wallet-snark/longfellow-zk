#include "circuits/anoncred/ptrcred_eu_residency.h"

#include <cstddef>
#include <memory>

#include "circuits/compiler/circuit_dump.h"
#include "circuits/compiler/compiler.h"
#include "circuits/logic/compiler_backend.h"
#include "circuits/logic/logic.h"
#include "ec/p256.h"
#include "gtest/gtest.h"

namespace proofs {
namespace {

TEST(PtrCredEUResidency, CircuitConstruction) {
  using CompilerBackend = CompilerBackend<Fp256Base>;
  using LogicCircuit = Logic<Fp256Base, CompilerBackend>;
  using v8 = typename LogicCircuit::v8;
  using EltW = LogicCircuit::EltW;
  using PtrCredEURes = PtrCredEUResidency<LogicCircuit, Fp256Base, P256, 1>;

  QuadCircuit<Fp256Base> Q(p256_base);
  const CompilerBackend cbk(&Q);
  const LogicCircuit LC(&cbk, p256_base);
  PtrCredEURes ptrcred(LC, p256, n256_order);

  EltW pkX = LC.eltw_input();
  EltW pkY = LC.eltw_input();
  EltW htr = LC.eltw_input();

  typename PtrCredEURes::OpenedAttribute oa[1];
  oa[0].input(LC);

  typename PtrCredEURes::CountryAttribute country_attr;
  country_attr.input(LC);

  v8 now[8];
  for (size_t i = 0; i < 8; ++i) {
    now[i] = LC.template vinput<8>();
  }

  Q.private_input();

  typename PtrCredEURes::Witness vw;
  vw.input(LC);

  ptrcred.assert_credential(pkX, pkY, htr, oa, country_attr, now, vw);

  auto CIRCUIT = Q.mkcircuit(1);

  EXPECT_TRUE(CIRCUIT != nullptr);
  dump_info("ptrcred_eu_residency", Q);
}

TEST(PtrCredEUResidency, CountryCodesExist) {
  EXPECT_EQ(kNumEUEEACountries, 31);
  EXPECT_STREQ(kEUEEACountryCodes[0], "AT");
  EXPECT_STREQ(kEUEEACountryCodes[10], "DE");
  EXPECT_STREQ(kEUEEACountryCodes[30], "CH");
}

} // namespace
} // namespace proofs
