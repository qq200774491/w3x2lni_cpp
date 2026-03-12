#include "core/config/runtime_paths.h"

#include <algorithm>

#include "core/platform/platform.h"

namespace w3x_toolkit::core {

namespace fs = std::filesystem;

namespace {

bool LooksLikeProjectRoot(const fs::path& root) {
  return fs::exists(root / "data") || fs::exists(root / "config.ini") ||
         fs::exists(root / "external") || fs::exists(root / "CMakeLists.txt");
}

void AppendUnique(std::vector<fs::path>& paths, const fs::path& candidate) {
  if (candidate.empty()) {
    return;
  }
  std::error_code ec;
  const fs::path normalized = fs::weakly_canonical(candidate, ec);
  const fs::path value = ec ? candidate.lexically_normal() : normalized;
  if (std::ranges::find(paths, value) == paths.end()) {
    paths.push_back(value);
  }
}

}  // namespace

fs::path RuntimePaths::GetSourceRoot() {
#ifdef W3X_SOURCE_DIR
  return fs::path(W3X_SOURCE_DIR);
#else
  return {};
#endif
}

std::vector<fs::path> RuntimePaths::CandidateRoots() {
  std::vector<fs::path> roots;
  std::error_code ec;
  AppendUnique(roots, fs::current_path(ec));
  AppendUnique(roots, Platform::GetExecutableDirectory());
  AppendUnique(roots, GetSourceRoot());
  return roots;
}

Result<fs::path> RuntimePaths::ResolveProjectRoot() {
  const auto roots = CandidateRoots();
  for (const fs::path& root : roots) {
    if (LooksLikeProjectRoot(root)) {
      return root;
    }
  }
  if (!roots.empty()) {
    return roots.front();
  }
  return std::unexpected(
      Error::ConfigError("Unable to resolve runtime project root"));
}

Result<fs::path> RuntimePaths::ResolveDataRoot() {
  for (const fs::path& root : CandidateRoots()) {
    const fs::path data_root = root / "data";
    if (fs::exists(data_root)) {
      return data_root;
    }
  }
  return std::unexpected(Error::FileNotFound(
      "Unable to locate bundled data directory"));
}

fs::path RuntimePaths::ResolveGlobalConfigPath() {
  auto root = ResolveProjectRoot();
  if (root.has_value()) {
    return root.value() / "config.ini";
  }
  return fs::current_path() / "config.ini";
}

fs::path RuntimePaths::ResolveDefaultConfigPath() {
  auto data_root = ResolveDataRoot();
  if (data_root.has_value()) {
    return data_root.value() / "default_config.ini";
  }
  auto root = ResolveProjectRoot();
  if (root.has_value()) {
    return root.value() / "data" / "default_config.ini";
  }
  return fs::current_path() / "data" / "default_config.ini";
}

fs::path RuntimePaths::ResolveLogPath() {
  const fs::path cwd_log = fs::current_path() / "w3x_toolkit.log";
  if (fs::exists(cwd_log)) {
    return cwd_log;
  }
  auto root = ResolveProjectRoot();
  if (root.has_value()) {
    return root.value() / "w3x_toolkit.log";
  }
  return cwd_log;
}

}  // namespace w3x_toolkit::core
