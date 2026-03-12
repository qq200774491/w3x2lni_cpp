#ifndef W3X_TOOLKIT_PARSER_W3U_W3U_PARSER_H_
#define W3X_TOOLKIT_PARSER_W3U_W3U_PARSER_H_

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#include "core/error/error.h"

namespace w3x_toolkit::parser::w3u {

// The type of an object modification's value.
enum class ModificationType : int32_t {
  kInteger = 0,
  kReal = 1,
  kUnreal = 2,  // bounded real [0,1] in some contexts, stored as float
  kString = 3,
};

// A single field modification within an object definition.
struct Modification {
  std::string field_id;  // 4-char rawcode identifying the field
  ModificationType type = ModificationType::kInteger;
  int32_t level = 0;           // level/variation (0 if not applicable)
  int32_t data_pointer = 0;    // data pointer / column index
  std::variant<int32_t, float, std::string> value;
};

// A single object definition (original or custom).
struct ObjectDef {
  std::string original_id;  // 4-char base object rawcode
  std::string custom_id;    // 4-char custom ID ("\0\0\0\0" if not custom)
  std::vector<Modification> modifications;
};

// Container for all objects parsed from a W3U/W3T/W3B/W3D/W3A/W3H/W3Q file.
struct ObjectData {
  int32_t version = 0;
  std::vector<ObjectDef> original_objects;  // modifications to stock objects
  std::vector<ObjectDef> custom_objects;    // user-created objects
};

// Whether the object type uses level/data-pointer fields in modifications.
// W3A (ability), W3D (doodad), W3H (buff), W3Q (upgrade) do.
// W3U (unit), W3T (item), W3B (destructable) do not.
enum class ObjectFileKind {
  kSimple,     // W3U, W3T, W3B - no level info in modifications
  kComplex,    // W3A, W3D, W3H, W3Q - level + data_pointer per modification
};

// Parses a binary object data file (W3U/W3T/W3B/W3D/W3A/W3H/W3Q).
// The |kind| parameter specifies whether the format includes level info.
core::Result<ObjectData> ParseObjectFile(const uint8_t* data, size_t size,
                                         ObjectFileKind kind);

// Convenience overload accepting a byte vector.
core::Result<ObjectData> ParseObjectFile(const std::vector<uint8_t>& data,
                                         ObjectFileKind kind);

// Convenience wrappers for specific file types.
inline core::Result<ObjectData> ParseW3u(const std::vector<uint8_t>& data) {
  return ParseObjectFile(data, ObjectFileKind::kSimple);
}
inline core::Result<ObjectData> ParseW3t(const std::vector<uint8_t>& data) {
  return ParseObjectFile(data, ObjectFileKind::kSimple);
}
inline core::Result<ObjectData> ParseW3b(const std::vector<uint8_t>& data) {
  return ParseObjectFile(data, ObjectFileKind::kSimple);
}
inline core::Result<ObjectData> ParseW3a(const std::vector<uint8_t>& data) {
  return ParseObjectFile(data, ObjectFileKind::kComplex);
}
inline core::Result<ObjectData> ParseW3d(const std::vector<uint8_t>& data) {
  return ParseObjectFile(data, ObjectFileKind::kComplex);
}
inline core::Result<ObjectData> ParseW3h(const std::vector<uint8_t>& data) {
  return ParseObjectFile(data, ObjectFileKind::kComplex);
}
inline core::Result<ObjectData> ParseW3q(const std::vector<uint8_t>& data) {
  return ParseObjectFile(data, ObjectFileKind::kComplex);
}

}  // namespace w3x_toolkit::parser::w3u

#endif  // W3X_TOOLKIT_PARSER_W3U_W3U_PARSER_H_
