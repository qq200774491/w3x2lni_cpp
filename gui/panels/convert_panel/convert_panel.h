#ifndef W3X_TOOLKIT_GUI_CONVERT_PANEL_H_
#define W3X_TOOLKIT_GUI_CONVERT_PANEL_H_

#include <functional>
#include <string>

#include "imgui.h"

#include "gui/panels/panel.h"

namespace w3x_toolkit::gui {

// Supported output formats for map conversion.
enum class ConvertFormat : int {
  kLni = 0,
  kSlk = 1,
  kObj = 2,
};

// Panel that exposes the map conversion interface: input/output selection,
// format picker, progress bar, and status messages.
class ConvertPanel : public Panel {
 public:
  // Callback invoked when the user clicks "Convert".
  // Parameters: input path, output directory, format.
  using ConvertCallback =
      std::function<void(const std::string&, const std::string&,
                         ConvertFormat)>;

  ConvertPanel();

  // Sets the callback invoked on conversion.
  void SetConvertCallback(ConvertCallback callback);

  // Updates the progress bar (0.0 .. 1.0).
  void SetProgress(float progress);

  // Appends a status message.
  void SetStatusMessage(const std::string& message);

 protected:
  void RenderContents() override;

 private:
  char input_path_[512] = {};
  char output_dir_[512] = {};
  int format_index_ = 0;
  float progress_ = 0.0f;
  std::string status_message_;
  ConvertCallback convert_callback_;
};

}  // namespace w3x_toolkit::gui

#endif  // W3X_TOOLKIT_GUI_CONVERT_PANEL_H_
