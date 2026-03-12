#include "parser/w3i/w3i_parser.h"

#include <cstring>
#include <string>

namespace w3x_toolkit::parser::w3i {

namespace {

// Binary reader helper for little-endian data with bounds checking.
class BinaryReader {
 public:
  BinaryReader(const uint8_t* data, size_t size)
      : data_(data), size_(size), pos_(0) {}

  bool HasBytes(size_t count) const { return pos_ + count <= size_; }
  size_t Position() const { return pos_; }
  size_t Remaining() const { return size_ - pos_; }

  int32_t ReadInt32() {
    if (!HasBytes(4)) return 0;
    int32_t value;
    std::memcpy(&value, data_ + pos_, 4);
    pos_ += 4;
    return value;
  }

  uint32_t ReadUInt32() {
    if (!HasBytes(4)) return 0;
    uint32_t value;
    std::memcpy(&value, data_ + pos_, 4);
    pos_ += 4;
    return value;
  }

  float ReadFloat() {
    if (!HasBytes(4)) return 0.0f;
    float value;
    std::memcpy(&value, data_ + pos_, 4);
    pos_ += 4;
    return value;
  }

  uint8_t ReadByte() {
    if (!HasBytes(1)) return 0;
    return data_[pos_++];
  }

  // Reads a null-terminated string.
  std::string ReadString() {
    std::string result;
    while (pos_ < size_ && data_[pos_] != '\0') {
      result += static_cast<char>(data_[pos_++]);
    }
    if (pos_ < size_) {
      ++pos_;  // skip null terminator
    }
    return result;
  }

  // Reads exactly `count` bytes as a string.
  std::string ReadFixedString(size_t count) {
    if (!HasBytes(count)) return "";
    std::string result(reinterpret_cast<const char*>(data_ + pos_), count);
    pos_ += count;
    // Trim trailing nulls.
    while (!result.empty() && result.back() == '\0') {
      result.pop_back();
    }
    return result;
  }

  void Skip(size_t count) {
    if (pos_ + count <= size_) {
      pos_ += count;
    } else {
      pos_ = size_;
    }
  }

 private:
  const uint8_t* data_;
  size_t size_;
  size_t pos_;
};

}  // namespace

core::Result<W3iData> ParseW3i(const uint8_t* data, size_t size) {
  if (data == nullptr || size < 4) {
    return std::unexpected(
        core::Error::ParseError("W3I data is too small or null"));
  }

  BinaryReader reader(data, size);
  W3iData w3i;

  w3i.file_version = reader.ReadInt32();

  // Validate version.
  if (w3i.file_version != 18 &&
      (w3i.file_version < 25 || w3i.file_version > 31)) {
    return std::unexpected(core::Error::InvalidFormat(
        "Unsupported W3I version: " + std::to_string(w3i.file_version)));
  }

  int32_t version = w3i.file_version;

  // Map header.
  if (version >= 28) {
    w3i.map_version = reader.ReadInt32();
    w3i.editor_version = reader.ReadInt32();
    w3i.game_version[0] = reader.ReadInt32();
    w3i.game_version[1] = reader.ReadInt32();
    w3i.game_version[2] = reader.ReadInt32();
    w3i.game_version[3] = reader.ReadInt32();
  } else {
    w3i.map_version = reader.ReadInt32();
    w3i.editor_version = reader.ReadInt32();
  }

  w3i.map_name = reader.ReadString();
  w3i.author = reader.ReadString();
  w3i.description = reader.ReadString();
  w3i.players_recommended = reader.ReadString();

  // Camera bounds (8 floats).
  for (int i = 0; i < 8; ++i) {
    w3i.camera.bounds[i] = reader.ReadFloat();
  }
  // Camera complements (4 int32s).
  for (int i = 0; i < 4; ++i) {
    w3i.camera.complements[i] = reader.ReadInt32();
  }

  // Map dimensions.
  w3i.map_width = reader.ReadInt32();
  w3i.map_height = reader.ReadInt32();

  // Map flags.
  uint32_t flags = reader.ReadUInt32();
  w3i.flags.disable_preview = (flags >> 0) & 1;
  w3i.flags.custom_ally = (flags >> 1) & 1;
  w3i.flags.melee_map = (flags >> 2) & 1;
  w3i.flags.large_map = (flags >> 3) & 1;
  w3i.flags.masked_area_show_terrain = (flags >> 4) & 1;
  w3i.flags.fix_force_setting = (flags >> 5) & 1;
  w3i.flags.custom_force = (flags >> 6) & 1;
  w3i.flags.custom_techtree = (flags >> 7) & 1;
  w3i.flags.custom_ability = (flags >> 8) & 1;
  w3i.flags.custom_upgrade = (flags >> 9) & 1;
  w3i.flags.map_menu_mark = (flags >> 10) & 1;
  w3i.flags.show_wave_on_cliff = (flags >> 11) & 1;
  w3i.flags.show_wave_on_rolling = (flags >> 12) & 1;

  // Main ground tileset.
  w3i.main_ground = static_cast<char>(reader.ReadByte());

  // Version-dependent sections.
  if (version >= 25) {
    // Loading screen (TFT).
    w3i.loading_screen.id = reader.ReadInt32();
    w3i.loading_screen.path = reader.ReadString();
    w3i.loading_screen.text = reader.ReadString();
    w3i.loading_screen.title = reader.ReadString();
    w3i.loading_screen.subtitle = reader.ReadString();

    // Game data setting.
    w3i.game_data_setting = reader.ReadInt32();

    // Prologue.
    w3i.prologue.path = reader.ReadString();
    w3i.prologue.text = reader.ReadString();
    w3i.prologue.title = reader.ReadString();
    w3i.prologue.subtitle = reader.ReadString();

    // Fog settings.
    w3i.fog.type = reader.ReadInt32();
    w3i.fog.start_z = reader.ReadFloat();
    w3i.fog.end_z = reader.ReadFloat();
    w3i.fog.density = reader.ReadFloat();
    w3i.fog.color[0] = reader.ReadByte();
    w3i.fog.color[1] = reader.ReadByte();
    w3i.fog.color[2] = reader.ReadByte();
    w3i.fog.color[3] = reader.ReadByte();

    // Environment.
    w3i.environment.weather_id = reader.ReadFixedString(4);
    w3i.environment.sound = reader.ReadString();
    w3i.environment.light = static_cast<char>(reader.ReadByte());
    w3i.environment.water_color[0] = reader.ReadByte();
    w3i.environment.water_color[1] = reader.ReadByte();
    w3i.environment.water_color[2] = reader.ReadByte();
    w3i.environment.water_color[3] = reader.ReadByte();

    // Script type (version >= 28).
    if (version >= 28) {
      w3i.script_type = reader.ReadInt32();
    }

    // Unknown fields in version >= 31.
    if (version >= 31) {
      reader.ReadInt32();  // unknown_10
      reader.ReadInt32();  // unknown_11
    }
  } else if (version == 18) {
    // Loading screen (RoC).
    w3i.loading_screen.id = reader.ReadInt32();
    w3i.loading_screen.text = reader.ReadString();
    w3i.loading_screen.title = reader.ReadString();
    w3i.loading_screen.subtitle = reader.ReadString();

    // Prologue (RoC).
    w3i.prologue.id = reader.ReadInt32();
    w3i.prologue.text = reader.ReadString();
    w3i.prologue.title = reader.ReadString();
    w3i.prologue.subtitle = reader.ReadString();
  }

  // Player slots.
  int32_t player_count = reader.ReadInt32();
  w3i.players.reserve(static_cast<size_t>(player_count));
  for (int32_t i = 0; i < player_count; ++i) {
    PlayerSlot slot;
    slot.player_id = reader.ReadInt32();
    slot.type = reader.ReadInt32();
    slot.race = reader.ReadInt32();
    slot.fix_start = reader.ReadInt32();
    slot.name = reader.ReadString();
    slot.start_x = reader.ReadFloat();
    slot.start_y = reader.ReadFloat();
    slot.ally_low_flag = reader.ReadUInt32();
    slot.ally_high_flag = reader.ReadUInt32();

    if (version >= 31) {
      reader.ReadInt32();  // unknown_12
      reader.ReadInt32();  // unknown_13
    }

    w3i.players.push_back(std::move(slot));
  }

  // Forces.
  int32_t force_count = reader.ReadInt32();
  w3i.forces.reserve(static_cast<size_t>(force_count));
  for (int32_t i = 0; i < force_count; ++i) {
    Force force;
    uint32_t force_flags = reader.ReadUInt32();
    force.allied = (force_flags >> 0) & 1;
    force.allied_victory = (force_flags >> 1) & 1;
    force.share_vision = (force_flags >> 3) & 1;
    force.share_control = (force_flags >> 4) & 1;
    force.share_advanced = (force_flags >> 5) & 1;
    force.player_mask = reader.ReadUInt32();
    force.name = reader.ReadString();
    w3i.forces.push_back(std::move(force));
  }

  // Upgrade availability.
  if (reader.Remaining() > 0) {
    // Check for end marker (0xFF byte).
    if (reader.Remaining() >= 1 &&
        *(data + reader.Position()) != 0xFF) {
      int32_t upgrade_count = reader.ReadInt32();
      w3i.upgrades.reserve(static_cast<size_t>(upgrade_count));
      for (int32_t i = 0; i < upgrade_count; ++i) {
        UpgradeAvailability ua;
        ua.player_mask = reader.ReadUInt32();
        ua.upgrade_id = reader.ReadFixedString(4);
        ua.level = reader.ReadInt32();
        ua.availability = reader.ReadInt32();
        w3i.upgrades.push_back(std::move(ua));
      }
    }
  }

  // Tech availability.
  if (reader.Remaining() > 0) {
    if (reader.Remaining() >= 1 &&
        *(data + reader.Position()) != 0xFF) {
      int32_t tech_count = reader.ReadInt32();
      w3i.techs.reserve(static_cast<size_t>(tech_count));
      for (int32_t i = 0; i < tech_count; ++i) {
        TechAvailability ta;
        ta.player_mask = reader.ReadUInt32();
        ta.tech_id = reader.ReadFixedString(4);
        w3i.techs.push_back(std::move(ta));
      }
    }
  }

  return w3i;
}

core::Result<W3iData> ParseW3i(const std::vector<uint8_t>& data) {
  return ParseW3i(data.data(), data.size());
}

}  // namespace w3x_toolkit::parser::w3i
