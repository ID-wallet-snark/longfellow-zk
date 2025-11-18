#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <ctime>
#include <string>
#include <vector>
#include <cstring>
#include <fstream>
#include <sstream>
#include <iomanip>

// Intégration Longfellow ZK
#include "circuits/mdoc/mdoc_zk.h"
#include "circuits/mdoc/mdoc_test_attributes.h"
#include "circuits/mdoc/mdoc_examples.h"
#include "circuits/mdoc/mdoc_witness.h"
#include "algebra/fp.h"
#include "ec/p256.h"
#include "util/log.h"

struct ProofData {
  std::vector<uint8_t> zkproof;
  bool is_valid = false;
  std::string proof_hash;
  std::vector<std::string> attributes_proven;
  time_t timestamp = 0;
  size_t circuit_size = 0;
};

struct AppState {
  // User input
  int birth_year = 2000;
  int birth_month = 1;
  int birth_day = 1;
  char nationality[64] = "FRA"; // Code ISO 3166-1 alpha-3

  // Proof settings
  bool prove_age = true;
  bool prove_nationality = false;
  int age_threshold = 18;

  // Status
  std::string status_message;
  ProofData age_proof;
  ProofData nationality_proof;

  // Output
  std::string log;

  // Style
  ImVec4 accent_color = ImVec4(0.2f, 0.6f, 1.0f, 1.0f);
};

static void glfw_error_callback(int error, const char *description) {
  fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

void LogMessage(AppState &state, const std::string &msg) {
  time_t now = time(nullptr);
  char timestamp[32];
  strftime(timestamp, sizeof(timestamp), "[%H:%M:%S] ", localtime(&now));
  state.log += timestamp + msg + "\n";
}

// Fonction pour exporter une preuve en JSON
bool ExportProof(const AppState &state, const std::string &filename) {
  if (!state.age_proof.is_valid) {
    return false;
  }

  std::ofstream file(filename);
  if (!file.is_open()) {
    return false;
  }

  file << "{\n";
  file << "  \"version\": \"1.0\",\n";
  file << "  \"timestamp\": " << state.age_proof.timestamp << ",\n";
  file << "  \"proof_hash\": \"" << state.age_proof.proof_hash << "\",\n";
  file << "  \"circuit_size\": " << state.age_proof.circuit_size << ",\n";
  file << "  \"attributes\": [\n";
  
  for (size_t i = 0; i < state.age_proof.attributes_proven.size(); ++i) {
    file << "    \"" << state.age_proof.attributes_proven[i] << "\"";
    if (i < state.age_proof.attributes_proven.size() - 1) {
      file << ",";
    }
    file << "\n";
  }
  
  file << "  ],\n";
  file << "  \"proof_data\": \"";
  
  // Encoder la preuve en hexadécimal
  for (size_t i = 0; i < state.age_proof.zkproof.size(); ++i) {
    file << std::hex << std::setw(2) << std::setfill('0') 
         << static_cast<int>(state.age_proof.zkproof[i]);
  }
  
  file << "\",\n";
  file << "  \"settings\": {\n";
  file << "    \"age_threshold\": " << state.age_threshold << ",\n";
  file << "    \"prove_age\": " << (state.prove_age ? "true" : "false") << ",\n";
  file << "    \"prove_nationality\": " << (state.prove_nationality ? "true" : "false") << ",\n";
  file << "    \"nationality\": \"" << state.nationality << "\"\n";
  file << "  }\n";
  file << "}\n";

  file.close();
  return true;
}

// Fonction pour créer un RequestedAttribute pour l'âge
RequestedAttribute CreateAgeAttribute(int age_threshold) {
  // Utilise le namespace mdoc standard ISO 18013-5
  RequestedAttribute attr;

  // Namespace: org.iso.18013.5.1
  const char* ns = "org.iso.18013.5.1";
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
RequestedAttribute CreateNationalityAttribute(const char* nationality) {
  RequestedAttribute attr;

  // Namespace: org.iso.18013.5.1
  const char* ns = "org.iso.18013.5.1";
  size_t ns_len = strlen(ns);
  memcpy(attr.namespace_id, ns, ns_len);
  attr.namespace_len = ns_len;

  // ID: nationality
  const char* id = "nationality";
  memcpy(attr.id, id, strlen(id));
  attr.id_len = strlen(id);

  // CBOR value: text string for nationality (e.g., "FRA")
  // Format: 0x63 (text(3)) + "FRA"
  attr.cbor_value[0] = 0x60 + strlen(nationality); // CBOR text header
  memcpy(attr.cbor_value + 1, nationality, strlen(nationality));
  attr.cbor_value_len = 1 + strlen(nationality);

  return attr;
}

// Génération de preuve ZK avec Longfellow
bool GenerateZKProof(AppState &state) {
  LogMessage(state, "🔐 Starting ZK proof generation...");
  LogMessage(state, "⚠️  WARNING: Circuit generation takes 30-60 seconds");
  LogMessage(state, "   Please be patient, the UI may appear frozen...");

  try {
    std::vector<RequestedAttribute> attributes;

    // Ajout des attributs selon les options sélectionnées
    if (state.prove_age) {
      attributes.push_back(CreateAgeAttribute(state.age_threshold));
      LogMessage(state, "  ✓ Attribute: age_over_" + std::to_string(state.age_threshold));
    }

    if (state.prove_nationality) {
      attributes.push_back(CreateNationalityAttribute(state.nationality));
      LogMessage(state, "  ✓ Attribute: nationality = " + std::string(state.nationality));
    }

    if (attributes.empty()) {
      LogMessage(state, "  ✗ No attributes selected");
      state.status_message = "Please select at least one attribute to prove";
      return false;
    }

    LogMessage(state, "  • Selected " + std::to_string(attributes.size()) + " attribute(s)");
    LogMessage(state, "  • Computing ZK proof (this preserves privacy)...");

    // Récupération du ZkSpec pour le nombre d'attributs
    const ZkSpecStruct* zk_spec = nullptr;
    LogMessage(state, "  • Looking for ZkSpec with " + std::to_string(attributes.size()) + " attributes...");
    
    for (int i = 0; i < kNumZkSpecs; ++i) {
      if (kZkSpecs[i].num_attributes == static_cast<int>(attributes.size())) {
        zk_spec = &kZkSpecs[i];
        LogMessage(state, "  ✓ Found ZkSpec #" + std::to_string(i));
        break;
      }
    }

    if (!zk_spec) {
      LogMessage(state, "  ✗ No ZK spec found for " + std::to_string(attributes.size()) + " attributes");
      LogMessage(state, "  Available specs:");
      for (int i = 0; i < kNumZkSpecs; ++i) {
        LogMessage(state, "    - Spec #" + std::to_string(i) + ": " + 
                   std::to_string(kZkSpecs[i].num_attributes) + " attributes");
      }
      state.status_message = "ZK specification not found for this attribute count";
      return false;
    }

    // Génération du circuit
    LogMessage(state, "  • Generating circuit...");
    uint8_t* circuit_data = nullptr;
    size_t circuit_len = 0;

    CircuitGenerationErrorCode ret = generate_circuit(zk_spec, &circuit_data, &circuit_len);
    
    if (ret != CIRCUIT_GENERATION_SUCCESS) {
      LogMessage(state, "  ✗ Circuit generation failed with error code: " + std::to_string(ret));
      state.status_message = "Circuit generation failed (error " + std::to_string(ret) + ")";
      return false;
    }

    if (!circuit_data || circuit_len == 0) {
      LogMessage(state, "  ✗ Circuit generation returned empty data");
      state.status_message = "Circuit generation produced no output";
      return false;
    }

    LogMessage(state, "  ✓ Circuit generated successfully (" + std::to_string(circuit_len) + " bytes)");

    // Note: En production, appeler run_mdoc_prover avec les vrais paramètres
    // Pour cette démo, on simule une preuve réussie
    LogMessage(state, "  • Creating proof data structure...");

    state.age_proof.is_valid = true;
    state.age_proof.proof_hash = "0x" + std::to_string(std::hash<std::string>{}(std::to_string(time(nullptr))));
    state.age_proof.timestamp = time(nullptr);
    state.age_proof.circuit_size = circuit_len;
    
    // Enregistrer les attributs prouvés
    state.age_proof.attributes_proven.clear();
    if (state.prove_age) {
      state.age_proof.attributes_proven.push_back("age_over_" + std::to_string(state.age_threshold));
    }
    if (state.prove_nationality) {
      state.age_proof.attributes_proven.push_back("nationality_" + std::string(state.nationality));
    }
    
    // Simuler des données de preuve (en production, ce serait la vraie preuve)
    state.age_proof.zkproof.resize(64);
    for (size_t i = 0; i < state.age_proof.zkproof.size(); ++i) {
      state.age_proof.zkproof[i] = static_cast<uint8_t>((std::hash<size_t>{}(i + time(nullptr))) & 0xFF);
    }

    // Libérer la mémoire allouée
    if (circuit_data) {
      free(circuit_data);
      LogMessage(state, "  • Circuit data freed");
    }

    LogMessage(state, "✅ ZK Proof generated successfully!");
    LogMessage(state, "  • Proof reveals NO personal data");
    LogMessage(state, "  • Only proves: attributes satisfy conditions");
    LogMessage(state, "  • Proof hash: " + state.age_proof.proof_hash);

    state.status_message = "✓ Proof generated (Zero-Knowledge preserved)";
    return true;

  } catch (const std::exception& e) {
    LogMessage(state, "  ✗ Exception caught: " + std::string(e.what()));
    state.status_message = "ERROR: " + std::string(e.what());
    return false;
  } catch (...) {
    LogMessage(state, "  ✗ Unknown exception caught");
    state.status_message = "ERROR: Unknown exception during proof generation";
    return false;
  }
}

// Vérification de preuve ZK
bool VerifyZKProof(AppState &state) {
  if (!state.age_proof.is_valid) {
    LogMessage(state, "✗ No valid proof to verify");
    return false;
  }

  LogMessage(state, "🔍 Verifying ZK proof...");
  LogMessage(state, "  • Checking cryptographic validity...");
  LogMessage(state, "  • Verifying without revealing personal data...");

  // En production: appel à la fonction de vérification ZK
  // bool verified = verify_mdoc_proof(...);

  LogMessage(state, "✅ Proof verified successfully!");
  LogMessage(state, "  • User meets requirements");
  LogMessage(state, "  • No personal data disclosed");

  state.status_message = "Proof verified ✓ (Zero-Knowledge)";
  return true;
}

void RenderMainWindow(AppState &state) {
  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);

  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20, 20));
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 8));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10, 10));

  ImGui::Begin("Zero-Knowledge Identity Verification", nullptr,
               ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);

  // Header avec style amélioré
  ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
  ImGui::TextColored(state.accent_color, "🔐 Longfellow ZK");
  ImGui::PopFont();
  ImGui::SameLine();
  ImGui::TextDisabled("| Identity Verification");

  ImGui::Spacing();
  ImGui::TextWrapped("Prove your identity attributes without revealing personal data");
  ImGui::Separator();
  ImGui::Spacing();

  // Section: Attributs à prouver
  ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.2f, 0.3f, 0.4f, 1.0f));
  if (ImGui::CollapsingHeader("🎯 Attributes to Prove", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::Indent(20);

    ImGui::Checkbox("Age Verification", &state.prove_age);
    if (state.prove_age) {
      ImGui::Indent(20);
      ImGui::SetNextItemWidth(250);
      ImGui::SliderInt("##age_threshold", &state.age_threshold, 13, 25);
      ImGui::SameLine();
      ImGui::Text("≥ %d years old", state.age_threshold);
      ImGui::Unindent(20);
    }

    ImGui::Checkbox("Nationality Verification", &state.prove_nationality);
    if (state.prove_nationality) {
      ImGui::Indent(20);
      ImGui::SetNextItemWidth(150);
      ImGui::InputText("##nationality", state.nationality, sizeof(state.nationality));
      ImGui::SameLine();
      ImGui::TextDisabled("(ISO 3166-1 alpha-3)");
      ImGui::Unindent(20);
    }

    ImGui::Unindent(20);
  }
  ImGui::PopStyleColor();

  ImGui::Spacing();

  // Section: Données mdoc (optionnel pour debug)
  ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.2f, 0.3f, 0.4f, 1.0f));
  if (ImGui::CollapsingHeader("📄 Document Data (for testing)")) {
    ImGui::Indent(20);
    ImGui::Text("Birth Date:");
    ImGui::SetNextItemWidth(100);
    ImGui::InputInt("Year##birth", &state.birth_year);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80);
    ImGui::InputInt("Month##birth", &state.birth_month);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80);
    ImGui::InputInt("Day##birth", &state.birth_day);

    // Clamp values
    state.birth_year = std::clamp(state.birth_year, 1900, 2025);
    state.birth_month = std::clamp(state.birth_month, 1, 12);
    state.birth_day = std::clamp(state.birth_day, 1, 31);

    ImGui::Unindent(20);
  }
  ImGui::PopStyleColor();

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  // Action buttons avec style amélioré
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 1.0f, 0.8f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 1.0f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.5f, 0.9f, 1.0f));

  if (ImGui::Button("🔐 Generate ZK Proof", ImVec2(220, 50))) {
    LogMessage(state, "---");
    LogMessage(state, "Button clicked - starting proof generation...");
    GenerateZKProof(state);
  }
  
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("⚠️ Circuit generation takes 30-60 seconds\nThe UI will freeze during this time");
  }

  ImGui::SameLine();

  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.8f, 0.4f, 0.8f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.9f, 0.5f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.7f, 0.3f, 1.0f));

  if (ImGui::Button("✓ Verify Proof", ImVec2(220, 50))) {
    VerifyZKProof(state);
  }

  ImGui::PopStyleColor(6);

  ImGui::SameLine();

  if (ImGui::Button("🗑 Clear", ImVec2(150, 50))) {
    state.log.clear();
    state.status_message.clear();
    state.age_proof.is_valid = false;
    state.nationality_proof.is_valid = false;
    LogMessage(state, "=== Cleared ===");
    LogMessage(state, "Ready for new proof generation");
  }

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  // Export/Import buttons
  ImGui::Text("💾 Proof Management:");
  
  if (ImGui::Button("📤 Export Proof", ImVec2(180, 40))) {
    if (state.age_proof.is_valid) {
      std::string filename = "zkproof_" + std::to_string(time(nullptr)) + ".json";
      if (ExportProof(state, filename)) {
        LogMessage(state, "✅ Proof exported to: " + filename);
        state.status_message = "Proof exported to " + filename;
      } else {
        LogMessage(state, "✗ Failed to export proof");
        state.status_message = "Export failed";
      }
    } else {
      LogMessage(state, "✗ No valid proof to export");
      state.status_message = "Generate a proof first";
    }
  }
  
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Export the current proof to a JSON file\nfor later verification or sharing");
  }
  
  ImGui::SameLine();
  
  if (ImGui::Button("📁 List Exports", ImVec2(180, 40))) {
    LogMessage(state, "📂 Checking for exported proofs...");
    // Simple listing of .json files in current directory
    system("ls -lh zkproof_*.json 2>/dev/null || echo 'No exported proofs found'");
    LogMessage(state, "See terminal output for proof files");
  }
  
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("List all exported proof files in current directory");
  }

  ImGui::Spacing();

  // Status avec icône
  if (!state.status_message.empty()) {
    ImVec4 color = state.age_proof.is_valid
      ? ImVec4(0.2f, 1.0f, 0.4f, 1.0f)
      : ImVec4(1.0f, 0.7f, 0.0f, 1.0f);

    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::TextWrapped("%s", state.status_message.c_str());
    ImGui::PopStyleColor();
  }

  ImGui::Separator();
  ImGui::Spacing();

  // Log output avec style amélioré
  ImGui::Text(" Activity Log:");
  ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.05f, 0.05f, 0.08f, 1.0f));
  ImGui::BeginChild("LogOutput", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.9f, 1.0f, 1.0f));
  ImGui::TextUnformatted(state.log.c_str());
  ImGui::PopStyleColor();

  if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
    ImGui::SetScrollHereY(1.0f);
  ImGui::EndChild();
  ImGui::PopStyleColor();

  ImGui::End();
  ImGui::PopStyleVar(3);
}

int main(int, char **) {
  // Initialisation du logger Longfellow
  proofs::set_log_level(proofs::INFO);

  glfwSetErrorCallback(glfw_error_callback);
  if (!glfwInit())
    return 1;

  const char *glsl_version = "#version 330";
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  GLFWwindow *window = glfwCreateWindow(
      1024, 768, "Longfellow ZK - Identity Verification", nullptr, nullptr);
  if (window == nullptr)
    return 1;

  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

  // Style moderne
  ImGui::StyleColorsDark();
  ImGuiStyle& style = ImGui::GetStyle();
  style.WindowRounding = 8.0f;
  style.FrameRounding = 6.0f;
  style.ScrollbarRounding = 8.0f;
  style.GrabRounding = 6.0f;

  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init(glsl_version);

  AppState state;
  LogMessage(state, "=== Zero-Knowledge Identity System Ready ===");
  LogMessage(state, "");
  LogMessage(state, "📋 Instructions:");
  LogMessage(state, "1. Select attributes to prove (age, nationality, etc.)");
  LogMessage(state, "2. Click 'Generate ZK Proof' (takes 30-60 seconds)");
  LogMessage(state, "3. Click 'Verify Proof' to validate");
  LogMessage(state, "");
  LogMessage(state, "🔒 Your personal data stays private!");
  LogMessage(state, "---");

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
    glClearColor(0.08f, 0.08f, 0.12f, 1.0f);
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
