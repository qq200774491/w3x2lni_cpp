#include "gui/main_window/main_window.h"

#include <cstdio>

#include "GLFW/glfw3.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "core/logger/logger.h"

namespace w3x_toolkit::gui {

MainWindow::MainWindow() = default;

MainWindow::~MainWindow() { Shutdown(); }

void MainWindow::GlfwErrorCallback(int error, const char* description) {
  core::Logger::Instance().Error("GLFW error {}: {}", error, description);
}

core::Result<void> MainWindow::Initialize() {
  glfwSetErrorCallback(GlfwErrorCallback);

  if (!glfwInit()) {
    return std::unexpected(
        core::Error::IOError("Failed to initialize GLFW"));
  }

  // Request OpenGL 3.3 core profile.
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif

  window_ = glfwCreateWindow(kDefaultWidth, kDefaultHeight, kWindowTitle,
                              nullptr, nullptr);
  if (!window_) {
    glfwTerminate();
    return std::unexpected(
        core::Error::IOError("Failed to create GLFW window"));
  }

  glfwMakeContextCurrent(window_);
  glfwSwapInterval(1);  // Enable vsync.

  // Setup Dear ImGui context.
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();

  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

  ImGui::StyleColorsDark();

  // Initialise platform/renderer backends.
  ImGui_ImplGlfw_InitForOpenGL(window_, true);
  ImGui_ImplOpenGL3_Init("#version 330");

  core::Logger::Instance().Info("MainWindow initialized successfully");
  return {};
}

void MainWindow::Run() {
  while (!glfwWindowShouldClose(window_)) {
    glfwPollEvents();

    // Start the Dear ImGui frame.
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    SetupDockspace();
    RenderMenuBar();

    // Render all registered panels.
    for (auto& panel : panels_) {
      panel->Render();
    }

    // About dialog.
    if (show_about_dialog_) {
      ImGui::OpenPopup("About W3X Toolkit");
      show_about_dialog_ = false;
    }
    if (ImGui::BeginPopupModal("About W3X Toolkit", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
      ImGui::Text("W3X Toolkit");
      ImGui::Separator();
      ImGui::Text("A Warcraft III map toolchain.");
      ImGui::Text("Built with Dear ImGui.");
      if (ImGui::Button("OK", ImVec2(120, 0))) {
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndPopup();
    }

    RenderStatusBar();

    // Rendering.
    ImGui::Render();
    int display_w = 0;
    int display_h = 0;
    glfwGetFramebufferSize(window_, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(window_);
  }
}

void MainWindow::Shutdown() {
  if (!window_) return;

  panels_.clear();

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  glfwDestroyWindow(window_);
  window_ = nullptr;
  glfwTerminate();

  core::Logger::Instance().Info("MainWindow shut down");
}

void MainWindow::AddPanel(std::unique_ptr<Panel> panel) {
  panels_.push_back(std::move(panel));
}

void MainWindow::SetStatusMessage(const std::string& message) {
  status_message_ = message;
}

void MainWindow::SetupDockspace() {
  const ImGuiViewport* viewport = ImGui::GetMainViewport();

  // Reserve space for the menu bar at the top and status bar at the bottom.
  constexpr float kStatusBarHeight = 24.0f;

  ImGui::SetNextWindowPos(
      ImVec2(viewport->WorkPos.x, viewport->WorkPos.y));
  ImGui::SetNextWindowSize(
      ImVec2(viewport->WorkSize.x,
             viewport->WorkSize.y - kStatusBarHeight));
  ImGui::SetNextWindowViewport(viewport->ID);

  ImGuiWindowFlags window_flags =
      ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
      ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
      ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground;

  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

  ImGui::Begin("DockSpaceWindow", nullptr, window_flags);
  ImGui::PopStyleVar(3);

  ImGuiID dockspace_id = ImGui::GetID("MainDockspace");
  ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f),
                   ImGuiDockNodeFlags_PassthruCentralNode);

  ImGui::End();
}

void MainWindow::RenderMenuBar() {
  if (ImGui::BeginMainMenuBar()) {
    if (ImGui::BeginMenu("File")) {
      if (ImGui::MenuItem("Open Map...", "Ctrl+O")) {
        // TODO: Integrate with FileDialog.
      }
      if (ImGui::MenuItem("Save", "Ctrl+S")) {
        // TODO: Save current map.
      }
      ImGui::Separator();
      if (ImGui::MenuItem("Exit", "Alt+F4")) {
        glfwSetWindowShouldClose(window_, GLFW_TRUE);
      }
      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Tools")) {
      if (ImGui::MenuItem("Convert")) {
        // TODO: Show convert panel.
      }
      if (ImGui::MenuItem("Extract")) {
        // TODO: Show extraction options.
      }
      if (ImGui::MenuItem("Analyze")) {
        // TODO: Trigger analysis.
      }
      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View")) {
      for (auto& panel : panels_) {
        bool visible = panel->IsVisible();
        if (ImGui::MenuItem(panel->Title().c_str(), nullptr, &visible)) {
          panel->SetVisible(visible);
        }
      }
      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Help")) {
      if (ImGui::MenuItem("About")) {
        show_about_dialog_ = true;
      }
      ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
  }
}

void MainWindow::RenderStatusBar() {
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  constexpr float kStatusBarHeight = 24.0f;

  ImGui::SetNextWindowPos(
      ImVec2(viewport->WorkPos.x,
             viewport->WorkPos.y + viewport->WorkSize.y - kStatusBarHeight));
  ImGui::SetNextWindowSize(
      ImVec2(viewport->WorkSize.x, kStatusBarHeight));

  ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
                           ImGuiWindowFlags_NoMove |
                           ImGuiWindowFlags_NoScrollWithMouse |
                           ImGuiWindowFlags_NoDocking |
                           ImGuiWindowFlags_NoSavedSettings;

  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4.0f, 2.0f));
  if (ImGui::Begin("##StatusBar", nullptr, flags)) {
    ImGui::Text("%s", status_message_.c_str());
  }
  ImGui::End();
  ImGui::PopStyleVar();
}

}  // namespace w3x_toolkit::gui
