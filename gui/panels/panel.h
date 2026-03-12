#ifndef W3X_TOOLKIT_GUI_PANELS_PANEL_H_
#define W3X_TOOLKIT_GUI_PANELS_PANEL_H_

#include <string>
#include <string_view>

#include "imgui.h"

namespace w3x_toolkit::gui {

// Abstract base class for all dockable GUI panels.
class Panel {
 public:
  explicit Panel(std::string_view title) : title_(title), visible_(true) {}
  virtual ~Panel() = default;

  // Non-copyable, non-movable.
  Panel(const Panel&) = delete;
  Panel& operator=(const Panel&) = delete;
  Panel(Panel&&) = delete;
  Panel& operator=(Panel&&) = delete;

  // Renders the panel contents inside an ImGui window.  Called once per frame.
  // Subclasses must override RenderContents() to draw their UI elements.
  void Render() {
    if (!visible_) return;
    if (ImGui::Begin(title_.c_str(), &visible_)) {
      RenderContents();
    }
    ImGui::End();
  }

  // Returns the panel title (used as the ImGui window name/ID).
  const std::string& Title() const { return title_; }

  // Visibility control.
  bool IsVisible() const { return visible_; }
  void SetVisible(bool visible) { visible_ = visible; }

 protected:
  // Override this to draw the panel's actual content.
  virtual void RenderContents() = 0;

 private:
  std::string title_;
  bool visible_;
};

}  // namespace w3x_toolkit::gui

#endif  // W3X_TOOLKIT_GUI_PANELS_PANEL_H_
