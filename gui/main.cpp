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

// Intégration Longfellow ZK Logic
#include "zk_workflow.h"
#include "util/log.h"

// -----------------------------------------------------------------------------
// Data Structures (UI Specific)
// -----------------------------------------------------------------------------

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
  bool prove_vaccine = false;
  bool prove_insurance = false;
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
  // Note: The internal structure is hidden in zk_workflow.cpp, here we just hold byte buffers or similar if we wanted strict separation
  // but for now we keep the definition compatible with what logic expects (void* casting)
  struct CircuitCache {
    std::vector<uint8_t> circuit_data;
    size_t circuit_len = 0;
    size_t num_attributes = 0;
    const void *zk_spec = nullptr;
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
// Async ZK Wrapper
// -----------------------------------------------------------------------------

void GenerateZKProofAsync(AppState &state) {
  state.is_generating = true;
  state.status_message = "Generating proof... (This may take 30-60s)";

  // Create config copy
  ProverConfig config;
  config.birth_year = state.birth_year;
  config.birth_month = state.birth_month;
  config.birth_day = state.birth_day;
  config.prove_age = state.prove_age;
  config.prove_nationality = state.prove_nationality;
  config.prove_french_license = state.prove_french_license;
  config.prove_health_issuer = state.prove_health_issuer;
  config.prove_vaccine = state.prove_vaccine;
  config.prove_insurance = state.prove_insurance;
  config.selected_issuer = state.selected_issuer;
  config.eu_vaccines_compliant = state.eu_vaccines_compliant;
  config.prove_category_A = state.prove_category_A;
  config.prove_category_B = state.prove_category_B;
  config.prove_category_C = state.prove_category_C;
  config.age_threshold = state.age_threshold;
  config.selected_nationality = state.selected_nationality;
  config.circuit_cache_1attr = &state.circuit_cache_1attr;
  config.circuit_cache_2attr = &state.circuit_cache_2attr;

  state.generation_task = std::async(std::launch::async, [&state, config]() {
    ProofData result_proof;
    std::string log_buffer;
    int age_out = 0;

    bool success = PerformZKProofGeneration(config, result_proof, log_buffer, age_out);

    {
      std::lock_guard<std::recursive_mutex> lock(state.mutex);
      // Merge log
      state.log += log_buffer;
      state.calculated_age = age_out;

      if (success) {
          state.proof_data = result_proof;
          state.status_message = "✓ Proof generated successfully";
      } else {
          state.status_message = "❌ Verification Failed or Error";
      }
      state.is_generating = false;
    }
  });
}

// VRAIE vérification de preuve ZK - appelle run_mdoc_verifier
bool VerifyZKProof(AppState &state) {
  std::lock_guard<std::recursive_mutex> lock(state.mutex);
  std::string log_buffer;
  
  bool success = PerformZKVerification(state.proof_data, log_buffer);
  
  state.log += log_buffer;
  if (success) {
      state.status_message = "[OK] Proof verified successfully";
  } else {
      state.status_message = "Verification failed";
  }
  return success;
}

// Wrapper for Export
bool ExportProofWrapper(AppState &state, const std::string &filename) {
    std::lock_guard<std::recursive_mutex> lock(state.mutex);
    // Reconstruct config just for settings export (could be optimized)
    ProverConfig config;
    config.age_threshold = state.age_threshold;
    config.selected_nationality = state.selected_nationality;
    
    return ExportProof(state.proof_data, config, filename);
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
          
          static std::vector<std::string> nation_items;
          static std::vector<const char*> nation_ptrs;
          if (nation_items.empty()) {
              for (int i=0; i<kNumCountries; ++i) {
                  const auto& c = kCountries[i];
                  std::string label = std::string(c.name) + " (" + c.alpha3 + " / " + c.numeric + ")";
                  nation_items.push_back(label);
              }
              for (const auto& s : nation_items) nation_ptrs.push_back(s.c_str());
          }
          
          ImGui::Combo("##NatCombo", &state.selected_nationality, nation_ptrs.data(), (int)nation_ptrs.size());
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
            ImGui::Checkbox("Verify Specific Vaccine (Comirnaty/Pfizer)", &state.prove_vaccine);
            ImGui::Checkbox("Verify Health Insurance Status", &state.prove_insurance);

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
        if (ExportProofWrapper(state, "proof.json")) {
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
  // Note: Log level might be handled inside zk_workflow if needed, or here globally
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