#include "parser/w3u/w3u_parser.h"

#include <cstring>

namespace w3x_toolkit::parser::w3u {

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

  // Reads exactly 4 bytes as a rawcode string.
  std::string ReadRawcode() {
    if (!HasBytes(4)) return "\0\0\0\0";
    std::string result(reinterpret_cast<const char*>(data_ + pos_), 4);
    pos_ += 4;
    // Trim trailing nulls for display purposes.
    while (!result.empty() && result.back() == '\0') {
      result.pop_back();
    }
    return result;
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

  void Skip(size_t count) {
    pos_ = (pos_ + count <= size_) ? pos_ + count : size_;
  }

 private:
  const uint8_t* data_;
  size_t size_;
  size_t pos_;
};

// Returns true if 4 bytes are all zero (null object ID).
bool IsNullId(const std::string& id) {
  if (id.empty()) return true;
  for (char c : id) {
    if (c != '\0') return false;
  }
  return true;
}

core::Result<Modification> ReadModification(BinaryReader& reader,
                                            ObjectFileKind kind) {
  Modification mod;

  if (!reader.HasBytes(4)) {
    return std::unexpected(
        core::Error::ParseError("Unexpected end of data reading field ID"));
  }
  mod.field_id = reader.ReadRawcode();

  if (!reader.HasBytes(4)) {
    return std::unexpected(
        core::Error::ParseError("Unexpected end of data reading value type"));
  }
  int32_t value_type = reader.ReadInt32();
  mod.type = static_cast<ModificationType>(value_type);

  // Complex types include level and data_pointer.
  if (kind == ObjectFileKind::kComplex) {
    if (!reader.HasBytes(8)) {
      return std::unexpected(core::Error::ParseError(
          "Unexpected end of data reading level/data_pointer"));
    }
    mod.level = reader.ReadInt32();
    mod.data_pointer = reader.ReadInt32();
  }

  // Read value based on type.
  switch (mod.type) {
    case ModificationType::kInteger:
      if (!reader.HasBytes(4)) {
        return std::unexpected(
            core::Error::ParseError("Unexpected end reading integer value"));
      }
      mod.value = reader.ReadInt32();
      break;

    case ModificationType::kReal:
    case ModificationType::kUnreal:
      if (!reader.HasBytes(4)) {
        return std::unexpected(
            core::Error::ParseError("Unexpected end reading float value"));
      }
      mod.value = reader.ReadFloat();
      break;

    case ModificationType::kString:
      mod.value = reader.ReadString();
      break;

    default:
      return std::unexpected(core::Error::InvalidFormat(
          "Unknown modification value type: " + std::to_string(value_type)));
  }

  // Skip the trailing 4 bytes (end-of-modification marker / object ID echo).
  if (!reader.HasBytes(4)) {
    return std::unexpected(
        core::Error::ParseError("Unexpected end reading modification trailer"));
  }
  reader.Skip(4);

  return mod;
}

core::Result<ObjectDef> ReadObject(BinaryReader& reader,
                                   ObjectFileKind kind) {
  ObjectDef obj;

  if (!reader.HasBytes(8)) {
    return std::unexpected(
        core::Error::ParseError("Unexpected end of data reading object IDs"));
  }
  obj.original_id = reader.ReadRawcode();
  obj.custom_id = reader.ReadRawcode();

  if (!reader.HasBytes(4)) {
    return std::unexpected(core::Error::ParseError(
        "Unexpected end of data reading modification count"));
  }
  int32_t mod_count = reader.ReadInt32();

  obj.modifications.reserve(static_cast<size_t>(mod_count));
  for (int32_t i = 0; i < mod_count; ++i) {
    auto mod_result = ReadModification(reader, kind);
    if (!mod_result.has_value()) {
      return std::unexpected(mod_result.error());
    }
    obj.modifications.push_back(std::move(mod_result.value()));
  }

  return obj;
}

core::Result<std::vector<ObjectDef>> ReadChunk(BinaryReader& reader,
                                               ObjectFileKind kind) {
  std::vector<ObjectDef> objects;

  if (reader.Remaining() < 4) {
    return objects;
  }

  int32_t count = reader.ReadInt32();
  objects.reserve(static_cast<size_t>(count));

  for (int32_t i = 0; i < count; ++i) {
    auto obj_result = ReadObject(reader, kind);
    if (!obj_result.has_value()) {
      return std::unexpected(obj_result.error());
    }
    objects.push_back(std::move(obj_result.value()));
  }

  return objects;
}

}  // namespace

core::Result<ObjectData> ParseObjectFile(const uint8_t* data, size_t size,
                                         ObjectFileKind kind) {
  if (data == nullptr || size < 4) {
    return std::unexpected(
        core::Error::ParseError("Object data is too small or null"));
  }

  BinaryReader reader(data, size);
  ObjectData result;

  result.version = reader.ReadInt32();
  if (result.version != 2) {
    return std::unexpected(core::Error::InvalidFormat(
        "Unsupported object data version: " +
        std::to_string(result.version) + " (expected 2)"));
  }

  // Read original objects table.
  auto originals = ReadChunk(reader, kind);
  if (!originals.has_value()) {
    return std::unexpected(originals.error());
  }
  result.original_objects = std::move(originals.value());

  // Read custom objects table.
  auto customs = ReadChunk(reader, kind);
  if (!customs.has_value()) {
    return std::unexpected(customs.error());
  }
  result.custom_objects = std::move(customs.value());

  return result;
}

core::Result<ObjectData> ParseObjectFile(const std::vector<uint8_t>& data,
                                         ObjectFileKind kind) {
  return ParseObjectFile(data.data(), data.size(), kind);
}

}  // namespace w3x_toolkit::parser::w3u
