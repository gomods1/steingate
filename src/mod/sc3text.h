#pragma once
#include <cstdint>
#include <string>

namespace sga {

// SC3 string decoding. Format (MgsScriptTools stringtags/base.yaml): bytes
// >= 0x80 are 2-byte glyph codes ((b0 & 0x7F) << 8 | b1) into the game's
// charset; bytes < 0x80 are tags; 0xFF ends the string.
struct Sc3Line {
    std::string name;  // speaker, empty for narration
    std::string text;  // UTF-8, half-width, single spaces
};

namespace Sc3 {
bool LoadCharset(const std::wstring& path);
Sc3Line Decode(const uint8_t* s, size_t maxBytes = 8192);
// Skips one SC3 expression (used by instruction operand parsing).
const uint8_t* SkipExpr(const uint8_t* p);
// Value of an expression made of a single literal, else false.
bool ConstExpr(const uint8_t* p, int* out);
}  // namespace Sc3

}  // namespace sga
