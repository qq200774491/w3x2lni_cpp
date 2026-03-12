#include "core/platform/platform.h"

#include <cstdlib>
#include <string>

#ifdef W3X_PLATFORM_WINDOWS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <ShlObj.h>   // SHGetKnownFolderPath
#include <KnownFolders.h>
#else
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace w3x_toolkit::core {

#ifdef W3X_PLATFORM_WINDOWS
namespace {

std::wstring GetEnvironmentWide(const wchar_t* name) {
  wchar_t* buffer = nullptr;
  std::size_t size = 0;
  if (_wdupenv_s(&buffer, &size, name) != 0 || buffer == nullptr) {
    return {};
  }

  std::wstring value(buffer);
  std::free(buffer);
  return value;
}

}  // namespace
#endif

// ---------------------------------------------------------------------------
// GetHomeDirectory
// ---------------------------------------------------------------------------

std::filesystem::path Platform::GetHomeDirectory() {
#ifdef W3X_PLATFORM_WINDOWS
  const std::wstring profile = GetEnvironmentWide(L"USERPROFILE");
  if (!profile.empty()) {
    return std::filesystem::path(profile);
  }

  // Fall back to SHGetKnownFolderPath (Win32 API).
  wchar_t* folder_path = nullptr;
  HRESULT hr = SHGetKnownFolderPath(FOLDERID_Profile, 0, nullptr, &folder_path);
  if (SUCCEEDED(hr) && folder_path != nullptr) {
    std::filesystem::path result(folder_path);
    CoTaskMemFree(folder_path);
    return result;
  }
  if (folder_path != nullptr) {
    CoTaskMemFree(folder_path);
  }

  // Last resort: combine HOMEDRIVE + HOMEPATH.
  const std::wstring drive = GetEnvironmentWide(L"HOMEDRIVE");
  const std::wstring home_path = GetEnvironmentWide(L"HOMEPATH");
  if (!drive.empty() && !home_path.empty()) {
    return std::filesystem::path(drive + home_path);
  }

  // Nothing found; return an empty path.
  return {};
#else
  // POSIX: prefer the HOME environment variable.
  const char* home = std::getenv("HOME");
  if (home != nullptr && home[0] != '\0') {
    return std::filesystem::path(home);
  }

  // Fall back to the password database entry.
  struct passwd* pw = getpwuid(getuid());
  if (pw != nullptr && pw->pw_dir != nullptr) {
    return std::filesystem::path(pw->pw_dir);
  }

  return {};
#endif
}

// ---------------------------------------------------------------------------
// Utf8ToPath
// ---------------------------------------------------------------------------

std::filesystem::path Platform::Utf8ToPath(std::string_view utf8_path) {
#ifdef W3X_PLATFORM_WINDOWS
  std::wstring wide = Utf8ToWide(utf8_path);
  return std::filesystem::path(wide);
#else
  return std::filesystem::path(std::string(utf8_path));
#endif
}

// ---------------------------------------------------------------------------
// PathToUtf8
// ---------------------------------------------------------------------------

std::string Platform::PathToUtf8(const std::filesystem::path& path) {
#ifdef W3X_PLATFORM_WINDOWS
  const std::wstring& wide = path.native();
  return WideToUtf8(wide);
#else
  return path.string();
#endif
}

// ---------------------------------------------------------------------------
// Windows-only: UTF-8 <-> UTF-16 conversion
// ---------------------------------------------------------------------------

#ifdef W3X_PLATFORM_WINDOWS

std::wstring Platform::Utf8ToWide(std::string_view utf8_str) {
  if (utf8_str.empty()) {
    return {};
  }

  int input_len = static_cast<int>(utf8_str.size());

  // First call: determine the required buffer size.
  int wide_len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                     utf8_str.data(), input_len,
                                     nullptr, 0);
  if (wide_len <= 0) {
    return {};
  }

  std::wstring result(static_cast<std::size_t>(wide_len), L'\0');

  // Second call: perform the conversion.
  int written = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                    utf8_str.data(), input_len,
                                    result.data(), wide_len);
  if (written <= 0) {
    return {};
  }

  return result;
}

std::string Platform::WideToUtf8(std::wstring_view wide_str) {
  if (wide_str.empty()) {
    return {};
  }

  int input_len = static_cast<int>(wide_str.size());

  // First call: determine the required buffer size.
  int utf8_len = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                                     wide_str.data(), input_len,
                                     nullptr, 0,
                                     nullptr, nullptr);
  if (utf8_len <= 0) {
    return {};
  }

  std::string result(static_cast<std::size_t>(utf8_len), '\0');

  // Second call: perform the conversion.
  int written = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                                    wide_str.data(), input_len,
                                    result.data(), utf8_len,
                                    nullptr, nullptr);
  if (written <= 0) {
    return {};
  }

  return result;
}

#endif  // W3X_PLATFORM_WINDOWS

// ---------------------------------------------------------------------------
// SetupConsoleEncoding
// ---------------------------------------------------------------------------

void Platform::SetupConsoleEncoding() {
#ifdef W3X_PLATFORM_WINDOWS
  // Set both the input and output console code pages to UTF-8.
  SetConsoleCP(CP_UTF8);
  SetConsoleOutputCP(CP_UTF8);

  // Enable virtual terminal processing on stdout so that ANSI escape sequences
  // (colours, cursor movement, etc.) are interpreted correctly on Windows 10+.
  HANDLE stdout_handle = GetStdHandle(STD_OUTPUT_HANDLE);
  if (stdout_handle != INVALID_HANDLE_VALUE) {
    DWORD mode = 0;
    if (GetConsoleMode(stdout_handle, &mode)) {
      mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
      SetConsoleMode(stdout_handle, mode);
    }
  }

  // Do the same for stderr so that diagnostic output is also rendered
  // correctly.
  HANDLE stderr_handle = GetStdHandle(STD_ERROR_HANDLE);
  if (stderr_handle != INVALID_HANDLE_VALUE) {
    DWORD mode = 0;
    if (GetConsoleMode(stderr_handle, &mode)) {
      mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
      SetConsoleMode(stderr_handle, mode);
    }
  }
#endif
  // On Linux / macOS the terminal is assumed to support UTF-8 natively;
  // no setup is required.
}

}  // namespace w3x_toolkit::core
