// Copyright 2026 W3X Toolkit Authors
//
// Licensed under the MIT License.

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "cli/commands/analyze_command.h"
#include "cli/commands/convert_command.h"
#include "cli/commands/extract_command.h"
#include "core/filesystem/filesystem_utils.h"

namespace fs = std::filesystem;

namespace {

class TemporaryDirectory {
 public:
  TemporaryDirectory() {
    const auto unique_suffix =
        std::to_string(std::chrono::steady_clock::now()
                           .time_since_epoch()
                           .count());
    path_ = fs::temp_directory_path() /
            ("w3x_toolkit_smoke_tests_" + unique_suffix);
    fs::create_directories(path_);
  }

  ~TemporaryDirectory() {
    std::error_code ec;
    fs::remove_all(path_, ec);
  }

  const fs::path& path() const { return path_; }

 private:
  fs::path path_;
};

void Expect(bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

void ExpectFileExists(const fs::path& path) {
  Expect(fs::exists(path), "Expected file to exist: " + path.string());
}

void WriteText(const fs::path& path, const std::string& content) {
  auto result = w3x_toolkit::core::FilesystemUtils::WriteTextFile(path, content);
  if (!result.has_value()) {
    throw std::runtime_error(result.error());
  }
}

std::vector<std::uint8_t> BuildMinimalW3i() {
  std::vector<std::uint8_t> data;

  auto append_int32 = [&data](std::int32_t value) {
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(&value);
    data.insert(data.end(), bytes, bytes + sizeof(value));
  };
  auto append_uint32 = [&data](std::uint32_t value) {
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(&value);
    data.insert(data.end(), bytes, bytes + sizeof(value));
  };
  auto append_float = [&data](float value) {
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(&value);
    data.insert(data.end(), bytes, bytes + sizeof(value));
  };
  auto append_byte = [&data](std::uint8_t value) {
    data.push_back(value);
  };
  auto append_string = [&data](std::string_view value) {
    data.insert(data.end(), value.begin(), value.end());
    data.push_back(0);
  };
  auto append_fixed_string = [&data](std::string_view value, std::size_t size) {
    const auto actual_size = std::min(value.size(), size);
    data.insert(data.end(), value.begin(), value.begin() + actual_size);
    data.insert(data.end(), size - actual_size, 0);
  };

  append_int32(28);
  append_int32(1);
  append_int32(1);
  append_int32(1);
  append_int32(0);
  append_int32(0);
  append_int32(0);
  append_string("Smoke Test Map");
  append_string("Codex");
  append_string("Directory-mode smoke test");
  append_string("1v1");

  for (int i = 0; i < 8; ++i) {
    append_float(0.0f);
  }
  for (int i = 0; i < 4; ++i) {
    append_int32(0);
  }

  append_int32(64);
  append_int32(64);
  append_uint32(0);
  append_byte(static_cast<std::uint8_t>('A'));

  append_int32(0);
  append_string("");
  append_string("");
  append_string("");
  append_string("");
  append_int32(0);

  append_string("");
  append_string("");
  append_string("");
  append_string("");

  append_int32(0);
  append_float(0.0f);
  append_float(0.0f);
  append_float(0.0f);
  append_byte(0);
  append_byte(0);
  append_byte(0);
  append_byte(0);

  append_fixed_string("", 4);
  append_string("");
  append_byte(0);
  append_byte(0);
  append_byte(0);
  append_byte(0);
  append_byte(0);

  append_int32(0);

  append_int32(1);
  append_int32(0);
  append_int32(1);
  append_int32(1);
  append_int32(0);
  append_string("Player 1");
  append_float(0.0f);
  append_float(0.0f);
  append_uint32(0);
  append_uint32(0);

  append_int32(0);

  return data;
}

void WriteBinary(const fs::path& path, const std::vector<std::uint8_t>& content) {
  auto result =
      w3x_toolkit::core::FilesystemUtils::WriteBinaryFile(path, content);
  if (!result.has_value()) {
    throw std::runtime_error(result.error());
  }
}

void TestDirectoryModeCommands() {
  TemporaryDirectory temp;
  const fs::path map_dir = temp.path() / "map_dir";
  const fs::path convert_out = temp.path() / "converted";
  const fs::path extract_out = temp.path() / "extracted";
  fs::create_directories(map_dir / "scripts");
  fs::create_directories(map_dir / "units");
  fs::create_directories(map_dir / "ui");

  WriteBinary(map_dir / "war3map.w3i", BuildMinimalW3i());
  WriteText(map_dir / "scripts" / "war3map.j", "function main takes nothing returns nothing\nendfunction\n");
  WriteText(map_dir / "units" / "war3map.w3u", "raw object data");
  WriteText(map_dir / "war3map.wts", "STRING 1\n{\nHello\n}\n");
  WriteText(map_dir / "ui" / "Frame.fdf", "Frame \"SIMPLEFRAME\" \"Test\" {}\n");

  w3x_toolkit::cli::ConvertCommand convert;
  auto convert_result =
      convert.Execute({map_dir.string(), convert_out.string()});
  Expect(convert_result.has_value(), "Convert command should succeed");
  ExpectFileExists(convert_out / "map" / "war3map.w3i");
  ExpectFileExists(convert_out / "map" / "war3map.wts");
  ExpectFileExists(convert_out / "table" / "units" / "war3map.ini");
  ExpectFileExists(convert_out / "trigger" / "scripts" / "war3map.j");
  ExpectFileExists(convert_out / "w3x2lni.ini");

  w3x_toolkit::cli::ExtractCommand extract;
  auto extract_result =
      extract.Execute({map_dir.string(), extract_out.string(), "--type=scripts"});
  Expect(extract_result.has_value(), "Extract command should succeed");
  ExpectFileExists(extract_out / "scripts" / "war3map.j");
  Expect(!fs::exists(extract_out / "ui" / "Frame.fdf"),
         "Script-only extraction should not copy UI files");

  w3x_toolkit::cli::AnalyzeCommand analyze;
  auto analyze_result = analyze.Execute({map_dir.string()});
  Expect(analyze_result.has_value(), "Analyze command should succeed");
}

void TestPackedArchiveRejection() {
  TemporaryDirectory temp;
  const fs::path packed_map = temp.path() / "test_map.w3x";
  WriteText(packed_map, "not a real archive");

  w3x_toolkit::cli::ConvertCommand convert;
  auto convert_result =
      convert.Execute({packed_map.string(), (temp.path() / "out").string()});
  Expect(!convert_result.has_value(), "Packed map input should be rejected");
}

}  // namespace

int main() {
  try {
    TestDirectoryModeCommands();
    TestPackedArchiveRejection();
    std::cout << "Smoke tests passed." << std::endl;
    return 0;
  } catch (const std::exception& ex) {
    std::cerr << "Smoke tests failed: " << ex.what() << std::endl;
    return 1;
  }
}
