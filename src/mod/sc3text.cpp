#include "sc3text.h"

#include <windows.h>

#include <fstream>
#include <sstream>
#include <vector>

#include "log.h"

namespace sga {
namespace Sc3 {

namespace {
std::vector<wchar_t> g_charset;

wchar_t ToHalfwidth(wchar_t c) {
    if (c == 0x3000) return L' ';
    if (c >= 0xFF01 && c <= 0xFF5E) return (wchar_t)(c - 0xFF01 + 0x21);
    return c;
}

void Append(std::wstring& out, wchar_t c) {
    c = ToHalfwidth(c);
    if (c == L' ' && (out.empty() || out.back() == L' ')) return;
    out += c;
}

std::string Finish(std::wstring w) {
    while (!w.empty() && w.back() == L' ') w.pop_back();
    return WideToUtf8(w);
}
}  // namespace

bool LoadCharset(const std::wstring& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        Log("charset not found");
        return false;
    }
    std::stringstream ss;
    ss << f.rdbuf();
    std::wstring w = Utf8ToWide(ss.str());
    // The file is the charset in glyph order; surrogate pairs are not used.
    g_charset.assign(w.begin(), w.end());
    while (!g_charset.empty() && (g_charset.back() == L'\n' || g_charset.back() == L'\r'))
        g_charset.pop_back();
    Log("charset: %u glyphs", (unsigned)g_charset.size());
    return true;
}

const uint8_t* SkipExpr(const uint8_t* p) {
    for (int guard = 0; guard < 256; ++guard) {
        uint8_t b = *p;
        if (b == 0) return p + 1;
        if ((b & 0x80) == 0) {  // operator: opcode, priority
            p += 2;
            continue;
        }
        int n = 1;  // literal: varint, then priority byte
        if ((b & 0x60) == 0x60) n = 5;
        else if ((b & 0x40) == 0x40) n = 3;
        else if ((b & 0x20) == 0x20) n = 2;
        p += n + 1;
    }
    return p;
}

bool ConstExpr(const uint8_t* p, int* out) {
    uint8_t b = p[0];
    if (b == 0 || (b & 0x80) == 0) return false;
    int v, n;
    if ((b & 0x60) == 0x60) {
        v = *(const int32_t*)(p + 1);
        n = 5;
    } else if ((b & 0x40) == 0x40) {
        v = ((b & 0x1F) << 16) | p[1] | (p[2] << 8);
        v = (v << 11) >> 11;
        n = 3;
    } else if ((b & 0x20) == 0x20) {
        v = ((b & 0x1F) << 8) | p[1];
        v = (v << 19) >> 19;
        n = 2;
    } else {
        v = b & 0x1F;
        v = (v << 27) >> 27;
        n = 1;
    }
    if (p[n + 1] != 0) return false;  // more tokens follow: not a constant
    *out = v;
    return true;
}

Sc3Line Decode(const uint8_t* s, size_t maxBytes) {
    std::wstring name, text;
    bool inName = false, inRuby = false;
    const uint8_t* end = s + maxBytes;
    while (s < end) {
        uint8_t b = *s;
        if (b == 0xFF) break;
        if (b >= 0x80) {
            int g = ((b & 0x7F) << 8) | s[1];
            s += 2;
            if (inRuby) continue;  // ruby (furigana) text is a reading aid, skip it
            wchar_t c = g < (int)g_charset.size() ? g_charset[g] : L'?';
            if (c >= 0xE000 && c <= 0xF8FF) continue;  // private-use emoji glyphs
            Append(inName ? name : text, c);
            continue;
        }
        ++s;
        switch (b) {
            case 0x00: Append(text, L' '); break;           // line break
            case 0x01: inName = true; break;                 // name start
            case 0x02: inName = false; break;                // name end
            case 0x04: case 0x15: s = SkipExpr(s); break;    // color, eval
            case 0x07: case 0x1B: s += 1; break;             // wait, font
            case 0x0A: inRuby = true; break;                 // ruby text start
            case 0x0B: inRuby = false; break;                // ruby end
            case 0x0C: case 0x11: case 0x12: case 0x13: case 0x14:
            case 0x16: case 0x19: case 0x1A: s += 2; break;  // int16 operand tags
            default: break;
        }
    }
    return {Finish(name), Finish(text)};
}

}  // namespace Sc3
}  // namespace sga
