#ifndef W3X_TOOLKIT_CORE_CONFIG_RUNTIME_PATHS_H_
#define W3X_TOOLKIT_CORE_CONFIG_RUNTIME_PATHS_H_

#include <filesystem>
#include <vector>

#include "core/error/error.h"

namespace w3x_toolkit::core {

class RuntimePaths {
 public:
  static std::filesystem::path GetSourceRoot();
  static std::vector<std::filesystem::path> CandidateRoots();

  [[nodiscard]] static Result<std::filesystem::path> ResolveProjectRoot();
  [[nodiscard]] static Result<std::filesystem::path> ResolveDataRoot();

  static std::filesystem::path ResolveGlobalConfigPath();
  static std::filesystem::path ResolveDefaultConfigPath();
  static std::filesystem::path ResolveLogPath();
};

}  // namespace w3x_toolkit::core

#endif  // W3X_TOOLKIT_CORE_CONFIG_RUNTIME_PATHS_H_
