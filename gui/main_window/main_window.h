#ifndef W3X_TOOLKIT_GUI_MAIN_WINDOW_H_
#define W3X_TOOLKIT_GUI_MAIN_WINDOW_H_

#include <memory>
#include <string>
#include <vector>

#include "imgui.h"

#include "core/error/error.h"
#include "gui/panels/panel.h"

struct GLFWwindow;

namespace w3x_toolkit::gui {

// Top-level application window.  Manages the GLFW window, the OpenGL context,
// Dear ImGui initialisation, docking layout, and the main render loop.
class MainWindow {
 public:
  MainWindow();
  ~MainWindow();

  // Not copyable or movable.
  MainWindow(const MainWindow&) = delete;
  MainWindow& operator=(const MainWindow&) = delete;
  MainWindow(MainWindow&&) = delete;
  MainWindow& operator=(MainWindow&&) = delete;

  // Creates the GLFW window, loads OpenGL, and sets up the ImGui context.
  core::Result<void> Initialize();

  // Enters the main loop: polls events, renders ImGui, swaps buffers.
  // Returns only when the user closes the window.
  void Run();

  // Tears down ImGui, destroys the GLFW window, and terminates GLFW.
  void Shutdown();

  // Registers a panel to be rendered each frame.  The MainWindow takes
  // ownership of the panel.
  void AddPanel(std::unique_ptr<Panel> panel);

  // Updates the status bar text shown at the bottom of the window.
  void SetStatusMessage(const std::string& message);

 private:
  // Renders the top menu bar.
  void RenderMenuBar();

  // Renders the status bar at the bottom of the viewport.
  void RenderStatusBar();

  // Configures the dockspace that fills the entire viewport.
  void SetupDockspace();

  // GLFW error callback.
  static void GlfwErrorCallback(int error, const char* description);

  GLFWwindow* window_ = nullptr;
  std::vector<std::unique_ptr<Panel>> panels_;
  std::string status_message_ = "Ready";
  bool show_about_dialog_ = false;

  // Window dimensions.
  static constexpr int kDefaultWidth = 1280;
  static constexpr int kDefaultHeight = 800;
  static constexpr const char* kWindowTitle = "W3X Toolkit";
};

}  // namespace w3x_toolkit::gui

#endif  // W3X_TOOLKIT_GUI_MAIN_WINDOW_H_
