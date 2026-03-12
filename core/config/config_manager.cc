#include "core/config/config_manager.h"

#include <fstream>
#include <sstream>
#include <utility>

namespace w3x_toolkit::core {

// -----------------------------------------------------------------------------
// Move operations
// -----------------------------------------------------------------------------

ConfigManager::ConfigManager(ConfigManager&& other) noexcept {
  std::unique_lock other_lock(other.mutex_);
  data_ = std::move(other.data_);
  other.data_ = nlohmann::json::object();
}

ConfigManager& ConfigManager::operator=(ConfigManager&& other) noexcept {
  if (this == &other) {
    return *this;
  }
  // Lock both mutexes in a consistent order (lower address first) to prevent
  // deadlocks.
  std::shared_mutex* first = &mutex_;
  std::shared_mutex* second = &other.mutex_;
  if (first > second) {
    std::swap(first, second);
  }
  std::unique_lock lock_first(*first);
  std::unique_lock lock_second(*second);

  data_ = std::move(other.data_);
  other.data_ = nlohmann::json::object();
  return *this;
}

// -----------------------------------------------------------------------------
// File I/O
// -----------------------------------------------------------------------------

bool ConfigManager::LoadFromFile(const std::filesystem::path& file_path) {
  std::ifstream file(file_path);
  if (!file.is_open()) {
    return false;
  }

  nlohmann::json parsed;
  try {
    parsed = nlohmann::json::parse(file);
  } catch (const nlohmann::json::parse_error&) {
    return false;
  }

  if (!parsed.is_object()) {
    return false;
  }

  std::unique_lock lock(mutex_);
  data_ = std::move(parsed);
  return true;
}

bool ConfigManager::SaveToFile(const std::filesystem::path& file_path) const {
  std::shared_lock lock(mutex_);
  std::ofstream file(file_path);
  if (!file.is_open()) {
    return false;
  }
  // Pretty-print with 2-space indent.
  file << data_.dump(2);
  return file.good();
}

bool ConfigManager::LoadFromString(std::string_view json_text) {
  nlohmann::json parsed;
  try {
    parsed = nlohmann::json::parse(json_text);
  } catch (const nlohmann::json::parse_error&) {
    return false;
  }

  if (!parsed.is_object()) {
    return false;
  }

  std::unique_lock lock(mutex_);
  data_ = std::move(parsed);
  return true;
}

// -----------------------------------------------------------------------------
// Key inspection / removal
// -----------------------------------------------------------------------------

bool ConfigManager::Has(std::string_view key) const {
  std::shared_lock lock(mutex_);
  const auto parts = SplitKey(key);
  if (parts.empty()) {
    return false;
  }
  return Resolve(data_, parts) != nullptr;
}

bool ConfigManager::Remove(std::string_view key) {
  std::unique_lock lock(mutex_);
  auto parts = SplitKey(key);
  if (parts.empty()) {
    return false;
  }

  // Navigate to the parent of the target key.
  nlohmann::json* current = &data_;
  for (std::size_t i = 0; i + 1 < parts.size(); ++i) {
    auto it = current->find(parts[i]);
    if (it == current->end() || !it->is_object()) {
      return false;
    }
    current = &(*it);
  }

  const auto& leaf = parts.back();
  auto it = current->find(leaf);
  if (it == current->end()) {
    return false;
  }
  current->erase(it);
  return true;
}

std::vector<std::string> ConfigManager::Keys() const {
  std::shared_lock lock(mutex_);
  std::vector<std::string> result;
  if (data_.is_object()) {
    result.reserve(data_.size());
    for (auto it = data_.begin(); it != data_.end(); ++it) {
      result.push_back(it.key());
    }
  }
  return result;
}

void ConfigManager::Clear() {
  std::unique_lock lock(mutex_);
  data_ = nlohmann::json::object();
}

nlohmann::json ConfigManager::GetJson() const {
  std::shared_lock lock(mutex_);
  return data_;
}

// -----------------------------------------------------------------------------
// Private helpers
// -----------------------------------------------------------------------------

std::vector<std::string> ConfigManager::SplitKey(std::string_view key) {
  std::vector<std::string> parts;
  if (key.empty()) {
    return parts;
  }

  std::size_t start = 0;
  while (start < key.size()) {
    std::size_t dot_pos = key.find('.', start);
    if (dot_pos == std::string_view::npos) {
      parts.emplace_back(key.substr(start));
      break;
    }
    if (dot_pos > start) {
      parts.emplace_back(key.substr(start, dot_pos - start));
    }
    start = dot_pos + 1;
  }
  return parts;
}

const nlohmann::json* ConfigManager::Resolve(
    const nlohmann::json& root,
    const std::vector<std::string>& parts) {
  const nlohmann::json* current = &root;
  for (const auto& part : parts) {
    if (!current->is_object()) {
      return nullptr;
    }
    auto it = current->find(part);
    if (it == current->end()) {
      return nullptr;
    }
    current = &(*it);
  }
  return current;
}

nlohmann::json* ConfigManager::ResolveOrCreate(
    nlohmann::json& root,
    const std::vector<std::string>& parts,
    std::string& leaf_out) {
  if (parts.empty()) {
    return nullptr;
  }

  leaf_out = parts.back();
  nlohmann::json* current = &root;

  // Walk/create all intermediate objects except the last component.
  for (std::size_t i = 0; i + 1 < parts.size(); ++i) {
    const auto& part = parts[i];
    auto it = current->find(part);
    if (it == current->end()) {
      // Create a new intermediate object.
      (*current)[part] = nlohmann::json::object();
      current = &(*current)[part];
    } else if (it->is_object()) {
      current = &(*it);
    } else {
      // A non-object value sits at an intermediate path -- overwrite it with
      // an object so the deeper key can be created.
      *it = nlohmann::json::object();
      current = &(*it);
    }
  }
  return current;
}

}  // namespace w3x_toolkit::core
