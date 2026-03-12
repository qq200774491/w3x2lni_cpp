#ifndef W3X_TOOLKIT_GUI_FILE_DIALOG_H_
#define W3X_TOOLKIT_GUI_FILE_DIALOG_H_

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace w3x_toolkit::gui {

// Lightweight file/directory dialog utility.
//
// On Windows, this uses native Win32 dialogs (GetOpenFileName /
// GetSaveFileName / SHBrowseForFolder).  On other platforms it falls back
// to a simple ImGui-based file browser.
class FileDialog {
 public:
  FileDialog() = default;

  // Adds a file type filter.  |description| is shown in the dialog (e.g.
  // "Warcraft III Maps"), and |filter| is a semicolon-separated list of
  // patterns (e.g. "*.w3x;*.w3m").
  void AddFilter(const std::string& description, const std::string& filter);

  // Opens a file-open dialog and returns the selected file path, or
  // std::nullopt if the user cancelled.
  std::optional<std::string> OpenFileDialog();

  // Opens a file-save dialog and returns the chosen path.
  std::optional<std::string> SaveFileDialog();

  // Opens a directory picker and returns the chosen directory.
  std::optional<std::string> SelectDirectoryDialog();

 private:
  struct Filter {
    std::string description;
    std::string pattern;
  };
  std::vector<Filter> filters_;

#ifdef _WIN32
  // Builds a Win32 filter string (double-null-terminated).
  std::string BuildWin32Filter() const;
#endif

  // ImGui fallback state.
  // (The ImGui fallback is intentionally simple; a full in-process file
  // browser is out of scope for this utility.)
};

}  // namespace w3x_toolkit::gui

#endif  // W3X_TOOLKIT_GUI_FILE_DIALOG_H_
