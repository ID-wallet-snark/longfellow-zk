#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <atomic>
#include <cstring>
#include <ctime>
#include <fstream>
#include <future>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

// Intégration Longfellow ZK
#include "algebra/fp.h"
#include "circuits/mdoc/mdoc_examples.h"
#include "circuits/mdoc/mdoc_test_attributes.h"
#include "circuits/mdoc/mdoc_witness.h"
#include "circuits/mdoc/mdoc_zk.h"
#include "ec/p256.h"
#include "util/log.h"

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

struct AppState {
  // User input
  int birth_year = 2005;
  int birth_month = 1;
  int birth_day = 1;
  char nationality[64] = "FRA"; // Code ISO 3166-1 alpha-3

  // Proof settings
  bool prove_age = true;
  bool prove_nationality = false;
  bool prove_french_license = false;
  
  // License Categories
  bool prove_category_A = false;
  bool prove_category_B = true;
  bool prove_category_C = false;

  int age_threshold = 18;

  // Calculated age based on birth date
  int calculated_age = 19;

  // Status
  std::string status_message;
  ProofData proof_data;

  // Output
  std::string log;
  std::recursive_mutex mutex; // Protects log and proof_data during async ops

  // Async state
  std::atomic<bool> is_generating{false};
  std::future<void> generation_task;

  // Style
  ImVec4 accent_color = ImVec4(0.2f, 0.6f, 1.0f, 1.0f);

  // Circuit cache (pour éviter de régénérer à chaque fois)
  struct CircuitCache {
    std::vector<uint8_t> circuit_data;
    size_t circuit_len = 0;
    size_t num_attributes = 0;
    const ZkSpecStruct *zk_spec = nullptr;
  };
  CircuitCache circuit_cache_1attr;
  CircuitCache circuit_cache_2attr;
};

static void glfw_error_callback(int error, const char *description) {
  fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

void LogMessage(AppState &state, const std::string &msg) {
  std::lock_guard<std::recursive_mutex> lock(state.mutex);
  time_t now = time(nullptr);
  char timestamp[32];
  strftime(timestamp, sizeof(timestamp), "[%H:%M:%S] ", localtime(&now));
  state.log += timestamp + msg + "\n";
}

// Fonction pour exporter une preuve en JSON
bool ExportProof(AppState &state, const std::string &filename) {
  std::lock_guard<std::recursive_mutex> lock(state.mutex);
  if (!state.proof_data.is_valid) {
    return false;
  }

  std::ofstream file(filename);
  if (!file.is_open()) {
    return false;
  }

  file << "{\n";
  file << "  \"version\": \"1.0\",\n";
  file << "  \"timestamp\": " << state.proof_data.timestamp << ",\n";
  file << "  \"proof_hash\": \"" << state.proof_data.proof_hash << "\",\n";
  file << "  \"circuit_size\": " << state.proof_data.circuit_size << ",\n";
  file << "  \"attributes\": [\n";

  for (size_t i = 0; i < state.proof_data.attributes_proven.size(); ++i) {
    file << "    \"" << state.proof_data.attributes_proven[i] << "\"";
    if (i < state.proof_data.attributes_proven.size() - 1) {
      file << ",";
    }
    file << "\n";
  }

  file << "  ],\n";
  file << "  \"proof_data\": \"";

  // Encoder la preuve en hexadécimal
  for (size_t i = 0; i < state.proof_data.zkproof.size(); ++i) {
    file << std::hex << std::setw(2) << std::setfill('0')
         << static_cast<int>(state.proof_data.zkproof[i]);
  }

  file << "\",\n";
  file << "  \"settings\": {\n";
  file << "    \"age_threshold\": " << state.age_threshold << ",\n";
  file << "    \"prove_age\": " << (state.prove_age ? "true" : "false")
       << ",\n";
  file << "    \"prove_nationality\": "
       << (state.prove_nationality ? "true" : "false") << ",\n";
  file << "    \"nationality\": \"" << state.nationality << "\"\n";
  file << "  }\n";
  file << "}\n";

  file.close();
  return true;
}

// Fonction pour créer un RequestedAttribute pour l'âge
RequestedAttribute CreateAgeAttribute(int age_threshold) {
  RequestedAttribute attr;

  // Namespace: org.iso.18013.5.1
  const char *ns = "org.iso.18013.5.1";
  size_t ns_len = strlen(ns);
  memcpy(attr.namespace_id, ns, ns_len);
  attr.namespace_len = ns_len;

  // ID: age_over_18 (adaptable selon le threshold)
  std::string id = "age_over_" + std::to_string(age_threshold);
  memcpy(attr.id, id.c_str(), id.length());
  attr.id_len = id.length();

  // CBOR value: true (0xf5)
  attr.cbor_value[0] = 0xf5;
  attr.cbor_value_len = 1;

  return attr;
}

// Fonction pour créer un RequestedAttribute pour la nationalité
RequestedAttribute CreateNationalityAttribute(const char *nationality) {
  RequestedAttribute attr;

  // Namespace: org.iso.18013.5.1
  const char *ns = "org.iso.18013.5.1";
  size_t ns_len = strlen(ns);
  memcpy(attr.namespace_id, ns, ns_len);
  attr.namespace_len = ns_len;

  // ID: nationality
  const char *id = "nationality";
  memcpy(attr.id, id, strlen(id));
  attr.id_len = strlen(id);

  // CBOR value: text string for nationality (e.g., "FRA")
  // Format: 0x63 (text(3)) + "FRA"
  attr.cbor_value[0] = 0x60 + strlen(nationality); // CBOR text header
  memcpy(attr.cbor_value + 1, nationality, strlen(nationality));
  attr.cbor_value_len = 1 + strlen(nationality);

  return attr;
}

// Calculer l'âge actuel à partir de la date de naissance
int CalculateAge(int birth_year, int birth_month, int birth_day) {
  time_t now_time = time(nullptr);
  struct tm *now = localtime(&now_time);

  int current_year = now->tm_year + 1900;
  int current_month = now->tm_mon + 1;
  int current_day = now->tm_mday;

  int age = current_year - birth_year;

  // Ajuster si l'anniversaire n'est pas encore passé cette année
  if (current_month < birth_month ||
      (current_month == birth_month && current_day < birth_day)) {
    age--;
  }

  return age;
}

// VRAIE génération de preuve ZK avec Longfellow - appelle run_mdoc_prover
void GenerateZKProofAsync(AppState &state) {
  state.is_generating = true;
  state.status_message = "Generating proof... (This may take 30-60s)";

  // Capture necessary state by value to avoid race conditions
  int birth_year = state.birth_year;
  int birth_month = state.birth_month;
  int birth_day = state.birth_day;
  bool prove_age = state.prove_age;
  bool prove_nationality = state.prove_nationality;
  bool prove_french_license = state.prove_french_license;
  bool prove_category_A = state.prove_category_A;
  bool prove_category_B = state.prove_category_B;
  bool prove_category_C = state.prove_category_C;
  int age_threshold = state.age_threshold;
  std::string nationality = state.nationality;

  state.generation_task = std::async(std::launch::async, [&state, birth_year,
                                                          birth_month,
                                                          birth_day, prove_age,
                                                          prove_nationality,
                                                          prove_french_license,
                                                          prove_category_A,
                                                          prove_category_B,
                                                          prove_category_C,
                                                          age_threshold,
                                                          nationality]() {
    LogMessage(state, "[PROVER] Starting REAL ZK proof generation (Async)...");

    // Calculer l'âge actuel
    int calculated_age = CalculateAge(birth_year, birth_month, birth_day);
    {
      std::lock_guard<std::recursive_mutex> lock(state.mutex);
      state.calculated_age = calculated_age;
    }
    LogMessage(state, "[AGE] Calculated age from input: " +
                          std::to_string(calculated_age) + " years");

    // DYNAMIC LOGIC: Check age locally before running the heavy circuit
    if (prove_age && calculated_age < age_threshold) {
      LogMessage(state,
                 "  [BLOCKED] User is " + std::to_string(calculated_age) +
                     ", but threshold is " + std::to_string(age_threshold));
      LogMessage(
          state,
          "  [BLOCKED] Proof generation aborted to prevent false claim.");
      {
        std::lock_guard<std::recursive_mutex> lock(state.mutex);
        state.status_message = "❌ Verification Failed: User is under " +
                               std::to_string(age_threshold);
        state.is_generating = false;
      }
      return;
    }

    try {
      std::vector<RequestedAttribute> attributes;
      int mdoc_index = 0;

      if (prove_french_license) {
        mdoc_index = 3; // Use mdoc with issue_date and height
        attributes.push_back(proofs::test::issue_date_2024_03_15);
        LogMessage(state, "  ✓ Attribute: issue_date (Validity Check)");
        
        if (prove_category_B) {
            attributes.push_back(proofs::test::category_B_proxy);
            LogMessage(state, "  ✓ Attribute: category_B (via height proxy)");
        }
        if (prove_category_A) {
            attributes.push_back(proofs::test::driving_privileges_A);
            LogMessage(state, "  ✓ Attribute: category_A (driving_privileges)");
        }
        if (prove_category_C) {
            attributes.push_back(proofs::test::driving_privileges_C);
            LogMessage(state, "  ✓ Attribute: category_C (driving_privileges)");
        }
      } else {
        // For Age/Nationality, use mdoc_tests[0] (Valid > 18)
        // We only proceed here if the LOCAL check passed, ensuring consistency.
        mdoc_index = 0;
        if (prove_age) {
          attributes.push_back(CreateAgeAttribute(age_threshold));
          LogMessage(state, "  ✓ Attribute: age_over_" +
                                std::to_string(age_threshold));
        }
        if (prove_nationality) {
          attributes.push_back(CreateNationalityAttribute(nationality.c_str()));
          LogMessage(state, "  ✓ Attribute: nationality = " + nationality);
        }
      }

      if (attributes.empty()) {
        LogMessage(state, "  ✗ No attributes selected");
        state.is_generating = false;
        return;
      }

      // Récupération du ZkSpec
      const ZkSpecStruct *zk_spec = nullptr;
      for (int i = 0; i < kNumZkSpecs; ++i) {
        if (kZkSpecs[i].num_attributes == attributes.size()) {
          zk_spec = &kZkSpecs[i];
          break;
        }
      }

      if (!zk_spec) {
        LogMessage(state, "  ✗ No ZK spec found");
        state.is_generating = false;
        return;
      }

      // Vérifier cache
      std::vector<uint8_t> circuit_data_vec;
      size_t circuit_len = 0;
      bool circuit_from_cache = false;

      // Access cache safely
      {
        std::lock_guard<std::recursive_mutex> lock(state.mutex);
        AppState::CircuitCache *cache = nullptr;
        if (attributes.size() == 1)
          cache = &state.circuit_cache_1attr;
        else if (attributes.size() == 2)
          cache = &state.circuit_cache_2attr;

        if (cache && !cache->circuit_data.empty() &&
            cache->num_attributes == attributes.size() &&
            cache->zk_spec == zk_spec) {
          circuit_data_vec = cache->circuit_data;
          circuit_len = cache->circuit_len;
          circuit_from_cache = true;
          LogMessage(state, "  ✓ Using CACHED circuit");
        }
      }

      if (!circuit_from_cache) {
        LogMessage(state, "  • Generating circuit (CPU Intensive, 30-60s)...");
        // Note: This step is blocking on the worker thread and uses high CPU
        uint8_t *raw_circuit_data = nullptr;
        CircuitGenerationErrorCode ret =
            generate_circuit(zk_spec, &raw_circuit_data, &circuit_len);

        if (ret != CIRCUIT_GENERATION_SUCCESS || !raw_circuit_data) {
          LogMessage(state, "  ✗ Circuit generation failed");
          if (raw_circuit_data)
            free(raw_circuit_data);
          state.is_generating = false;
          return;
        }

        // Copy to vector and free raw pointer immediately
        circuit_data_vec.assign(raw_circuit_data,
                                raw_circuit_data + circuit_len);
        free(raw_circuit_data);

        // Update cache
        {
          std::lock_guard<std::recursive_mutex> lock(state.mutex);
          AppState::CircuitCache *cache = nullptr;
          if (attributes.size() == 1)
            cache = &state.circuit_cache_1attr;
          else if (attributes.size() == 2)
            cache = &state.circuit_cache_2attr;

          if (cache) {
            cache->circuit_data = circuit_data_vec;
            cache->circuit_len = circuit_len;
            cache->num_attributes = attributes.size();
            cache->zk_spec = zk_spec;
            LogMessage(state, "  ✓ Circuit cached");
          }
        }
      }

      // Prepare Prover
      const proofs::MdocTests *test = &proofs::mdoc_tests[mdoc_index];

      LogMessage(state, "  • Calling run_mdoc_prover...");
      uint8_t *zkproof = nullptr;
      size_t proof_len = 0;

      MdocProverErrorCode prover_ret = run_mdoc_prover(
          circuit_data_vec.data(), circuit_len, test->mdoc, test->mdoc_size,
          test->pkx.as_pointer, test->pky.as_pointer, test->transcript,
          test->transcript_size, attributes.data(), attributes.size(),
          (const char *)test->now, &zkproof, &proof_len, zk_spec);

      if (prover_ret != MDOC_PROVER_SUCCESS) {
        LogMessage(state,
                   "  [ERROR] Prover failed: " + std::to_string(prover_ret));
        if (zkproof)
          free(zkproof);
        state.is_generating = false;
        return;
      }

      LogMessage(state, "  [SUCCESS] Proof generated: " +
                            std::to_string(proof_len) + " bytes");

      // Update Proof Data safely
      {
        std::lock_guard<std::recursive_mutex> lock(state.mutex);
        state.proof_data.zkproof.assign(zkproof, zkproof + proof_len);
        state.proof_data.is_valid = true;
        state.proof_data.timestamp = time(nullptr);
        state.proof_data.circuit_size = circuit_len;
        state.proof_data.circuit_data = circuit_data_vec; // Copy vector
        state.proof_data.circuit_len = circuit_len;
        state.proof_data.attributes = attributes;
        state.proof_data.mdoc_test_index = mdoc_index;

        // Hash
        size_t hash_val = 0;
        for (size_t i = 0; i < std::min(proof_len, size_t(32)); ++i) {
          hash_val ^= (zkproof[i] << (i % 8));
        }
        state.proof_data.proof_hash = "0x" + std::to_string(hash_val);

        state.proof_data.attributes_proven.clear();
        if (prove_french_license) {
          state.proof_data.attributes_proven.push_back("French License Valid");
          if (prove_category_B) state.proof_data.attributes_proven.push_back("Category B");
          if (prove_category_A) state.proof_data.attributes_proven.push_back("Category A");
          if (prove_category_C) state.proof_data.attributes_proven.push_back("Category C");
        } else {
          if (prove_age)
            state.proof_data.attributes_proven.push_back(
                "age_over_" + std::to_string(age_threshold));
          if (prove_nationality)
            state.proof_data.attributes_proven.push_back("nationality_" +
                                                         nationality);
        }

        state.status_message = "✓ Proof generated successfully";
      }

      free(zkproof);

    } catch (const std::exception &e) {
      LogMessage(state, "  [ERROR] Exception: " + std::string(e.what()));
      state.is_generating = false;
    }

    state.is_generating = false;
  });
}

// VRAIE vérification de preuve ZK - appelle run_mdoc_verifier
bool VerifyZKProof(AppState &state) {
  std::lock_guard<std::recursive_mutex> lock(state.mutex);
  if (!state.proof_data.is_valid) {
    LogMessage(state, "[ERROR] No valid proof to verify");
    return false;
  }

  LogMessage(state, "[VERIFIER] Starting verification...");

  try {
    const proofs::MdocTests *test =
        &proofs::mdoc_tests[state.proof_data.mdoc_test_index];

    const ZkSpecStruct *zk_spec = nullptr;
    for (int i = 0; i < kNumZkSpecs; ++i) {
      if (kZkSpecs[i].num_attributes == state.proof_data.attributes.size()) {
        zk_spec = &kZkSpecs[i];
        break;
      }
    }

    if (!zk_spec) {
      LogMessage(state, "  [ERROR] ZK spec not found");
      return false;
    }

    MdocVerifierErrorCode verifier_ret = run_mdoc_verifier(
        state.proof_data.circuit_data.data(), state.proof_data.circuit_len,
        test->pkx.as_pointer, test->pky.as_pointer, test->transcript,
        test->transcript_size, state.proof_data.attributes.data(),
        state.proof_data.attributes.size(), (const char *)test->now,
        state.proof_data.zkproof.data(), state.proof_data.zkproof.size(),
        test->doc_type, zk_spec);

    if (verifier_ret != MDOC_VERIFIER_SUCCESS) {
      LogMessage(state, "  [ERROR] VERIFICATION FAILED: " +
                            std::to_string(verifier_ret));
      state.status_message = "Verification failed";
      return false;
    }

    LogMessage(state, "[SUCCESS] VERIFICATION SUCCESSFUL!");
    state.status_message = "[OK] Proof verified successfully";
    return true;

  } catch (const std::exception &e) {
    LogMessage(state, "  [ERROR] Exception: " + std::string(e.what()));
    return false;
  }
}

void RenderMainWindow(AppState &state) {
  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);

  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20, 20));
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 8));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10, 10));

  ImGui::Begin("Longfellow ZK", nullptr,
               ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);

  // Header
  ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
  ImGui::TextColored(state.accent_color, "Longfellow ZK");
  ImGui::PopFont();
  ImGui::SameLine();
  ImGui::TextDisabled("| Zero-Knowledge Identity Verification");

  if (state.is_generating) {
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f),
                       " [GENERATING PROOF...]");
  }

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  if (ImGui::BeginTabBar("MainTabs")) {

    // TAB 1: Identity Verification
    if (ImGui::BeginTabItem("Identity Verification")) {
      state.prove_french_license = false;

      ImGui::Spacing();
      ImGui::Text(
          "Verify your age or nationality without revealing personal data.");
      ImGui::Spacing();

      ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.2f, 0.3f, 0.4f, 1.0f));
      if (ImGui::CollapsingHeader("User Profile (Private Data)",
                                  ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent(20);
        ImGui::Text("Birth Date:");
        ImGui::Spacing();

        // Group date inputs for better layout
        ImGui::BeginGroup();

        // Year
        ImGui::PushItemWidth(120);
        ImGui::InputInt("Year", &state.birth_year, 0); // 0 step removes buttons
        ImGui::PopItemWidth();

        ImGui::SameLine();
        ImGui::Text("-");
        ImGui::SameLine();

        // Month
        ImGui::PushItemWidth(100);
        ImGui::InputInt("Month", &state.birth_month, 0);
        ImGui::PopItemWidth();

        ImGui::SameLine();
        ImGui::Text("-");
        ImGui::SameLine();

        // Day
        ImGui::PushItemWidth(100);
        ImGui::InputInt("Day", &state.birth_day, 0);
        ImGui::PopItemWidth();

        ImGui::EndGroup();

        ImGui::Spacing();
        int age =
            CalculateAge(state.birth_year, state.birth_month, state.birth_day);
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "Calculated Age: %d",
                           age);
        ImGui::Unindent(20);
      }

      ImGui::Spacing();

      if (ImGui::CollapsingHeader("Proof Settings",
                                  ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent(20);

        // Age Proof
        ImGui::Checkbox("Prove Age", &state.prove_age);
        if (ImGui::IsItemHovered())
          ImGui::SetTooltip("Enable to prove you are over a certain age.");

        if (state.prove_age) {
          ImGui::SameLine();
          ImGui::SetNextItemWidth(150);
          ImGui::SliderInt("Threshold", &state.age_threshold, 13, 25);
          if (ImGui::IsItemHovered())
            ImGui::SetTooltip(
                "Minimum age to verify against (e.g., 18 or 21).");
        }

        // Nationality Proof
        ImGui::Checkbox("Prove Nationality", &state.prove_nationality);
        if (ImGui::IsItemHovered())
          ImGui::SetTooltip(
              "Enable to prove you belong to a specific country.");

        if (state.prove_nationality) {
          ImGui::SameLine();
          ImGui::SetNextItemWidth(100);
          ImGui::InputText("Code", state.nationality,
                           sizeof(state.nationality));
          if (ImGui::IsItemHovered())
            ImGui::SetTooltip(
                "ISO 3166-1 alpha-3 country code (e.g., FRA, USA).");
        }
        ImGui::Unindent(20);
      }
      ImGui::PopStyleColor();
      ImGui::EndTabItem();
    }

    // TAB 2: French License
    if (ImGui::BeginTabItem("French Driver's License")) {
      state.prove_french_license = true;
      state.prove_age = false;
      state.prove_nationality = false;

      ImGui::Spacing();
      ImGui::TextColored(state.accent_color,
                         "French Driver's License Verification");
      ImGui::Separator();
      ImGui::Spacing();

      ImGui::TextWrapped(
          "This mode verifies that you hold a valid French Driver's License "
          "(Category B) without revealing your identity.");
      ImGui::Spacing();

      ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.2f, 0.3f, 0.4f, 1.0f));
      if (ImGui::CollapsingHeader("Attributes to Verify",
                                  ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent(20);
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "1. Validity Check");
        ImGui::SameLine();
        ImGui::TextDisabled("(Issue Date)");
        
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "2. License Categories");
        
        ImGui::Checkbox("Category A (Motorcycle)", &state.prove_category_A);
        ImGui::Checkbox("Category B (Car)", &state.prove_category_B);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Uses 'height' as a proxy for demo purposes.");
        ImGui::Checkbox("Category C (Truck)", &state.prove_category_C);

        ImGui::Spacing();
        ImGui::TextDisabled("Note: Only Category B is supported by the current mdoc (via proxy).");
        ImGui::Unindent(20);
      }
      ImGui::PopStyleColor();
      ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
  }

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  // Action Buttons
  if (state.is_generating) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
    ImGui::Button("   Generating Proof...   ", ImVec2(300, 50));
    ImGui::PopStyleColor();

    // Spinner
    ImGui::SameLine();
    ImGui::TextColored(state.accent_color, " | ");
    ImGui::SameLine();
    float time = (float)ImGui::GetTime();
    int num_dots = (int)(time * 3.0f) % 4;
    std::string dots = "";
    for (int i = 0; i < num_dots; ++i)
      dots += ".";
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Working%s",
                       dots.c_str());
  } else {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 1.0f, 1.0f));
    if (ImGui::Button("GENERATE PROOF", ImVec2(300, 50))) {
      GenerateZKProofAsync(state);
    }
    ImGui::PopStyleColor();
  }

  ImGui::SameLine();

  if (!state.is_generating) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.8f, 0.4f, 1.0f));
    if (ImGui::Button("VERIFY PROOF", ImVec2(300, 50))) {
      VerifyZKProof(state);
    }
    ImGui::PopStyleColor();
  }

  ImGui::SameLine();
  if (ImGui::Button("Clear Log", ImVec2(100, 50))) {
    std::lock_guard<std::recursive_mutex> lock(state.mutex);
    state.log.clear();
    state.status_message.clear();
  }

  // Export and Verify Buttons (only if proof exists)
  if (state.proof_data.is_valid) {
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::Button("EXPORT PROOF", ImVec2(145, 40))) {
      if (ExportProof(state, "proof.json")) {
        LogMessage(state, "Proof exported to proof.json");
      } else {
        LogMessage(state, "Failed to export proof");
      }
    }
    ImGui::SameLine();
    if (ImGui::Button("VERIFY PROOF", ImVec2(145, 40))) {
      VerifyZKProof(state);
    }
  }

  // Status Message
  ImGui::Spacing();
  if (!state.status_message.empty()) {
    std::lock_guard<std::recursive_mutex> lock(state.mutex);
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Status: %s",
                       state.status_message.c_str());
  }

  // Log Window
  ImGui::Spacing();
  ImGui::Text("Activity Log:");
  ImGui::BeginChild("Log", ImVec2(0, 200), true);
  {
    std::lock_guard<std::recursive_mutex> lock(state.mutex);
    ImGui::TextUnformatted(state.log.c_str());
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
      ImGui::SetScrollHereY(1.0f);
  }
  ImGui::EndChild();

  ImGui::End();
  ImGui::PopStyleVar(3);
}

int main(int, char **) {
  proofs::set_log_level(proofs::INFO);

  glfwSetErrorCallback(glfw_error_callback);
  if (!glfwInit())
    return 1;

  const char *glsl_version = "#version 330";
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  GLFWwindow *window =
      glfwCreateWindow(1024, 800, "Longfellow ZK", nullptr, nullptr);
  if (window == nullptr)
    return 1;

  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

  ImGui::StyleColorsDark();
  ImGuiStyle &style = ImGui::GetStyle();
  style.WindowRounding = 8.0f;
  style.FrameRounding = 6.0f;

  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init(glsl_version);

  AppState state;
  LogMessage(state, "Welcome to Longfellow ZK Identity Verification");

  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    RenderMainWindow(state);

    ImGui::Render();
    int display_w, display_h;
    glfwGetFramebufferSize(window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(window);
  }

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  glfwDestroyWindow(window);
  glfwTerminate();

  return 0;
}
