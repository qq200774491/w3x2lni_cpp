#ifndef W3X_TOOLKIT_CORE_PLATFORM_PLATFORM_H_
#define W3X_TOOLKIT_CORE_PLATFORM_PLATFORM_H_

#include <filesystem>
#include <string>
#include <string_view>

// ---------------------------------------------------------------------------
// OS detection
// ---------------------------------------------------------------------------
#if defined(_WIN32) || defined(_WIN64)
#define W3X_PLATFORM_WINDOWS 1
#elif defined(__APPLE__) && defined(__MACH__)
#define W3X_PLATFORM_MACOS 1
#elif defined(__linux__)
#define W3X_PLATFORM_LINUX 1
#else
#error "Unsupported platform"
#endif

// ---------------------------------------------------------------------------
// Architecture detection
// ---------------------------------------------------------------------------
#if defined(_M_X64) || defined(__x86_64__)
#define W3X_ARCH_X64 1
#elif defined(_M_IX86) || defined(__i386__)
#define W3X_ARCH_X86 1
#elif defined(_M_ARM64) || defined(__aarch64__)
#define W3X_ARCH_ARM64 1
#elif defined(_M_ARM) || defined(__arm__)
#define W3X_ARCH_ARM 1
#else
#define W3X_ARCH_UNKNOWN 1
#endif

namespace w3x_toolkit::core {

// Supported operating systems.
enum class OperatingSystem {
  kWindows,
  kLinux,
  kMacOS,
};

// Supported CPU architectures.
enum class Architecture {
  kX86,
  kX64,
  kArm,
  kArm64,
  kUnknown,
};

// Utility class that provides static methods for querying platform information
// and performing platform-specific operations such as UTF-8 path conversions
// and console encoding setup.
//
// This class cannot be instantiated; all functionality is accessed through
// static methods.
class Platform {
 public:
  Platform() = delete;
  ~Platform() = delete;
  Platform(const Platform&) = delete;
  Platform& operator=(const Platform&) = delete;

  // ---------------------------------------------------------------------------
  // Platform information queries
  // ---------------------------------------------------------------------------

  // Returns the operating system that the binary was compiled for.
  static constexpr OperatingSystem GetOperatingSystem();

  // Returns a human-readable name for the current OS (e.g. "Windows").
  static constexpr std::string_view GetOsName();

  // Returns the CPU architecture that the binary targets.
  static constexpr Architecture GetArchitecture();

  // Returns a human-readable name for the architecture (e.g. "x64").
  static constexpr std::string_view GetArchitectureName();

  // Returns the platform-native path separator character ('\\' on Windows,
  // '/' elsewhere).
  static constexpr char GetPathSeparator();

  // Returns the platform-native path separator as a string view.
  static constexpr std::string_view GetPathSeparatorString();

  // Returns true when the binary was compiled for Windows.
  static constexpr bool IsWindows();

  // Returns true when the binary was compiled for Linux.
  static constexpr bool IsLinux();

  // Returns true when the binary was compiled for macOS.
  static constexpr bool IsMacOS();

  // ---------------------------------------------------------------------------
  // Filesystem helpers
  // ---------------------------------------------------------------------------

  // Returns the home directory of the current user.
  // On Windows this queries the USERPROFILE environment variable (falling back
  // to the Win32 API). On POSIX systems it reads the HOME variable.
  static std::filesystem::path GetHomeDirectory();

  // ---------------------------------------------------------------------------
  // UTF-8 path conversion utilities
  // ---------------------------------------------------------------------------

  // Converts a UTF-8 encoded string to a std::filesystem::path.
  // On Windows this performs a UTF-8 -> UTF-16 conversion so that the
  // resulting path is compatible with the Win32 wide-char API.
  // On POSIX systems the string is used as-is.
  static std::filesystem::path Utf8ToPath(std::string_view utf8_path);

  // Converts a std::filesystem::path back to a UTF-8 encoded string.
  // On Windows this performs a UTF-16 -> UTF-8 conversion.
  // On POSIX systems the native string representation is returned directly.
  static std::string PathToUtf8(const std::filesystem::path& path);

#ifdef W3X_PLATFORM_WINDOWS
  // Converts a UTF-8 string to a UTF-16 wide string (Windows only).
  static std::wstring Utf8ToWide(std::string_view utf8_str);

  // Converts a UTF-16 wide string to a UTF-8 string (Windows only).
  static std::string WideToUtf8(std::wstring_view wide_str);
#endif

  // ---------------------------------------------------------------------------
  // Console encoding
  // ---------------------------------------------------------------------------

  // Configures the console / terminal for UTF-8 I/O.
  // On Windows this sets the console code pages to CP_UTF8 and enables virtual
  // terminal processing for ANSI escape sequences. On other platforms this is
  // a no-op.
  static void SetupConsoleEncoding();
};

// ---------------------------------------------------------------------------
// constexpr inline implementations
// ---------------------------------------------------------------------------

constexpr OperatingSystem Platform::GetOperatingSystem() {
#if defined(W3X_PLATFORM_WINDOWS)
  return OperatingSystem::kWindows;
#elif defined(W3X_PLATFORM_MACOS)
  return OperatingSystem::kMacOS;
#elif defined(W3X_PLATFORM_LINUX)
  return OperatingSystem::kLinux;
#endif
}

constexpr std::string_view Platform::GetOsName() {
#if defined(W3X_PLATFORM_WINDOWS)
  return "Windows";
#elif defined(W3X_PLATFORM_MACOS)
  return "macOS";
#elif defined(W3X_PLATFORM_LINUX)
  return "Linux";
#endif
}

constexpr Architecture Platform::GetArchitecture() {
#if defined(W3X_ARCH_X64)
  return Architecture::kX64;
#elif defined(W3X_ARCH_X86)
  return Architecture::kX86;
#elif defined(W3X_ARCH_ARM64)
  return Architecture::kArm64;
#elif defined(W3X_ARCH_ARM)
  return Architecture::kArm;
#else
  return Architecture::kUnknown;
#endif
}

constexpr std::string_view Platform::GetArchitectureName() {
#if defined(W3X_ARCH_X64)
  return "x64";
#elif defined(W3X_ARCH_X86)
  return "x86";
#elif defined(W3X_ARCH_ARM64)
  return "ARM64";
#elif defined(W3X_ARCH_ARM)
  return "ARM";
#else
  return "Unknown";
#endif
}

constexpr char Platform::GetPathSeparator() {
#if defined(W3X_PLATFORM_WINDOWS)
  return '\\';
#else
  return '/';
#endif
}

constexpr std::string_view Platform::GetPathSeparatorString() {
#if defined(W3X_PLATFORM_WINDOWS)
  return "\\";
#else
  return "/";
#endif
}

constexpr bool Platform::IsWindows() {
#if defined(W3X_PLATFORM_WINDOWS)
  return true;
#else
  return false;
#endif
}

constexpr bool Platform::IsLinux() {
#if defined(W3X_PLATFORM_LINUX)
  return true;
#else
  return false;
#endif
}

constexpr bool Platform::IsMacOS() {
#if defined(W3X_PLATFORM_MACOS)
  return true;
#else
  return false;
#endif
}

}  // namespace w3x_toolkit::core

#endif  // W3X_TOOLKIT_CORE_PLATFORM_PLATFORM_H_
