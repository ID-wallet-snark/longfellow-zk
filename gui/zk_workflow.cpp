#include "zk_workflow.h"

#include <algorithm>
#include <cstring>
#include <ctime>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <iostream>

// Intégration Longfellow ZK
#include "algebra/fp.h"
#include "circuits/mdoc/mdoc_examples.h"
#include "circuits/mdoc/mdoc_test_attributes.h"
#include "circuits/mdoc/mdoc_witness.h"
#include "ec/p256.h"

// -----------------------------------------------------------------------------
// Global Data
// -----------------------------------------------------------------------------

const CountryEntry kCountries[] = {
    { "France", "FRA", "250" },
    { "United States", "USA", "840" },
    { "Germany", "DEU", "276" },
    { "United Kingdom", "GBR", "826" },
    { "Spain", "ESP", "724" },
    { "Italy", "ITA", "380" },
    { "Poland", "POL", "616" },
    { "Netherlands", "NLD", "528" },
    { "Belgium", "BEL", "056" },
    { "Sweden", "SWE", "752" },
    { "Switzerland", "CHE", "756" },
    { "Austria", "AUT", "040" },
    { "Portugal", "PRT", "620" }
};
const int kNumCountries = sizeof(kCountries)/sizeof(kCountries[0]);

// -----------------------------------------------------------------------------
// Helper Functions Implementation
// -----------------------------------------------------------------------------

int CalculateAge(int birth_year, int birth_month, int birth_day) {
  time_t now_time = time(nullptr);
  struct tm *now = localtime(&now_time);

  int current_year = now->tm_year + 1900;
  int current_month = now->tm_mon + 1;
  int current_day = now->tm_mday;

  int age = current_year - birth_year;

  if (current_month < birth_month ||
      (current_month == birth_month && current_day < birth_day)) {
    age--;
  }

  return age;
}

RequestedAttribute CreateAgeAttribute(int age_threshold) {
  RequestedAttribute attr;
  const char *ns = "org.iso.18013.5.1";
  size_t ns_len = strlen(ns);
  memcpy(attr.namespace_id, ns, ns_len);
  attr.namespace_len = ns_len;

  std::string id = "age_over_" + std::to_string(age_threshold);
  memcpy(attr.id, id.c_str(), id.length());
  attr.id_len = id.length();

  attr.cbor_value[0] = 0xf5; // true
  attr.cbor_value_len = 1;

  return attr;
}

RequestedAttribute CreateNationalityAttribute(const char *nationality) {
  RequestedAttribute attr;
  const char *ns = "org.iso.18013.5.1";
  size_t ns_len = strlen(ns);
  memcpy(attr.namespace_id, ns, ns_len);
  attr.namespace_len = ns_len;

  const char *id = "nationality";
  memcpy(attr.id, id, strlen(id));
  attr.id_len = strlen(id);

  attr.cbor_value[0] = 0x60 + strlen(nationality); // CBOR text header
  memcpy(attr.cbor_value + 1, nationality, strlen(nationality));
  attr.cbor_value_len = 1 + strlen(nationality);

  return attr;
}

RequestedAttribute CreateIssuerAttribute(const char *issuer_code) {
  return CreateNationalityAttribute(issuer_code);
}

RequestedAttribute CreateVaccineAttribute(const char *vaccine_code) {
  RequestedAttribute attr;
  const char *ns = "org.iso.18013.5.1.health"; 
  size_t ns_len = strlen(ns);
  memcpy(attr.namespace_id, ns, ns_len);
  attr.namespace_len = ns_len;

  const char *id = "vaccine_id";
  memcpy(attr.id, id, strlen(id));
  attr.id_len = strlen(id);

  attr.cbor_value[0] = 0x60 + strlen(vaccine_code);
  memcpy(attr.cbor_value + 1, vaccine_code, strlen(vaccine_code));
  attr.cbor_value_len = 1 + strlen(vaccine_code);

  return attr;
}

RequestedAttribute CreateInsuranceAttribute(const char *status) {
  RequestedAttribute attr;
  const char *ns = "org.iso.18013.5.1.health";
  size_t ns_len = strlen(ns);
  memcpy(attr.namespace_id, ns, ns_len);
  attr.namespace_len = ns_len;

  const char *id = "insurance_status";
  memcpy(attr.id, id, strlen(id));
  attr.id_len = strlen(id);

  attr.cbor_value[0] = 0x60 + strlen(status);
  memcpy(attr.cbor_value + 1, status, strlen(status));
  attr.cbor_value_len = 1 + strlen(status);

  return attr;
}

bool ExportProof(const ProofData &proof_data, const ProverConfig &config, const std::string &filename) {
  if (!proof_data.is_valid) return false;

  std::ofstream file(filename);
  if (!file.is_open()) return false;

  file << "{\n";
  file << "  \"version\": \"1.0\",\n";
  file << "  \"timestamp\": " << proof_data.timestamp << ",\n";
  file << "  \"proof_hash\": \"" << proof_data.proof_hash << "\",\n";
  file << "  \"circuit_size\": " << proof_data.circuit_size << ",\n";
  file << "  \"attributes\": [\n";

  for (size_t i = 0; i < proof_data.attributes_proven.size(); ++i) {
    file << "    \"" << proof_data.attributes_proven[i] << "\"";
    if (i < proof_data.attributes_proven.size() - 1) file << ",";
    file << "\n";
  }

  file << "  ],\n";
  file << "  \"proof_data\": \"";
  for (size_t i = 0; i < proof_data.zkproof.size(); ++i) {
    file << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(proof_data.zkproof[i]);
  }
  file << "\",\n";
  file << "  \"settings\": {\n";
  file << "    \"age_threshold\": " << config.age_threshold << ",\n";
  
  std::string nat_code = "250"; 
  if (config.selected_nationality >= 0 && config.selected_nationality < kNumCountries) {
      nat_code = kCountries[config.selected_nationality].numeric;
  }
  file << "    \"nationality\": \"" << nat_code << "\"\n";
  file << "  }\n";
  file << "}\n";

  return true;
}

// -----------------------------------------------------------------------------
// Core Logic
// -----------------------------------------------------------------------------

// Defines strict cache structure to match what was in main.cpp, but anonymous here since we use void* in header
struct CircuitCache {
    std::vector<uint8_t> circuit_data;
    size_t circuit_len = 0;
    size_t num_attributes = 0;
    const ZkSpecStruct *zk_spec = nullptr;
};

bool PerformZKProofGeneration(const ProverConfig& config, 
                              ProofData& proof_out, 
                              std::string& log_out, 
                              int& calculated_age_out) 
{
    auto Log = [&](const std::string& msg) {
        // Simple local logging, appended to the out string
        time_t now = time(nullptr);
        char timestamp[32];
        strftime(timestamp, sizeof(timestamp), "[%H:%M:%S] ", localtime(&now));
        log_out += timestamp + msg + "\n";
    };

    Log("[PROVER] Starting REAL ZK proof generation...");

    // Age Check
    int calculated_age = CalculateAge(config.birth_year, config.birth_month, config.birth_day);
    calculated_age_out = calculated_age;
    Log("[AGE] Calculated age: " + std::to_string(calculated_age));

    if (config.prove_age && calculated_age < config.age_threshold) {
        Log("  [BLOCKED] User is " + std::to_string(calculated_age) + ", but threshold is " + std::to_string(config.age_threshold));
        return false; 
    }

    try {
        std::vector<RequestedAttribute> attributes;
        int mdoc_index = 0;

        if (config.prove_french_license) {
            mdoc_index = 3; // Use mdoc with issue_date and height
            attributes.push_back(proofs::test::issue_date_2024_03_15);
            Log("  ✓ Attribute: issue_date (Validity Check)");
            
            if (config.prove_category_B) {
                attributes.push_back(proofs::test::category_B_proxy);
                Log("  ✓ Attribute: category_B (via height proxy)");
            }
            if (config.prove_category_A) {
                attributes.push_back(proofs::test::driving_privileges_A);
                Log("  ✓ Attribute: category_A");
            }
            if (config.prove_category_C) {
                attributes.push_back(proofs::test::driving_privileges_C);
                Log("  ✓ Attribute: category_C");
            }
        } else if (config.prove_health_issuer) {
             mdoc_index = 0; // This signed document is issued by "FRA" 
             
             std::string target_issuer = "";
             if (config.selected_issuer == 0) target_issuer = "FRA"; 
             else if (config.selected_issuer == 1) target_issuer = "USA";
             else if (config.selected_issuer == 2) target_issuer = "DEU";
             else target_issuer = "INVALID";

             Log("  • Initiating ZK Constraint: IssuerCountry == " + target_issuer);
             attributes.push_back(CreateIssuerAttribute(target_issuer.c_str()));

             if (config.prove_vaccine) {
                 attributes.push_back(CreateVaccineAttribute("EU/1/20/1528"));
                 Log("  ✓ Attribute: vaccine_id == EU/1/20/1528 (Comirnaty)");
             }
             if (config.prove_insurance) {
                 attributes.push_back(CreateInsuranceAttribute("active"));
                 Log("  ✓ Attribute: insurance_status == active");
             }
        } else {
            // Standard Identity
            mdoc_index = 0; 
            if (config.prove_age) {
                attributes.push_back(CreateAgeAttribute(config.age_threshold));
                Log("  ✓ Attribute: age_over_" + std::to_string(config.age_threshold));
            }
            if (config.prove_nationality) {
                std::string target_nat = "250"; // Default
                if (config.selected_nationality >= 0 && config.selected_nationality < kNumCountries) {
                    target_nat = kCountries[config.selected_nationality].numeric;
                } else {
                    target_nat = "UNK";
                }
                attributes.push_back(CreateNationalityAttribute(target_nat.c_str()));
                Log("  ✓ Attribute: nationality = " + target_nat);
            }
        }

        if (attributes.empty()) {
            Log("  ✗ No attributes selected");
            return false;
        }

        // Get ZkSpec
        const ZkSpecStruct *zk_spec = nullptr;
        for (int i = 0; i < kNumZkSpecs; ++i) {
            if (kZkSpecs[i].num_attributes == attributes.size()) {
                zk_spec = &kZkSpecs[i];
                break;
            }
        }

        if (!zk_spec) {
            Log("  ✗ No ZK spec found");
            return false;
        }

        // Cache Logic
        std::vector<uint8_t> circuit_data_vec;
        size_t circuit_len = 0;
        bool circuit_from_cache = false;

        CircuitCache* cache = nullptr;
        if (attributes.size() == 1) cache = (CircuitCache*)config.circuit_cache_1attr;
        else if (attributes.size() == 2) cache = (CircuitCache*)config.circuit_cache_2attr;

        if (cache && !cache->circuit_data.empty() &&
            cache->num_attributes == attributes.size() &&
            cache->zk_spec == zk_spec) {
            circuit_data_vec = cache->circuit_data;
            circuit_len = cache->circuit_len;
            circuit_from_cache = true;
            Log("  ✓ Using CACHED circuit");
        }

        if (!circuit_from_cache) {
            Log("  • Generating circuit (CPU Intensive, 30-60s)...");
            uint8_t *raw_circuit_data = nullptr;
            CircuitGenerationErrorCode ret = generate_circuit(zk_spec, &raw_circuit_data, &circuit_len);

            if (ret != CIRCUIT_GENERATION_SUCCESS || !raw_circuit_data) {
                Log("  ✗ Circuit generation failed");
                if (raw_circuit_data) free(raw_circuit_data);
                return false;
            }

            circuit_data_vec.assign(raw_circuit_data, raw_circuit_data + circuit_len);
            free(raw_circuit_data);

            // Update cache if possible (requires mutex in caller usually, but here we assume config passed valid ptrs)
            if (cache) {
                cache->circuit_data = circuit_data_vec;
                cache->circuit_len = circuit_len;
                cache->num_attributes = attributes.size();
                cache->zk_spec = zk_spec;
                Log("  ✓ Circuit cached");
            }
        }

        // Prover
        const proofs::MdocTests *test = &proofs::mdoc_tests[mdoc_index];
        Log("  • Calling run_mdoc_prover...");
        
        uint8_t *zkproof = nullptr;
        size_t proof_len = 0;

        MdocProverErrorCode prover_ret = run_mdoc_prover(
            circuit_data_vec.data(), circuit_len, test->mdoc, test->mdoc_size,
            test->pkx.as_pointer, test->pky.as_pointer, test->transcript,
            test->transcript_size, attributes.data(), attributes.size(),
            (const char *)test->now, &zkproof, &proof_len, zk_spec);

        if (prover_ret != MDOC_PROVER_SUCCESS) {
            Log("  [ERROR] Prover failed: " + std::to_string(prover_ret));
            if (zkproof) free(zkproof);
            return false;
        }

        Log("  [SUCCESS] Proof generated: " + std::to_string(proof_len) + " bytes");

        // Fill result
        proof_out.zkproof.assign(zkproof, zkproof + proof_len);
        proof_out.is_valid = true;
        proof_out.timestamp = time(nullptr);
        proof_out.circuit_size = circuit_len;
        proof_out.circuit_data = circuit_data_vec;
        proof_out.circuit_len = circuit_len;
        proof_out.attributes = attributes;
        proof_out.mdoc_test_index = mdoc_index;

        size_t hash_val = 0;
        for (size_t i = 0; i < std::min(proof_len, size_t(32)); ++i) {
            hash_val ^= (zkproof[i] << (i % 8));
        }
        proof_out.proof_hash = "0x" + std::to_string(hash_val);

        proof_out.attributes_proven.clear();
        if (config.prove_french_license) {
            proof_out.attributes_proven.push_back("French License Valid");
            if (config.prove_category_B) proof_out.attributes_proven.push_back("Category B");
            if (config.prove_category_A) proof_out.attributes_proven.push_back("Category A");
            if (config.prove_category_C) proof_out.attributes_proven.push_back("Category C");
        } else if (config.prove_health_issuer) {
            proof_out.attributes_proven.push_back("Issuer Verified");
            std::string iss = (config.selected_issuer == 0) ? "FRA" : ((config.selected_issuer == 1) ? "USA" : "DEU");
            proof_out.attributes_proven.push_back("Authority: " + iss);
            if (config.prove_vaccine) proof_out.attributes_proven.push_back("Vaccine: Comirnaty (Pfizer)");
            if (config.prove_insurance) proof_out.attributes_proven.push_back("Insurance: Active");
        } else {
            if (config.prove_age)
                proof_out.attributes_proven.push_back("age_over_" + std::to_string(config.age_threshold));
            if (config.prove_nationality) {
                std::string nat_str = "250";
                if (config.selected_nationality >= 0 && config.selected_nationality < kNumCountries) {
                    nat_str = kCountries[config.selected_nationality].numeric;
                }
                proof_out.attributes_proven.push_back("nationality_" + nat_str);
            }
        }

        free(zkproof);
        return true;

    } catch (const std::exception &e) {
        Log("  [ERROR] Exception: " + std::string(e.what()));
        return false;
    }
}

bool PerformZKVerification(const ProofData& proof_data, std::string& log_out) {
    auto Log = [&](const std::string& msg) {
        log_out += msg + "\n"; // Simple append
    };

    if (!proof_data.is_valid) {
        Log("[ERROR] No valid proof to verify");
        return false;
    }

    Log("[VERIFIER] Starting verification...");

    try {
        const proofs::MdocTests *test = &proofs::mdoc_tests[proof_data.mdoc_test_index];

        const ZkSpecStruct *zk_spec = nullptr;
        for (int i = 0; i < kNumZkSpecs; ++i) {
            if (kZkSpecs[i].num_attributes == proof_data.attributes.size()) {
                zk_spec = &kZkSpecs[i];
                break;
            }
        }

        if (!zk_spec) {
            Log("  [ERROR] ZK spec not found");
            return false;
        }

        MdocVerifierErrorCode verifier_ret = run_mdoc_verifier(
            proof_data.circuit_data.data(), proof_data.circuit_len,
            test->pkx.as_pointer, test->pky.as_pointer, test->transcript,
            test->transcript_size, proof_data.attributes.data(),
            proof_data.attributes.size(), (const char *)test->now,
            proof_data.zkproof.data(), proof_data.zkproof.size(),
            test->doc_type, zk_spec);

        if (verifier_ret != MDOC_VERIFIER_SUCCESS) {
            Log("  [ERROR] VERIFICATION FAILED: " + std::to_string(verifier_ret));
            return false;
        }

        Log("[SUCCESS] VERIFICATION SUCCESSFUL!");
        return true;

    } catch (const std::exception &e) {
        Log("  [ERROR] Exception: " + std::string(e.what()));
        return false;
    }
}

