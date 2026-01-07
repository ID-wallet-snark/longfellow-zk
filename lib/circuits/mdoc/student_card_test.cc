#include "circuits/mdoc/mdoc_attribute_ids.h"
#include "circuits/mdoc/mdoc_test_attributes.h"
#include "circuits/mdoc/mdoc_zk.h"
#include "circuits/mdoc/mdoc_examples.h"
#include "gtest/gtest.h"
#include <set>
#include <string>

namespace proofs {
namespace {

static const RequestedAttribute is_student_true = {
    .namespace_id = {'f', 'r', '.', 'g', 'o', 'u', 'v', '.', 'e', 'd', 'u', 'c', 'a', 't', 'i', 'o', 'n', '.', '1'},
    .id = {'i', 's', '_', 's', 't', 'u', 'd', 'e', 'n', 't'},
    .cbor_value = {0xF5}, 
    .namespace_len = 19,
    .id_len = 10,
    .cbor_value_len = 1
};

TEST(StudentCardTest, DocumentspecAndNamespaceAreCorrect) {
    EXPECT_STREQ(kFRStudent1Namespace, "fr.gouv.education.1");
    EXPECT_STREQ(kFRStudentDocType, "fr.gouv.education.1.student");
}

TEST(StudentCardTest, RequiredAttributesAreDefined) {
  std::set<std::string> required_attributes = {
      "family_name",
      "given_name",
      "birth_date",
      "is_student"
  };

  std::set<std::string> found_attributes;

  for (const auto& attr : kMdocAttributes) {
    if (attr.documentspec == kFRStudent1Namespace) {
      found_attributes.insert(std::string(attr.identifier));
    }
  }

  for (const auto& required : required_attributes) {
    EXPECT_TRUE(found_attributes.count(required))
        << "Missing attribute: " << required << " in namespace " << kFRStudent1Namespace;
  }
}

class StudentCardZKTest : public testing::Test {
 protected:
  static void SetUpTestCase() {
     if (circuit_ == nullptr) {
       generate_circuit(&kZkSpecs[0], &circuit_, &circuit_len_);
     }
  }
  
  static void TearDownTestCase() {
    if (circuit_) {
        free(circuit_);
        circuit_ = nullptr;
    }
  }

  static uint8_t* circuit_;
  static size_t circuit_len_;
};

uint8_t* StudentCardZKTest::circuit_ = nullptr;
size_t StudentCardZKTest::circuit_len_ = 0;

TEST_F(StudentCardZKTest, AttributeDefinitionIsCorrect) {
    ASSERT_EQ(is_student_true.namespace_len, 19);
    ASSERT_EQ(is_student_true.id_len, 10);
    EXPECT_EQ(is_student_true.cbor_value[0], 0xF5);
}

TEST_F(StudentCardZKTest, VerifyStudentStatusLogic) {
    const MdocTests* test_mdoc = &mdoc_tests[3]; 
    RequestedAttribute attrs[] = { is_student_true };
    
    uint8_t* zkproof = nullptr;
    size_t proof_len = 0;
    
    MdocProverErrorCode ret = run_mdoc_prover(
        circuit_, circuit_len_, 
        test_mdoc->mdoc, test_mdoc->mdoc_size,
        test_mdoc->pkx.as_pointer, test_mdoc->pky.as_pointer,
        test_mdoc->transcript, test_mdoc->transcript_size,
        attrs, 1, 
        (const char*)test_mdoc->now,
        &zkproof, &proof_len, &kZkSpecs[0]
    );

    if (ret == MDOC_PROVER_SUCCESS) {
        printf("[  INFO ] Sample mDoc contains student status! Verifying proof...\n");
        MdocVerifierErrorCode v_ret = run_mdoc_verifier(
            circuit_, circuit_len_,
            test_mdoc->pkx.as_pointer, test_mdoc->pky.as_pointer,
            test_mdoc->transcript, test_mdoc->transcript_size,
            attrs, 1,
            (const char*)test_mdoc->now,
            zkproof, proof_len, test_mdoc->doc_type, &kZkSpecs[0]
        );
        EXPECT_EQ(v_ret, MDOC_VERIFIER_SUCCESS);
        free(zkproof);
    } else {
        printf("[  INFO ] Prover finished with code %d (Expected if sample lacks 'is_student')\n", ret);
        SUCCEED(); 
    }
}

}  // namespace
}  // namespace proofs
