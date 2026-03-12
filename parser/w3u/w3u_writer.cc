#include "parser/w3u/w3u_writer.h"

#include <cstring>
#include <string>
#include <variant>

namespace w3x_toolkit::parser::w3u {

namespace {

void AppendInt32(std::vector<std::uint8_t>& buffer, std::int32_t value) {
  const auto* bytes = reinterpret_cast<const std::uint8_t*>(&value);
  buffer.insert(buffer.end(), bytes, bytes + sizeof(value));
}

void AppendFloat(std::vector<std::uint8_t>& buffer, float value) {
  const auto* bytes = reinterpret_cast<const std::uint8_t*>(&value);
  buffer.insert(buffer.end(), bytes, bytes + sizeof(value));
}

core::Result<void> AppendRawcode(std::vector<std::uint8_t>& buffer,
                                 const std::string& rawcode,
                                 const char* label) {
  if (rawcode.size() > 4) {
    return std::unexpected(core::Error::InvalidFormat(
        std::string(label) + " rawcode is longer than 4 bytes: " + rawcode));
  }
  buffer.insert(buffer.end(), rawcode.begin(), rawcode.end());
  buffer.insert(buffer.end(), 4 - rawcode.size(), 0);
  return {};
}

void AppendString(std::vector<std::uint8_t>& buffer, const std::string& value) {
  buffer.insert(buffer.end(), value.begin(), value.end());
  buffer.push_back(0);
}

core::Result<void> AppendModification(std::vector<std::uint8_t>& buffer,
                                      const Modification& modification,
                                      ObjectFileKind kind) {
  W3X_RETURN_IF_ERROR(
      AppendRawcode(buffer, modification.field_id, "Modification field"));
  AppendInt32(buffer, static_cast<std::int32_t>(modification.type));
  if (kind == ObjectFileKind::kComplex) {
    AppendInt32(buffer, modification.level);
    AppendInt32(buffer, modification.data_pointer);
  }

  switch (modification.type) {
    case ModificationType::kInteger: {
      if (!std::holds_alternative<std::int32_t>(modification.value)) {
        return std::unexpected(core::Error::InvalidFormat(
            "Integer modification does not contain an integer value"));
      }
      AppendInt32(buffer, std::get<std::int32_t>(modification.value));
      break;
    }
    case ModificationType::kReal:
    case ModificationType::kUnreal: {
      if (!std::holds_alternative<float>(modification.value)) {
        return std::unexpected(core::Error::InvalidFormat(
            "Float modification does not contain a float value"));
      }
      AppendFloat(buffer, std::get<float>(modification.value));
      break;
    }
    case ModificationType::kString: {
      if (!std::holds_alternative<std::string>(modification.value)) {
        return std::unexpected(core::Error::InvalidFormat(
            "String modification does not contain a string value"));
      }
      AppendString(buffer, std::get<std::string>(modification.value));
      break;
    }
  }

  buffer.insert(buffer.end(), 4, 0);
  return {};
}

core::Result<void> AppendObject(std::vector<std::uint8_t>& buffer,
                                const ObjectDef& object,
                                ObjectFileKind kind,
                                bool custom_chunk) {
  const std::string custom_id = custom_chunk ? object.custom_id : std::string();
  W3X_RETURN_IF_ERROR(
      AppendRawcode(buffer, object.original_id, "Object original"));
  W3X_RETURN_IF_ERROR(AppendRawcode(buffer, custom_id, "Object custom"));
  AppendInt32(buffer,
              static_cast<std::int32_t>(object.modifications.size()));
  for (const Modification& modification : object.modifications) {
    W3X_RETURN_IF_ERROR(AppendModification(buffer, modification, kind));
  }
  return {};
}

core::Result<void> AppendChunk(std::vector<std::uint8_t>& buffer,
                               const std::vector<ObjectDef>& objects,
                               ObjectFileKind kind,
                               bool custom_chunk) {
  AppendInt32(buffer, static_cast<std::int32_t>(objects.size()));
  for (const ObjectDef& object : objects) {
    W3X_RETURN_IF_ERROR(AppendObject(buffer, object, kind, custom_chunk));
  }
  return {};
}

}  // namespace

core::Result<std::vector<std::uint8_t>> SerializeObjectFile(
    const ObjectData& data, ObjectFileKind kind) {
  if (data.version != 2) {
    return std::unexpected(core::Error::InvalidFormat(
        "Only version 2 object files are supported for serialization"));
  }

  std::vector<std::uint8_t> buffer;
  buffer.reserve(64);
  AppendInt32(buffer, data.version);
  W3X_RETURN_IF_ERROR(
      AppendChunk(buffer, data.original_objects, kind, false));
  W3X_RETURN_IF_ERROR(
      AppendChunk(buffer, data.custom_objects, kind, true));
  return buffer;
}

}  // namespace w3x_toolkit::parser::w3u
