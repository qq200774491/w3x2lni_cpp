#include "gui/widgets/file_dialog.h"

#include <cstring>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <commdlg.h>
#include <shlobj.h>
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#endif

namespace w3x_toolkit::gui {

void FileDialog::AddFilter(const std::string& description,
                           const std::string& filter) {
  filters_.push_back({description, filter});
}

// ---------------------------------------------------------------------------
// Windows native implementation
// ---------------------------------------------------------------------------

#ifdef _WIN32

std::string FileDialog::BuildWin32Filter() const {
  // Win32 filter strings are pairs of null-terminated strings terminated
  // by an extra null.  Example: "Maps\0*.w3x;*.w3m\0All\0*.*\0\0"
  std::string result;
  for (const auto& f : filters_) {
    result += f.description;
    result.push_back('\0');
    result += f.pattern;
    result.push_back('\0');
  }
  if (result.empty()) {
    result += "All Files";
    result.push_back('\0');
    result += "*.*";
    result.push_back('\0');
  }
  result.push_back('\0');  // Double-null terminator.
  return result;
}

std::optional<std::string> FileDialog::OpenFileDialog() {
  char file_buf[MAX_PATH] = {};
  std::string filter = BuildWin32Filter();

  OPENFILENAMEA ofn = {};
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = nullptr;
  ofn.lpstrFilter = filter.c_str();
  ofn.lpstrFile = file_buf;
  ofn.nMaxFile = sizeof(file_buf);
  ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

  if (GetOpenFileNameA(&ofn)) {
    return std::string(file_buf);
  }
  return std::nullopt;
}

std::optional<std::string> FileDialog::SaveFileDialog() {
  char file_buf[MAX_PATH] = {};
  std::string filter = BuildWin32Filter();

  OPENFILENAMEA ofn = {};
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = nullptr;
  ofn.lpstrFilter = filter.c_str();
  ofn.lpstrFile = file_buf;
  ofn.nMaxFile = sizeof(file_buf);
  ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

  if (GetSaveFileNameA(&ofn)) {
    return std::string(file_buf);
  }
  return std::nullopt;
}

std::optional<std::string> FileDialog::SelectDirectoryDialog() {
  // Use IFileDialog (Vista+) via SHBrowseForFolder as a simpler fallback.
  BROWSEINFOA bi = {};
  bi.lpszTitle = "Select Output Directory";
  bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

  LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
  if (pidl) {
    char path[MAX_PATH] = {};
    if (SHGetPathFromIDListA(pidl, path)) {
      CoTaskMemFree(pidl);
      return std::string(path);
    }
    CoTaskMemFree(pidl);
  }
  return std::nullopt;
}

// ---------------------------------------------------------------------------
// Non-Windows fallback (simple ImGui-based stub)
// ---------------------------------------------------------------------------

#else

std::optional<std::string> FileDialog::OpenFileDialog() {
  // On non-Windows platforms a full ImGui file browser would be needed.
  // For now, return nullopt so callers can handle the "no selection" case.
  // Integration with a cross-platform library (e.g. tinyfiledialogs or
  // nativefiledialog) is recommended for production use.
  return std::nullopt;
}

std::optional<std::string> FileDialog::SaveFileDialog() {
  return std::nullopt;
}

std::optional<std::string> FileDialog::SelectDirectoryDialog() {
  return std::nullopt;
}

#endif  // _WIN32

}  // namespace w3x_toolkit::gui
