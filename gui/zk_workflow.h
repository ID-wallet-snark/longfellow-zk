#ifndef ZK_WORKFLOW_H
#define ZK_WORKFLOW_H

#include <vector>
#include <string>
#include <ctime>
#include <cstdint>

// ZK Lib definitions
#include "circuits/mdoc/mdoc_zk.h"

// -----------------------------------------------------------------------------
// Data Structures
// -----------------------------------------------------------------------------

struct CountryEntry {
  const char* name;
  const char* alpha3;
  const char* numeric;
};

extern const CountryEntry kCountries[];
extern const int kNumCountries;

struct ProofData {
  std::vector<uint8_t> zkproof;
  bool is_valid = false;
  std::string proof_hash;
  std::vector<std::string> attributes_proven;
  time_t timestamp = 0;
  size_t circuit_size = 0;
  // Stocker le circuit pour la vérification
  std::vector<uint8_t> circuit_data;
  size_t circuit_len = 0;
  // Stocker les attributs utilisés
  std::vector<RequestedAttribute> attributes;
  // Index of the mock mdoc used (0 for Age, 3 for French License)
  int mdoc_test_index = 0;
};

// Configuration struct to pass from UI to Logic without passing the whole AppState
struct ProverConfig {
    int birth_year;
    int birth_month;
    int birth_day;
    bool prove_age;
    bool prove_nationality;
    bool prove_french_license;
    bool prove_health_issuer;
    bool prove_vaccine;
    bool prove_insurance;
    int selected_issuer;
    bool eu_vaccines_compliant;
    bool prove_category_A;
    bool prove_category_B;
    bool prove_category_C;
    int age_threshold;
    int selected_nationality;
    
    // Cache references (pointers to cache in AppState)
    // In a full refactor, cache should be managed by the workflow too, 
    // but we'll keep it simple for now.
    void* circuit_cache_1attr; 
    void* circuit_cache_2attr;
};

// -----------------------------------------------------------------------------
// Function Declarations
// -----------------------------------------------------------------------------

// Attribute Helpers
RequestedAttribute CreateAgeAttribute(int age_threshold);
RequestedAttribute CreateNationalityAttribute(const char *nationality);
RequestedAttribute CreateIssuerAttribute(const char *issuer_code);
RequestedAttribute CreateVaccineAttribute(const char *vaccine_code);
RequestedAttribute CreateInsuranceAttribute(const char *status);

// Logic Helpers
int CalculateAge(int birth_year, int birth_month, int birth_day);
bool ExportProof(const ProofData &proof_data, const ProverConfig &config, const std::string &filename);

// Core ZK Operations
// Returns true if successful, fills proof_out and status_out
bool PerformZKProofGeneration(const ProverConfig& config, 
                              ProofData& proof_out, 
                              std::string& log_out, 
                              int& calculated_age_out);

bool PerformZKVerification(const ProofData& proof_data, std::string& log_out);

#endif // ZK_WORKFLOW_H
