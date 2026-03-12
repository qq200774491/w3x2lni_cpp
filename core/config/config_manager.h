#ifndef W3X_TOOLKIT_CORE_CONFIG_CONFIG_MANAGER_H_
#define W3X_TOOLKIT_CORE_CONFIG_CONFIG_MANAGER_H_

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/error/error.h"

namespace w3x_toolkit::core {

enum class ConfigLayer {
  kDefault,
  kGlobal,
  kMap,
};

enum class ConfigValueType {
  kString,
  kBoolean,
  kInteger,
};

struct ConfigValueInfo {
  std::string value;
  ConfigLayer layer = ConfigLayer::kDefault;
};

class ConfigManager {
 public:
  using Section = std::map<std::string, std::string, std::less<>>;
  using IniData = std::map<std::string, Section, std::less<>>;

  struct EntryDefinition {
    std::string section;
    std::string key;
    ConfigValueType type = ConfigValueType::kString;
    bool visible = true;
  };

  ConfigManager();

  [[nodiscard]] Result<void> LoadDefaults(const std::filesystem::path& path);
  [[nodiscard]] Result<void> LoadGlobal(const std::filesystem::path& path);
  [[nodiscard]] Result<void> LoadMap(const std::filesystem::path& path);

  [[nodiscard]] Result<void> SaveGlobal(const std::filesystem::path& path) const;
  [[nodiscard]] Result<void> SaveMap(const std::filesystem::path& path) const;
  [[nodiscard]] Result<void> SaveEffective(
      const std::filesystem::path& path) const;

  void ClearGlobal();
  void ClearMap();

  [[nodiscard]] bool HasDefinition(std::string_view section,
                                   std::string_view key) const;
  [[nodiscard]] bool IsVisible(std::string_view section,
                               std::string_view key) const;

  [[nodiscard]] std::optional<ConfigValueInfo> GetRaw(
      std::string_view section, std::string_view key) const;
  [[nodiscard]] std::string GetString(std::string_view section,
                                      std::string_view key,
                                      std::string_view fallback = "") const;
  [[nodiscard]] bool GetBool(std::string_view section, std::string_view key,
                             bool fallback = false) const;
  [[nodiscard]] int GetInt(std::string_view section, std::string_view key,
                           int fallback = 0) const;

  [[nodiscard]] Result<void> SetGlobal(std::string_view section,
                                       std::string_view key,
                                       std::string_view value);
  [[nodiscard]] Result<void> SetMap(std::string_view section,
                                    std::string_view key,
                                    std::string_view value);
  bool RemoveGlobal(std::string_view section, std::string_view key);
  bool RemoveMap(std::string_view section, std::string_view key);

  [[nodiscard]] std::vector<std::string> Sections() const;
  [[nodiscard]] std::vector<std::string> Keys(std::string_view section) const;

  [[nodiscard]] static Result<std::pair<std::string, std::string>>
  ParseQualifiedKey(std::string_view qualified_key);
  [[nodiscard]] static std::string LayerName(ConfigLayer layer);

 private:
  [[nodiscard]] const EntryDefinition* FindDefinition(
      std::string_view section, std::string_view key) const;

  [[nodiscard]] Result<void> LoadLayer(const std::filesystem::path& path,
                                       IniData& target,
                                       bool required);
  [[nodiscard]] Result<void> SaveLayer(const std::filesystem::path& path,
                                       const IniData& data) const;
  [[nodiscard]] IniData BuildEffectiveVisibleData() const;

  [[nodiscard]] Result<IniData> ParseIni(std::string_view content,
                                         const std::filesystem::path& path) const;
  [[nodiscard]] Result<std::string> NormalizeValue(
      const EntryDefinition* definition, std::string_view value,
      std::string_view context) const;
  [[nodiscard]] std::string SerializeIni(const IniData& data) const;

  std::vector<EntryDefinition> definitions_;
  std::vector<std::string> section_order_;
  std::map<std::string, std::vector<std::string>, std::less<>> key_order_;
  IniData defaults_;
  IniData global_;
  IniData map_;
};

}  // namespace w3x_toolkit::core

#endif  // W3X_TOOLKIT_CORE_CONFIG_CONFIG_MANAGER_H_
