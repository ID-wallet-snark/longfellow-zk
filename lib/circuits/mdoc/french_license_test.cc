#include "mdoc_attribute_ids.h"
#include "gtest/gtest.h"
#include <set>
#include <string>
#include <vector>

namespace proofs {
namespace {

TEST(FrenchLicenseTest, AllRequiredAttributesAreDefined) {
  std::set<std::string> required_attributes = {
      "family_name",        "given_name",          "birth_date",
      "issue_date",         "expiry_date",         "driving_privileges",
      "category_AM_expiry", "category_A1_expiry",  "category_A2_expiry",
      "category_A_expiry",  "category_B_expiry",   "category_B1_expiry",
      "category_C1_expiry", "category_C_expiry",   "category_D1_expiry",
      "category_D_expiry",  "category_BE_expiry",  "category_C1E_expiry",
      "category_CE_expiry", "category_D1E_expiry", "category_DE_expiry"};

  std::set<std::string> found_attributes;

  for (const auto &attr : kMdocAttributes) {
    if (attr.documentspec == kFRANTSMDL1Namespace) {
      found_attributes.insert(std::string(attr.identifier));
    }
  }

  for (const auto &required : required_attributes) {
    EXPECT_TRUE(found_attributes.count(required))
        << "Missing attribute: " << required << " in namespace "
        << kFRANTSMDL1Namespace;
  }
}

TEST(FrenchLicenseTest, DocTypeIsCorrect) {
  EXPECT_STREQ(kFRDocType, "fr.gouv.ants.mdl.1.permis");
}

TEST(FrenchLicenseTest, NamespaceIsCorrect) {
  EXPECT_STREQ(kFRANTSMDL1Namespace, "fr.gouv.ants.mdl.1");
}

} // namespace
} // namespace proofs
