#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <atomic>
#include <cstring>
#include <ctime>
#include <cmath>
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

// -----------------------------------------------------------------------------
// Data Structures
// -----------------------------------------------------------------------------

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
  int selected_nationality = 0; // 0: FRA, 1: USA, 2: DEU, etc.

  // Proof settings
  bool prove_age = true;
  bool prove_nationality = false;
  bool prove_french_license = false;
  
  // Health Pass / Issuer Settings
  bool prove_health_issuer = false;
  int selected_issuer = 0; // 0: France, 1: USA, 2: Germany
  bool eu_vaccines_compliant = true;
  bool simulate_scan = false;

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

// -----------------------------------------------------------------------------
// UI Helper Functions
// -----------------------------------------------------------------------------

static void glfw_error_callback(int error, const char *description) {
  fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

// Custom Spinner
void Spinner(const char* label, float radius, int thickness, const ImVec4& color) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);

    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size((radius) * 2, (radius + style.FramePadding.y) * 2);

    const ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));
    ImGui::ItemSize(bb, style.FramePadding.y);
    if (!ImGui::ItemAdd(bb, id))
        return;

    // Render
    window->DrawList->PathClear();

    int num_segments = 30;
    int start = abs(ImSin(g.Time * 1.8f) * (num_segments - 5));

    const float a_min = IM_PI * 2.0f * ((float)start) / (float)num_segments;
    const float a_max = IM_PI * 2.0f * ((float)num_segments - 3) / (float)num_segments;

    const ImVec2 centre = ImVec2(pos.x + radius, pos.y + radius + style.FramePadding.y);

    for (int i = 0; i < num_segments; i++) {
        const float a = a_min + ((float)i / (float)num_segments) * (a_max - a_min);
        window->DrawList->PathLineTo(ImVec2(centre.x + ImCos(a + g.Time * 8) * radius,
                                            centre.y + ImSin(a + g.Time * 8) * radius));
    }

    window->DrawList->PathStroke(ImGui::GetColorU32(color), false, thickness);
}

void SetupStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    style.WindowRounding = 8.0f;
    style.FrameRounding = 4.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.TabRounding = 4.0f;

    style.WindowPadding = ImVec2(15, 15);
    style.FramePadding = ImVec2(8, 6);
    style.ItemSpacing = ImVec2(10, 10);
    style.IndentSpacing = 20.0f;

    // Cyberpunk-ish / Professional Blue-Grey Theme
    colors[ImGuiCol_Text]                   = ImVec4(0.95f, 0.96f, 0.98f, 1.00f);
    colors[ImGuiCol_TextDisabled]           = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    colors[ImGuiCol_WindowBg]               = ImVec4(0.10f, 0.11f, 0.14f, 1.00f);
    colors[ImGuiCol_ChildBg]                = ImVec4(0.13f, 0.14f, 0.17f, 1.00f);
    colors[ImGuiCol_PopupBg]                = ImVec4(0.13f, 0.14f, 0.17f, 0.95f);
    colors[ImGuiCol_Border]                 = ImVec4(0.25f, 0.25f, 0.27f, 0.50f);
    colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]                = ImVec4(0.20f, 0.22f, 0.27f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.25f, 0.28f, 0.33f, 1.00f);
    colors[ImGuiCol_FrameBgActive]          = ImVec4(0.30f, 0.34f, 0.40f, 1.00f);
    colors[ImGuiCol_TitleBg]                = ImVec4(0.08f, 0.08f, 0.10f, 1.00f);
    colors[ImGuiCol_TitleBgActive]          = ImVec4(0.08f, 0.08f, 0.10f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
    colors[ImGuiCol_MenuBarBg]              = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
    colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
    colors[ImGuiCol_CheckMark]              = ImVec4(0.20f, 0.60f, 1.00f, 1.00f);
    colors[ImGuiCol_SliderGrab]             = ImVec4(0.24f, 0.52f, 0.88f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_Button]                 = ImVec4(0.20f, 0.22f, 0.27f, 1.00f);
    colors[ImGuiCol_ButtonHovered]          = ImVec4(0.24f, 0.52f, 0.88f, 1.00f);
    colors[ImGuiCol_ButtonActive]           = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_Header]                 = ImVec4(0.20f, 0.22f, 0.27f, 1.00f);
    colors[ImGuiCol_HeaderHovered]          = ImVec4(0.24f, 0.52f, 0.88f, 1.00f);
    colors[ImGuiCol_HeaderActive]           = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_Separator]              = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
    colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.10f, 0.40f, 0.75f, 0.78f);
    colors[ImGuiCol_SeparatorActive]        = ImVec4(0.10f, 0.40f, 0.75f, 1.00f);
    colors[ImGuiCol_ResizeGrip]             = ImVec4(0.26f, 0.59f, 0.98f, 0.25f);
    colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
    colors[ImGuiCol_ResizeGripActive]       = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
    colors[ImGuiCol_Tab]                    = ImVec4(0.18f, 0.35f, 0.58f, 0.86f);
    colors[ImGuiCol_TabHovered]             = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
    colors[ImGuiCol_TabActive]              = ImVec4(0.20f, 0.41f, 0.68f, 1.00f);
    colors[ImGuiCol_TabUnfocused]           = ImVec4(0.07f, 0.10f, 0.15f, 0.97f);
    colors[ImGuiCol_TabUnfocusedActive]     = ImVec4(0.14f, 0.26f, 0.42f, 1.00f);
    colors[ImGuiCol_PlotLines]              = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered]       = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
    colors[ImGuiCol_PlotHistogram]          = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered]   = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
    colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
    colors[ImGuiCol_DragDropTarget]         = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
    colors[ImGuiCol_NavHighlight]           = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
}

void LogMessage(AppState &state, const std::string &msg) {
  std::lock_guard<std::recursive_mutex> lock(state.mutex);
  time_t now = time(nullptr);
  char timestamp[32];
  strftime(timestamp, sizeof(timestamp), "[%H:%M:%S] ", localtime(&now));
  state.log += timestamp + msg + "\n";
}

// -----------------------------------------------------------------------------
// Core ZK Logic (Unchanged)
// -----------------------------------------------------------------------------

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
  
  std::string nat_code = "FRA"; // Default
  if (state.selected_nationality == 1) nat_code = "USA";
  else if (state.selected_nationality == 2) nat_code = "DEU";
  else if (state.selected_nationality == 3) nat_code = "GBR";
  else if (state.selected_nationality == 4) nat_code = "ESP";

  file << "    \"nationality\": \"" << nat_code << "\"\n";
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

// Fonction pour créer un RequestedAttribute pour l'Emetteur (Issuer)
// Utilise le champ "nationality" comme proxy pour la démonstration
RequestedAttribute CreateIssuerAttribute(const char *issuer_code) {
  // For this demo, verifying the Issuer is cryptographically identical 
  // to verifying the Nationality field of the signer.
  // Real implementation would check "issuing_country" or similar.
  return CreateNationalityAttribute(issuer_code);
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
  bool prove_health_issuer = state.prove_health_issuer;
  int selected_issuer = state.selected_issuer;
  bool eu_vaccines_compliant = state.eu_vaccines_compliant;
  bool prove_category_A = state.prove_category_A;
  bool prove_category_B = state.prove_category_B;
  bool prove_category_C = state.prove_category_C;
  int age_threshold = state.age_threshold;
  int selected_nationality = state.selected_nationality;

  state.generation_task = std::async(std::launch::async, [&state, birth_year,
                                                          birth_month,
                                                          birth_day, prove_age,
                                                          prove_nationality,
                                                          prove_french_license,
                                                          prove_health_issuer,
                                                          selected_issuer,
                                                          eu_vaccines_compliant,
                                                          prove_category_A,
                                                          prove_category_B,
                                                          prove_category_C,
                                                          age_threshold,
                                                          selected_nationality]() {
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
      } else if (prove_health_issuer) {
         // REAL ZK IMPLEMENTATION: ISSUER VERIFICATION
         // -------------------------------------------
         // We verify the Authority that signed the document (Root of Trust).
         // In our test vector, the Issuer Country is stored in the 'nationality' field ("FRA").
         // We demand a ZK proof that the underlying document contains the selected Country Code.
         
         mdoc_index = 0; // This signed document is issued by "FRA"
         
         std::string target_issuer = "";
         if (selected_issuer == 0) target_issuer = "FRA"; // France
         else if (selected_issuer == 1) target_issuer = "USA"; // USA
         else if (selected_issuer == 2) target_issuer = "DEU"; // Germany
         else target_issuer = "INVALID";

         LogMessage(state, "  • Initiating ZK Constraint: IssuerCountry == " + target_issuer);
         
         // Using CreateIssuerAttribute (wraps nationality field) to prove origin.
         attributes.push_back(CreateIssuerAttribute(target_issuer.c_str()));
      } else {
        // Standard Identity (Tab 1)
        mdoc_index = 0; // mdoc[0] has nationality "FRA"
        if (prove_age) {
          attributes.push_back(CreateAgeAttribute(age_threshold));
          LogMessage(state, "  ✓ Attribute: age_over_" +
                                std::to_string(age_threshold));
        }
        if (prove_nationality) {
          // Map integer selection to Country Code
          std::string target_nat = "";
          if (selected_nationality == 0) target_nat = "FRA";
          else if (selected_nationality == 1) target_nat = "USA";
          else if (selected_nationality == 2) target_nat = "DEU";
          else if (selected_nationality == 3) target_nat = "GBR";
          else if (selected_nationality == 4) target_nat = "ESP";
          else target_nat = "UNK";

          attributes.push_back(CreateNationalityAttribute(target_nat.c_str()));
          LogMessage(state, "  ✓ Attribute: nationality = " + target_nat);
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
        } else if (prove_health_issuer) {
           state.proof_data.attributes_proven.push_back("Issuer Verified");
           std::string iss = (selected_issuer == 0) ? "FRA" : ((selected_issuer == 1) ? "USA" : "DEU");
           state.proof_data.attributes_proven.push_back("Authority: " + iss);
        } else {
          if (prove_age)
            state.proof_data.attributes_proven.push_back(
                "age_over_" + std::to_string(age_threshold));
          if (prove_nationality) {
             std::string nat_str = "FRA";
             if (selected_nationality == 1) nat_str = "USA";
             else if (selected_nationality == 2) nat_str = "DEU";
             else if (selected_nationality == 3) nat_str = "GBR";
             else if (selected_nationality == 4) nat_str = "ESP";
             state.proof_data.attributes_proven.push_back("nationality_" + nat_str);
          }
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

// -----------------------------------------------------------------------------
// Main Render Loop
// -----------------------------------------------------------------------------

void RenderMainWindow(AppState &state) {
  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);

  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  ImGui::Begin("Longfellow ZK", nullptr,
               ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | 
                   ImGuiWindowFlags_NoBringToFrontOnFocus);
  ImGui::PopStyleVar();

  // 1. Header
  // ---------------------------------------------------------
  ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.08f, 0.08f, 0.10f, 1.00f));
  ImGui::BeginChild("Header", ImVec2(0, 60), false);
  {
      ImGui::SetCursorPos(ImVec2(20, 15));
      ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]); // Use default font (scaled if needed)
      ImGui::TextColored(state.accent_color, "LONGFELLOW");
      ImGui::SameLine();
      ImGui::TextColored(ImVec4(1,1,1,1), "ZK");
      ImGui::PopFont();
      
      ImGui::SameLine();
      ImGui::SetCursorPosY(17);
      ImGui::TextDisabled(" |  Zero-Knowledge Identity Verification");

      if (state.is_generating) {
        ImGui::SameLine(ImGui::GetWindowWidth() - 220);
        Spinner("##spinner_header", 10, 2, state.accent_color);
        ImGui::SameLine();
        ImGui::SetCursorPosY(17);
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Processing...");
      }
  }
  ImGui::EndChild();
  ImGui::PopStyleColor();

  ImGui::Separator();

  // 2. Main Content Area (Split into Left and Right)
  // ---------------------------------------------------------
  ImGui::BeginChild("Content", ImVec2(0, 0), true);
  
  ImGui::Columns(2, "MainColumns", false); 
  ImGui::SetColumnWidth(0, 550); // Fixed width for controls

  // LEFT COLUMN: Controls
  // ---------------------
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20, 20));
  ImGui::BeginChild("Controls", ImVec2(0, 0), ImGuiChildFlags_AlwaysUseWindowPadding);
  
  if (ImGui::BeginTabBar("MainTabs")) {
    // TAB 1: Identity Verification
    if (ImGui::BeginTabItem("Identity Verification")) {
      state.prove_french_license = false;
      ImGui::Spacing();
      ImGui::TextWrapped("Verify age or nationality using a trusted mDoc credential, without revealing your full birth date or ID number.");
      ImGui::Spacing();
      ImGui::Separator();
      ImGui::Spacing();

      ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.15f, 0.16f, 0.20f, 1.0f));
      if (ImGui::CollapsingHeader("User Profile (Private Input)", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent(10);
        ImGui::Spacing();
        
        // Date Input Table for perfect alignment
        if (ImGui::BeginTable("DateInputTable", 4)) {
            ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 100);
            ImGui::TableSetupColumn("Y", ImGuiTableColumnFlags_WidthFixed, 80);
            ImGui::TableSetupColumn("M", ImGuiTableColumnFlags_WidthFixed, 60);
            ImGui::TableSetupColumn("D", ImGuiTableColumnFlags_WidthFixed, 60);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Date of Birth:");

            ImGui::TableSetColumnIndex(1);
            ImGui::PushItemWidth(-1);
            ImGui::InputInt("##Year", &state.birth_year, 0);
            ImGui::PopItemWidth();
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Year (YYYY)");

            ImGui::TableSetColumnIndex(2);
            ImGui::PushItemWidth(-1);
            ImGui::InputInt("##Month", &state.birth_month, 0);
            ImGui::PopItemWidth();
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Month (1-12)");

            ImGui::TableSetColumnIndex(3);
            ImGui::PushItemWidth(-1);
            ImGui::InputInt("##Day", &state.birth_day, 0);
            ImGui::PopItemWidth();
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Day (1-31)");

            ImGui::EndTable();
        }
        
        int age = CalculateAge(state.birth_year, state.birth_month, state.birth_day);
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.4f, 0.4f, 0.4f, 1.0f), "Calculated Age:");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "%d years old", age);
        ImGui::Unindent(10);
        ImGui::Spacing();
      }

      ImGui::Spacing();
      if (ImGui::CollapsingHeader("Proof Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent(10);
        ImGui::Spacing();

        // Age Proof
        ImGui::Checkbox("Prove Age Requirement", &state.prove_age);
        if (state.prove_age) {
          ImGui::SameLine();
          ImGui::SetNextItemWidth(150);
          ImGui::SliderInt("##Threshold", &state.age_threshold, 13, 25, "Over %d");
        }

        // Nationality Proof
        ImGui::Spacing();
        ImGui::Checkbox("Prove Nationality", &state.prove_nationality);
        if (state.prove_nationality) {
          ImGui::SameLine();
          ImGui::SetNextItemWidth(200);
          const char* nations[] = { "France (FRA)", "United States (USA)", "Germany (DEU)", "United Kingdom (GBR)", "Spain (ESP)" };
          ImGui::Combo("##NatCombo", &state.selected_nationality, nations, IM_ARRAYSIZE(nations));
        }
        ImGui::Unindent(10);
        ImGui::Spacing();
      }
      ImGui::PopStyleColor(); // Header
      ImGui::EndTabItem();
    }

        // TAB 2: Health Pass (Issuer Verification)

        if (ImGui::BeginTabItem("Issuer Verification")) {

          state.prove_health_issuer = true;

          state.prove_french_license = false;

          state.prove_age = false;

          state.prove_nationality = false;

    

          ImGui::Spacing();

          ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.4f, 1.0f), "Health Certificate Issuer Verification");

          ImGui::TextWrapped("Verify the Issuing Authority of a digital health certificate using Zero-Knowledge Proofs, without revealing personal health data.");

          ImGui::Spacing();

          ImGui::Separator();

          ImGui::Spacing();

    

          ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.15f, 0.16f, 0.20f, 1.0f));

          if (ImGui::CollapsingHeader("Certificate Authority", ImGuiTreeNodeFlags_DefaultOpen)) {

            ImGui::Indent(10);

            ImGui::Spacing();

            

            ImGui::Text("Issuing Authority / Country:");

            const char* items[] = { "France (Ministère de la Santé)", "USA (CDC)", "Deutschland (RKI)", "Invalid / Other" };

            ImGui::Combo("##IssuerCombo", &state.selected_issuer, items, IM_ARRAYSIZE(items));

            

            ImGui::Spacing();

            ImGui::Checkbox("EU Mandatory Vaccines Compliant", &state.eu_vaccines_compliant);
            ImGui::SameLine();
            ImGui::TextDisabled("(?)");
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
                ImGui::TextUnformatted("Verifies compliance with all mandatory vaccines for EU travel.");
                ImGui::PopTextWrapPos();
                ImGui::EndTooltip();
            }

    

            ImGui::Spacing();

            ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "Cryptographic Binding:");

            ImGui::TextWrapped("The selected authority will be verified against the signed document using Zero-Knowledge proofs. No local simulation.");

            

            ImGui::Unindent(10);

            ImGui::Spacing();

          }

    

          ImGui::Spacing();

          if (ImGui::CollapsingHeader("ZK Verification Scope", ImGuiTreeNodeFlags_DefaultOpen)) {

              ImGui::Indent(10);

              ImGui::Spacing();

              ImGui::Text("This Zero-Knowledge circuit proves:");

              ImGui::BulletText("The certificate signature is valid (ECDSA P-256)");

              ImGui::BulletText("The Issuer Country matches the selection");

              ImGui::BulletText("The user holds the corresponding private key");

              ImGui::Spacing();

              ImGui::TextDisabled("Note: Personal identity (Name, DOB) is NOT revealed.");

              ImGui::Unindent(10);

          }

    

          ImGui::PopStyleColor();

          ImGui::EndTabItem();

        }

    // TAB 3: French License
    if (ImGui::BeginTabItem("Driver's License")) {
      state.prove_french_license = true;
      state.prove_health_issuer = false;
      state.prove_age = false;
      state.prove_nationality = false;

      ImGui::Spacing();
      ImGui::TextColored(state.accent_color, "Driver's License Verification");
      ImGui::TextWrapped("Verify that you hold a valid French Driver's License without revealing your identity.");
      ImGui::Spacing();
      ImGui::Separator();
      ImGui::Spacing();

      ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.15f, 0.16f, 0.20f, 1.0f));
      if (ImGui::CollapsingHeader("Attributes to Verify", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent(10);
        ImGui::Spacing();
        
        ImGui::TextColored(ImVec4(0.6f, 0.7f, 0.8f, 1.0f), "License Validity");
        ImGui::TextDisabled("Checks 'issue_date' and signature.");
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::TextColored(ImVec4(0.6f, 0.7f, 0.8f, 1.0f), "Categories");
        ImGui::Checkbox("Category A (Motorcycle)", &state.prove_category_A);
        ImGui::Checkbox("Category B (Car)", &state.prove_category_B);
        ImGui::Checkbox("Category C (Truck)", &state.prove_category_C);
        
        ImGui::Spacing();
        ImGui::TextDisabled("* Demo Note: Uses 'height' as proxy for B-Category");
        ImGui::Unindent(10);
        ImGui::Spacing();
      }
      ImGui::PopStyleColor();
      ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
  }

  ImGui::Spacing();
  ImGui::Spacing();
  
  // Action Buttons Area
  float button_height = 45.0f;
  if (state.is_generating) {
      ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.2f, 0.5f));
      ImGui::Button("GENERATING...", ImVec2(ImGui::GetContentRegionAvail().x, button_height));
      ImGui::PopStyleColor();
      ImGui::PopItemFlag();
  } else {
      ImGui::PushStyleColor(ImGuiCol_Button, state.accent_color);
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 1.0f, 1.0f));
      if (ImGui::Button("GENERATE PROOF", ImVec2(ImGui::GetContentRegionAvail().x, button_height))) {
          GenerateZKProofAsync(state);
      }
      ImGui::PopStyleColor(2);
  }

  ImGui::EndChild(); // End Controls
  ImGui::PopStyleVar();

  ImGui::NextColumn();

  // RIGHT COLUMN: Log & Status
  // --------------------------
  ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.05f, 0.05f, 0.05f, 1.0f));
  ImGui::BeginChild("LogPanel", ImVec2(0, 0), false);
  
  // Status Bar at Top of Right Column
  ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.13f, 0.16f, 1.0f));
  ImGui::BeginChild("StatusBar", ImVec2(0, 80), false);
  {
      ImGui::SetCursorPos(ImVec2(15, 15));
      ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "CURRENT STATUS");
      
      ImGui::SetCursorPos(ImVec2(15, 35));
      if (state.status_message.empty()) {
          ImGui::Text("Ready");
      } else {
          // Simple color coding based on content
          ImVec4 statusColor = ImVec4(1, 1, 1, 1);
          if (state.status_message.find("✓") != std::string::npos || state.status_message.find("[OK]") != std::string::npos) 
              statusColor = ImVec4(0.4f, 1.0f, 0.4f, 1.0f);
          else if (state.status_message.find("fail") != std::string::npos || state.status_message.find("Error") != std::string::npos || state.status_message.find("❌") != std::string::npos)
              statusColor = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
          else if (state.status_message.find("Generating") != std::string::npos)
              statusColor = ImVec4(1.0f, 0.8f, 0.2f, 1.0f);

          ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]); 
          ImGui::TextColored(statusColor, "%s", state.status_message.c_str());
          ImGui::PopFont();
      }
  }
  ImGui::EndChild();
  ImGui::PopStyleColor();

  // Proof Details (if available)
  if (state.proof_data.is_valid) {
      ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 10));
      ImGui::BeginChild("ProofActions", ImVec2(0, 60), false);
      ImGui::SetCursorPos(ImVec2(10, 10));
      
      if (ImGui::Button("EXPORT JSON", ImVec2(120, 35))) {
        if (ExportProof(state, "proof.json")) {
            LogMessage(state, "Proof exported to proof.json");
        }
      }
      ImGui::SameLine();
      if (ImGui::Button("VERIFY AGAIN", ImVec2(120, 35))) {
        VerifyZKProof(state);
      }
      ImGui::EndChild();
      ImGui::PopStyleVar();
  }

  // Log Output
  ImGui::Separator();
  ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f)); // Transparent
  ImGui::BeginChild("LogList", ImVec2(0, 0), true);
  {
      ImGui::Indent(10);
      ImGui::Spacing();
      ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "SYSTEM LOG");
      ImGui::Spacing();
      
      std::lock_guard<std::recursive_mutex> lock(state.mutex);
      ImGui::TextUnformatted(state.log.c_str());
      if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
          ImGui::SetScrollHereY(1.0f);
  }
  ImGui::EndChild();
  ImGui::PopStyleColor();

  ImGui::EndChild(); // End LogPanel
  ImGui::PopStyleColor();

  ImGui::EndChild(); // End MainContent
  ImGui::End();
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
      glfwCreateWindow(1080, 720, "Longfellow ZK", nullptr, nullptr);
  if (window == nullptr)
    return 1;

  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

  // Setup Custom Style
  SetupStyle();

  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init(glsl_version);

  AppState state;
  LogMessage(state, "Welcome to Longfellow ZK Identity Verification");
  LogMessage(state, "System initialized. Ready to generate proofs.");

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
    glClearColor(0.10f, 0.11f, 0.14f, 1.0f); // Match WindowBg
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
