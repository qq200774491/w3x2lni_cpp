#include "gui/panels/convert_panel/convert_panel.h"

#include "gui/widgets/file_dialog.h"

namespace w3x_toolkit::gui {

ConvertPanel::ConvertPanel() : Panel("Convert") {}

void ConvertPanel::SetConvertCallback(ConvertCallback callback) {
  convert_callback_ = std::move(callback);
}

void ConvertPanel::SetProgress(float progress) { progress_ = progress; }

void ConvertPanel::SetStatusMessage(const std::string& message) {
  status_message_ = message;
}

void ConvertPanel::RenderContents() {
  ImGui::Text("Conversion Tool");
  ImGui::Separator();
  ImGui::Spacing();

  // --- Input file ---
  ImGui::Text("Input File:");
  ImGui::SetNextItemWidth(-80.0f);
  ImGui::InputText("##InputPath", input_path_, sizeof(input_path_),
                   ImGuiInputTextFlags_ReadOnly);
  ImGui::SameLine();
  if (ImGui::Button("Browse##In")) {
    FileDialog dialog;
    dialog.AddFilter("Warcraft III Maps", "*.w3x;*.w3m");
    dialog.AddFilter("All Files", "*.*");
    auto result = dialog.OpenFileDialog();
    if (result.has_value() && !result->empty()) {
      std::snprintf(input_path_, sizeof(input_path_), "%s",
                    result->c_str());
    }
  }

  ImGui::Spacing();

  // --- Output directory ---
  ImGui::Text("Output Directory:");
  ImGui::SetNextItemWidth(-80.0f);
  ImGui::InputText("##OutputDir", output_dir_, sizeof(output_dir_),
                   ImGuiInputTextFlags_ReadOnly);
  ImGui::SameLine();
  if (ImGui::Button("Browse##Out")) {
    FileDialog dialog;
    auto result = dialog.SelectDirectoryDialog();
    if (result.has_value() && !result->empty()) {
      std::snprintf(output_dir_, sizeof(output_dir_), "%s",
                    result->c_str());
    }
  }

  ImGui::Spacing();

  // --- Format selector ---
  ImGui::Text("Output Format:");
  const char* format_names[] = {"LNI", "SLK", "OBJ"};
  ImGui::Combo("##Format", &format_index_, format_names,
               IM_ARRAYSIZE(format_names));

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  // --- Convert button ---
  bool can_convert = (input_path_[0] != '\0' && output_dir_[0] != '\0');
  if (!can_convert) {
    ImGui::BeginDisabled();
  }
  if (ImGui::Button("Convert", ImVec2(120, 0))) {
    if (convert_callback_) {
      convert_callback_(std::string(input_path_), std::string(output_dir_),
                        static_cast<ConvertFormat>(format_index_));
    }
  }
  if (!can_convert) {
    ImGui::EndDisabled();
  }

  ImGui::Spacing();

  // --- Progress bar ---
  if (progress_ > 0.0f) {
    ImGui::ProgressBar(progress_, ImVec2(-1.0f, 0.0f));
  }

  // --- Status message ---
  if (!status_message_.empty()) {
    ImGui::Spacing();
    ImGui::TextWrapped("%s", status_message_.c_str());
  }
}

}  // namespace w3x_toolkit::gui
