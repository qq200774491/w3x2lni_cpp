#ifndef W3X_TOOLKIT_CORE_CONFIG_CONFIG_MANAGER_H_
#define W3X_TOOLKIT_CORE_CONFIG_CONFIG_MANAGER_H_

#include <cstdint>
#include <filesystem>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

namespace w3x_toolkit::core {

// Manages typed configuration values backed by JSON files.
//
// Keys use dot notation to address nested objects (e.g., "app.log_level").
// Thread safety: concurrent reads are safe; writes acquire an exclusive lock.
//
// Example usage:
//   ConfigManager cfg;
//   cfg.LoadFromFile("settings.json");
//   auto level = cfg.Get<std::string>("app.log_level", "info");
//   cfg.Set("app.log_level", "debug");
//   cfg.SaveToFile("settings.json");
class ConfigManager {
 public:
  ConfigManager() = default;

  // Non-copyable, movable.
  ConfigManager(const ConfigManager&) = delete;
  ConfigManager& operator=(const ConfigManager&) = delete;
  ConfigManager(ConfigManager&& other) noexcept;
  ConfigManager& operator=(ConfigManager&& other) noexcept;

  ~ConfigManager() = default;

  // ---------------------------------------------------------------------------
  // File I/O
  // ---------------------------------------------------------------------------

  // Loads configuration from a JSON file, replacing current contents.
  // Returns false if the file cannot be read or parsed.
  [[nodiscard]] bool LoadFromFile(const std::filesystem::path& file_path);

  // Saves the current configuration to a JSON file (pretty-printed).
  // Returns false if the file cannot be written.
  [[nodiscard]] bool SaveToFile(const std::filesystem::path& file_path) const;

  // Loads configuration from a JSON string, replacing current contents.
  // Returns false if the string is not valid JSON.
  [[nodiscard]] bool LoadFromString(std::string_view json_text);

  // ---------------------------------------------------------------------------
  // Typed accessors
  // ---------------------------------------------------------------------------

  // Returns the value at |key| converted to T, or std::nullopt if the key does
  // not exist or the stored value is not convertible to T.
  template <typename T>
  [[nodiscard]] std::optional<T> Get(std::string_view key) const;

  // Returns the value at |key| converted to T, or |default_value| when the key
  // is absent or not convertible.
  template <typename T>
  [[nodiscard]] T Get(std::string_view key, const T& default_value) const;

  // Sets |key| to |value|.  Intermediate objects are created as needed.
  template <typename T>
  void Set(std::string_view key, const T& value);

  // ---------------------------------------------------------------------------
  // Key inspection / removal
  // ---------------------------------------------------------------------------

  // Returns true if a value exists at |key|.
  [[nodiscard]] bool Has(std::string_view key) const;

  // Removes the value at |key|.  Returns true if the key existed.
  bool Remove(std::string_view key);

  // Returns a list of all top-level keys.
  [[nodiscard]] std::vector<std::string> Keys() const;

  // Removes all stored configuration.
  void Clear();

  // Returns the underlying JSON object (deep copy).  Useful for serialization
  // or debugging.
  [[nodiscard]] nlohmann::json GetJson() const;

 private:
  // Splits a dot-notation key into its components.
  static std::vector<std::string> SplitKey(std::string_view key);

  // Navigates to the JSON value addressed by |parts|, returning nullptr if any
  // intermediate key is missing or not an object.
  static const nlohmann::json* Resolve(const nlohmann::json& root,
                                       const std::vector<std::string>& parts);

  // Navigates (or creates) the path to the *parent* of the final key component.
  // Returns a pointer to the parent object and writes the leaf name into
  // |leaf_out|.  Returns nullptr on failure.
  static nlohmann::json* ResolveOrCreate(nlohmann::json& root,
                                         const std::vector<std::string>& parts,
                                         std::string& leaf_out);

  mutable std::shared_mutex mutex_;
  nlohmann::json data_ = nlohmann::json::object();
};

// =============================================================================
// Template implementations (must be in the header)
// =============================================================================

template <typename T>
std::optional<T> ConfigManager::Get(std::string_view key) const {
  std::shared_lock lock(mutex_);
  const auto parts = SplitKey(key);
  if (parts.empty()) {
    return std::nullopt;
  }
  const nlohmann::json* node = Resolve(data_, parts);
  if (node == nullptr) {
    return std::nullopt;
  }
  try {
    return node->get<T>();
  } catch (const nlohmann::json::exception&) {
    return std::nullopt;
  }
}

template <typename T>
T ConfigManager::Get(std::string_view key, const T& default_value) const {
  return Get<T>(key).value_or(default_value);
}

template <typename T>
void ConfigManager::Set(std::string_view key, const T& value) {
  std::unique_lock lock(mutex_);
  auto parts = SplitKey(key);
  if (parts.empty()) {
    return;
  }
  std::string leaf;
  nlohmann::json* parent = ResolveOrCreate(data_, parts, leaf);
  if (parent != nullptr) {
    (*parent)[leaf] = value;
  }
}

}  // namespace w3x_toolkit::core

#endif  // W3X_TOOLKIT_CORE_CONFIG_CONFIG_MANAGER_H_
