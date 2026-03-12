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
#include "cli/commands/pack_command.h"
#include "cli/commands/unpack_command.h"
#include "core/filesystem/filesystem_utils.h"
#include "parser/w3u/w3u_parser.h"

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

std::string ReadText(const fs::path& path) {
  auto result = w3x_toolkit::core::FilesystemUtils::ReadTextFile(path);
  if (!result.has_value()) {
    throw std::runtime_error(result.error());
  }
  return result.value();
}

std::vector<std::uint8_t> ReadBinary(const fs::path& path) {
  auto result = w3x_toolkit::core::FilesystemUtils::ReadBinaryFile(path);
  if (!result.has_value()) {
    throw std::runtime_error(result.error());
  }
  return result.value();
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
  fs::create_directories(map_dir / "textures");
  fs::create_directories(map_dir / "sound");

  WriteBinary(map_dir / "war3map.w3i", BuildMinimalW3i());
  WriteText(map_dir / "scripts" / "war3map.j", "function main takes nothing returns nothing\nendfunction\n");
  WriteText(map_dir / "units" / "war3map.w3u", "raw object data");
  WriteText(map_dir / "war3map.wts", "STRING 1\n{\nHello\n}\n");
  WriteText(map_dir / "ui" / "Frame.fdf", "Frame \"SIMPLEFRAME\" \"Test\" {}\n");
  WriteText(map_dir / "textures" / "Preview.blp", "fake texture");
  WriteText(map_dir / "sound" / "Theme.mp3", "fake sound");

  w3x_toolkit::cli::ConvertCommand convert;
  auto convert_result =
      convert.Execute({map_dir.string(), convert_out.string()});
  Expect(convert_result.has_value(), "Convert command should succeed");
  ExpectFileExists(convert_out / ".w3x");
  ExpectFileExists(convert_out / "map" / "war3map.w3i");
  ExpectFileExists(convert_out / "map" / "war3map.wts");
  ExpectFileExists(convert_out / "table" / "units" / "war3map.ini");
  ExpectFileExists(convert_out / "trigger" / "scripts" / "war3map.j");
  ExpectFileExists(convert_out / "resource" / "textures" / "Preview.blp");
  ExpectFileExists(convert_out / "sound" / "sound" / "Theme.mp3");
  ExpectFileExists(convert_out / "table" / "w3i.ini");
  ExpectFileExists(convert_out / "table" / "imp.ini");
  ExpectFileExists(convert_out / "w3x2lni" / "locale" / "w3i.lng");
  ExpectFileExists(convert_out / "w3x2lni" / "locale" / "lml.lng");
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
  Expect(!convert_result.has_value(),
         "Invalid packed map input should still be rejected");
}

void TestPackedArchiveCommands() {
  TemporaryDirectory temp;
  const fs::path map_dir = temp.path() / "packed_commands_input";
  const fs::path packed_map = temp.path() / "packed_commands_map.w3x";
  const fs::path convert_out = temp.path() / "packed_converted";
  const fs::path extract_out = temp.path() / "packed_extracted";

  fs::create_directories(map_dir / "scripts");
  WriteBinary(map_dir / "war3map.w3i", BuildMinimalW3i());
  WriteText(map_dir / "war3map.j",
            "function main takes nothing returns nothing\nendfunction\n");
  WriteText(map_dir / "scripts" / "helper.lua", "print('helper')\n");
  WriteText(map_dir / "war3map.wts", "STRING 1\n{\nPacked\n}\n");

  w3x_toolkit::cli::PackCommand pack;
  auto pack_result = pack.Execute({map_dir.string(), packed_map.string()});
  Expect(pack_result.has_value(), "Pack command should create a packed map");

  w3x_toolkit::cli::AnalyzeCommand analyze;
  auto analyze_result = analyze.Execute({packed_map.string()});
  Expect(analyze_result.has_value(),
         "Analyze command should accept packed map input");

  w3x_toolkit::cli::ExtractCommand extract;
  auto extract_result = extract.Execute(
      {packed_map.string(), extract_out.string(), "--type=scripts"});
  Expect(extract_result.has_value(),
         "Extract command should accept packed map input");
  ExpectFileExists(extract_out / "war3map.j");
  ExpectFileExists(extract_out / "scripts" / "helper.lua");

  w3x_toolkit::cli::ConvertCommand convert;
  auto convert_result = convert.Execute(
      {packed_map.string(), convert_out.string(), "--no-table-data"});
  Expect(convert_result.has_value(),
         "Convert command should accept packed map input");
  ExpectFileExists(convert_out / ".w3x");
  ExpectFileExists(convert_out / "map" / "war3map.w3i");
  ExpectFileExists(convert_out / "map" / "war3map.wts");
  ExpectFileExists(convert_out / "trigger" / "war3map.j");
  ExpectFileExists(convert_out / "trigger" / "scripts" / "helper.lua");
  ExpectFileExists(convert_out / "table" / "w3i.ini");
  ExpectFileExists(convert_out / "table" / "imp.ini");
  ExpectFileExists(convert_out / "w3x2lni" / "locale" / "w3i.lng");
  ExpectFileExists(convert_out / "w3x2lni" / "locale" / "lml.lng");
  ExpectFileExists(convert_out / "w3x2lni.ini");
}

void TestPackUnpackRoundTrip() {
  TemporaryDirectory temp;
  const fs::path map_dir = temp.path() / "packed_input";
  const fs::path packed_map = temp.path() / "packed_map.w3x";
  const fs::path unpacked_dir = temp.path() / "unpacked";
  const fs::path repacked_map = temp.path() / "repacked_map.w3x";
  const fs::path repacked_unpacked_dir = temp.path() / "repacked_unpacked";

  fs::create_directories(map_dir / "units");

  WriteBinary(map_dir / "war3map.w3i", BuildMinimalW3i());
  WriteText(map_dir / "war3map.j",
            "function main takes nothing returns nothing\nendfunction\n");
  WriteText(map_dir / "ability.ini",
            "[A000]\nName=TestAbility\nTip=Test Tip\n");
  WriteText(map_dir / "destructable.ini", "[ATg1]\nName=TestGate\nHP=123\n");
  WriteText(map_dir / "doodad.ini", "[ACt0]\nName=TestStall\nnumVar=2\n");
  WriteText(map_dir / "misc.ini", "[HERO]\nName=Test Hero\n");
  WriteText(map_dir / "units" / "abilitydata.slk",
            "ID;PWXL;N;E\n"
            "B;X2;Y2;D0\n"
            "C;X1;Y1;K\"alias\"\n"
            "C;X2;K\"code\"\n"
            "C;X1;Y2;K\"A000\"\n"
            "C;X2;K\"ANcl\"\n"
            "E\n");

  w3x_toolkit::cli::PackCommand pack;
  auto pack_result = pack.Execute({map_dir.string(), packed_map.string()});
  Expect(pack_result.has_value(), "Pack command should succeed");
  ExpectFileExists(packed_map);

  w3x_toolkit::cli::UnpackCommand unpack;
  auto unpack_result =
      unpack.Execute({packed_map.string(), unpacked_dir.string()});
  Expect(unpack_result.has_value(), "Unpack command should succeed");
  ExpectFileExists(unpacked_dir / ".w3x");
  ExpectFileExists(unpacked_dir / "map" / "war3map.w3i");
  ExpectFileExists(unpacked_dir / "map" / "war3map.j");
  ExpectFileExists(unpacked_dir / "table" / "ability.ini");
  ExpectFileExists(unpacked_dir / "table" / "destructable.ini");
  ExpectFileExists(unpacked_dir / "table" / "doodad.ini");
  ExpectFileExists(unpacked_dir / "table" / "units" / "abilitydata.slk");
  ExpectFileExists(unpacked_dir / "map" / "war3map.w3a");
  ExpectFileExists(unpacked_dir / "map" / "war3map.w3b");
  ExpectFileExists(unpacked_dir / "map" / "war3map.w3d");
  ExpectFileExists(unpacked_dir / "map" / "war3mapmisc.txt");
  ExpectFileExists(unpacked_dir / "w3x2lni" / ".w3x_manifest.json");

  auto ability_obj =
      w3x_toolkit::parser::w3u::ParseW3a(
          ReadBinary(unpacked_dir / "map" / "war3map.w3a"));
  Expect(ability_obj.has_value(), "Generated war3map.w3a should be parseable");
  Expect(ability_obj->custom_objects.size() == 1,
         "Generated war3map.w3a should contain one custom object");
  Expect(ability_obj->custom_objects[0].original_id == "ANcl",
         "Custom object should preserve its parent rawcode");
  Expect(ability_obj->custom_objects[0].custom_id == "A000",
         "Custom object should preserve its custom rawcode");

  auto destructable_obj =
      w3x_toolkit::parser::w3u::ParseW3b(
          ReadBinary(unpacked_dir / "map" / "war3map.w3b"));
  Expect(destructable_obj.has_value(),
         "Generated war3map.w3b should be parseable");
  Expect(!destructable_obj->original_objects.empty(),
         "Generated war3map.w3b should contain one original object");

  auto doodad_obj = w3x_toolkit::parser::w3u::ParseW3d(
      ReadBinary(unpacked_dir / "map" / "war3map.w3d"));
  Expect(doodad_obj.has_value(), "Generated war3map.w3d should be parseable");
  Expect(!doodad_obj->original_objects.empty(),
         "Generated war3map.w3d should contain one original object");

  WriteText(unpacked_dir / "table" / "units" / "abilitydata.slk",
            ReadText(unpacked_dir / "table" / "units" / "abilitydata.slk") +
                "C;X1;Y2;K\"roundtrip\"\n");
  WriteText(unpacked_dir / "table" / "ability.ini",
            "[A000]\nName=UpdatedAbility\nTip=Updated Tip\n");

  auto repack_result =
      pack.Execute({unpacked_dir.string(), repacked_map.string()});
  Expect(repack_result.has_value(), "Pack command should repack unpacked dir");
  ExpectFileExists(repacked_map);

  auto re_unpacked_result =
      unpack.Execute({repacked_map.string(), repacked_unpacked_dir.string()});
  Expect(re_unpacked_result.has_value(),
         "Unpack command should unpack repacked archive");
  ExpectFileExists(repacked_unpacked_dir / ".w3x");
  ExpectFileExists(repacked_unpacked_dir / "map" / "war3map.w3a");
  ExpectFileExists(repacked_unpacked_dir / "map" / "war3map.w3b");
  ExpectFileExists(repacked_unpacked_dir / "map" / "war3map.w3d");
  ExpectFileExists(repacked_unpacked_dir / "map" / "war3mapmisc.txt");
  Expect(ReadText(repacked_unpacked_dir / "table" / "units" / "abilitydata.slk")
             .find("roundtrip") != std::string::npos,
         "Modified SLK content should survive repack");

  auto repacked_ability_obj = w3x_toolkit::parser::w3u::ParseW3a(
      ReadBinary(repacked_unpacked_dir / "map" / "war3map.w3a"));
  Expect(repacked_ability_obj.has_value(),
         "Repacked war3map.w3a should remain parseable");
  Expect(!repacked_ability_obj->custom_objects.empty(),
         "Repacked war3map.w3a should still contain the custom object");
  bool found_updated_name = false;
  for (const auto& modification :
       repacked_ability_obj->custom_objects[0].modifications) {
    if (modification.field_id == "anam" &&
        std::holds_alternative<std::string>(modification.value) &&
        std::get<std::string>(modification.value) == "UpdatedAbility") {
      found_updated_name = true;
      break;
    }
  }
  Expect(found_updated_name,
         "Repacked war3map.w3a should be regenerated from ability.ini");
}

}  // namespace

int main() {
  try {
    TestDirectoryModeCommands();
    TestPackedArchiveRejection();
    TestPackedArchiveCommands();
    TestPackUnpackRoundTrip();
    std::cout << "Smoke tests passed." << std::endl;
    return 0;
  } catch (const std::exception& ex) {
    std::cerr << "Smoke tests failed: " << ex.what() << std::endl;
    return 1;
  }
}
