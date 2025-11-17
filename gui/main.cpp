#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <ctime>
#include <string>

// TODO: Inclure les headers Longfellow ZK lors de l'intégration
// #include "longfellow/zk_proof.h"
// #include "longfellow/mdoc.h"

struct AppState {
  // User input
  int birth_year = 2000;
  int birth_month = 1;
  int birth_day = 1;
  int age_threshold = 18;

  // Status
  std::string status_message;
  bool proof_exists = false;

  // Output
  std::string log;
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

// TODO: Remplacer par l'implémentation Longfellow ZK réelle
bool GenerateAgeProof(AppState &state) {
  LogMessage(state, "Starting proof generation...");

  // Calculate actual age
  time_t now = time(nullptr);
  tm *current = localtime(&now);
  int current_year = current->tm_year + 1900;
  int age = current_year - state.birth_year;

  if (current->tm_mon + 1 < state.birth_month ||
      (current->tm_mon + 1 == state.birth_month &&
       current->tm_mday < state.birth_day)) {
    age--;
  }

  LogMessage(state, "Age threshold: " + std::to_string(state.age_threshold));

  // TODO: Appelez la bibliothèque Longfellow ZK ici
  if (age >= state.age_threshold) {
    state.status_message =
        "Proof generated: Age >= " + std::to_string(state.age_threshold);
    state.proof_exists = true;
    LogMessage(state, "✓ Proof generation successful");
    return true;
  } else {
    state.status_message =
        "Cannot generate proof: Age < " + std::to_string(state.age_threshold);
    LogMessage(state, "✗ User does not meet age threshold");
    return false;
  }
}

// TODO: Remplacer par l'implémentation Longfellow ZK réelle
bool VerifyAgeProof(AppState &state) {
  if (!state.proof_exists) {
    LogMessage(state, "✗ No proof to verify");
    return false;
  }

  LogMessage(state, "Verifying proof...");

  // TODO: Appelez Longfellow ZK pour vérification ici
  LogMessage(state, "✓ Proof verification successful");
  state.status_message =
      "Proof verified: Age >= " + std::to_string(state.age_threshold);
  return true;
}

void RenderMainWindow(AppState &state) {
  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);

  ImGui::Begin("Age Verification - Zero Knowledge Proof", nullptr,
               ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoCollapse);

  ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f),
                     "Longfellow ZK - Age Verification");
  ImGui::Separator();

  // Birth date input
  ImGui::Text("Birth Date:");
  ImGui::SetNextItemWidth(100);
  ImGui::InputInt("Year", &state.birth_year);
  ImGui::SameLine();
  ImGui::SetNextItemWidth(80);
  ImGui::InputInt("Month", &state.birth_month);
  ImGui::SameLine();
  ImGui::SetNextItemWidth(80);
  ImGui::InputInt("Day", &state.birth_day);

  // Clamp values
  if (state.birth_year < 1900)
    state.birth_year = 1900;
  if (state.birth_year > 2025)
    state.birth_year = 2025;
  if (state.birth_month < 1)
    state.birth_month = 1;
  if (state.birth_month > 12)
    state.birth_month = 12;
  if (state.birth_day < 1)
    state.birth_day = 1;
  if (state.birth_day > 31)
    state.birth_day = 31;

  ImGui::Spacing();

  // Age threshold
  ImGui::Text("Age Threshold:");
  ImGui::SetNextItemWidth(200);
  ImGui::SliderInt("##threshold", &state.age_threshold, 13, 21);
  ImGui::SameLine();
  ImGui::Text("Prove age >= %d", state.age_threshold);

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  // Action buttons
  if (ImGui::Button("Generate ZK Proof", ImVec2(200, 40))) {
    GenerateAgeProof(state);
  }

  ImGui::SameLine();

  if (ImGui::Button("Verify Proof", ImVec2(200, 40))) {
    VerifyAgeProof(state);
  }

  ImGui::SameLine();

  if (ImGui::Button("Clear", ImVec2(120, 40))) {
    state.log.clear();
    state.status_message.clear();
    state.proof_exists = false;
    LogMessage(state, "Cleared");
  }

  ImGui::Spacing();

  // Status
  if (!state.status_message.empty()) {
    ImVec4 color = state.proof_exists ? ImVec4(0.0f, 1.0f, 0.0f, 1.0f)
                                      : ImVec4(1.0f, 0.7f, 0.0f, 1.0f);
    ImGui::TextColored(color, "%s", state.status_message.c_str());
  }

  ImGui::Separator();

  // Log output
  ImGui::Text("Log:");
  ImGui::BeginChild("LogOutput", ImVec2(0, 0), true);
  ImGui::TextUnformatted(state.log.c_str());
  if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
    ImGui::SetScrollHereY(1.0f);
  ImGui::EndChild();

  ImGui::End();
}

int main(int, char **) {
  glfwSetErrorCallback(glfw_error_callback);
  if (!glfwInit())
    return 1;

  const char *glsl_version = "#version 330";
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  GLFWwindow *window = glfwCreateWindow(
      900, 650, "Longfellow ZK - Age Verification", nullptr, nullptr);
  if (window == nullptr)
    return 1;

  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

  ImGui::StyleColorsDark();

  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init(glsl_version);

  AppState state;
  LogMessage(state, "Age Verification System Ready");
  LogMessage(state, "Enter birth date and click 'Generate ZK Proof'");

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
    glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
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
