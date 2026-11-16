#include "textcap.h"

#include <windows.h>

#include <mutex>

#include "MinHook.h"
#include "game.h"
#include "log.h"
#include "sc3text.h"

namespace sga {
namespace TextCap {

namespace {

// Text draw functions (cdecl), found by CoZ sghd-patch signatures (verified
// unique on this build, tools/re/sigscan.py); argument lists from CoZ
// LanguageBarrier GameText.cpp.
using DrawSingleTextLineProc = int(__cdecl*)(int tex, int x, int y, unsigned a4, const uint8_t* s, int maxLen,
                                             int color, int glyph, int opacity);
using DrawPhoneTextProc = int(__cdecl*)(int tex, int x, int y, int lineLength, const uint8_t* s, int skip,
                                        int count, int color, int glyph, int opacity);
using DrawInteractiveMailProc = int(__cdecl*)(int tex, int x, int y, int lineLength, const uint8_t* s,
                                              unsigned skip, unsigned count, int color, unsigned glyph,
                                              int opacity, int unselColor, int selColor, int selectedLink);

DrawSingleTextLineProc g_line = nullptr;
DrawPhoneTextProc g_phone = nullptr;
DrawInteractiveMailProc g_mail = nullptr;

std::mutex g_mu;
std::vector<DrawnText> g_building, g_last;
bool g_debug = false;
std::string g_lastDump;

// Link words in mails are marked like ruby without reading text:
// 0x09 (start) ... 0x0B (end), e.g. _MAIL.SCX "\rs;report\re;".
std::vector<std::string> Links(const uint8_t* s) {
    std::vector<std::string> out;
    const uint8_t* p = s;
    for (int guard = 0; guard < 4096 && *p != 0xFF; ++guard) {
        if (*p >= 0x80) {
            p += 2;
            continue;
        }
        if (*p == 0x09) {
            const uint8_t* start = p + 1;
            const uint8_t* q = start;
            while (*q != 0xFF && *q != 0x0B && *q != 0x0A) q += *q >= 0x80 ? 2 : 1;
            if (*q == 0x0B) {
                std::vector<uint8_t> tmp(start, q);
                tmp.push_back(0xFF);
                out.push_back(Sc3::Decode(tmp.data()).text);
            }
            p = q;
            continue;
        }
        switch (*p) {
            case 0x04: case 0x15: p = Sc3::SkipExpr(p + 1); break;
            case 0x07: case 0x1B: p += 2; break;
            case 0x0C: case 0x11: case 0x12: case 0x13: case 0x14: case 0x16: case 0x19: case 0x1A: p += 3; break;
            default: ++p; break;
        }
    }
    return out;
}

void Record(int func, int x, int y, uint32_t color, const uint8_t* s, int selectedLink) {
    if (!s) return;
    DrawnText d;
    d.func = func;
    d.x = x;
    d.y = y;
    d.color = color;
    d.selectedLink = selectedLink;
    Sc3Line l = Sc3::Decode(s, 4096);
    d.text = l.name.empty() ? l.text : l.name + ": " + l.text;
    if (func == 2) d.links = Links(s);
    if (d.text.empty()) return;
    std::lock_guard<std::mutex> lock(g_mu);
    if (g_building.size() < 512) g_building.push_back(std::move(d));
}

void SafeRecord(int func, int x, int y, uint32_t color, const uint8_t* s, int selectedLink) {
    __try {
        Record(func, x, y, color, s, selectedLink);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

int __cdecl HookLine(int tex, int x, int y, unsigned a4, const uint8_t* s, int maxLen, int color, int glyph,
                     int opacity) {
    if (opacity > 0) SafeRecord(0, x, y, (uint32_t)color, s, -1);
    return g_line(tex, x, y, a4, s, maxLen, color, glyph, opacity);
}

int __cdecl HookPhone(int tex, int x, int y, int lineLength, const uint8_t* s, int skip, int count, int color,
                      int glyph, int opacity) {
    if (opacity > 0) SafeRecord(1, x, y, (uint32_t)color, s, -1);
    return g_phone(tex, x, y, lineLength, s, skip, count, color, glyph, opacity);
}

int __cdecl HookMail(int tex, int x, int y, int lineLength, const uint8_t* s, unsigned skip, unsigned count,
                     int color, unsigned glyph, int opacity, int unselColor, int selColor, int selectedLink) {
    if (opacity > 0) SafeRecord(2, x, y, (uint32_t)color, s, selectedLink);
    return g_mail(tex, x, y, lineLength, s, skip, count, color, glyph, opacity, unselColor, selColor, selectedLink);
}

bool Hook(uintptr_t target, void* detour, void** orig, const char* name) {
    if (!target) return false;
    if (MH_CreateHook((void*)target, detour, orig) != MH_OK || MH_EnableHook((void*)target) != MH_OK) {
        Log("text hook FAILED: %s", name);
        return false;
    }
    Log("text hook ok: %s at %p", name, (void*)target);
    return true;
}

}  // namespace

bool Install() {
    MH_STATUS st = MH_Initialize();
    if (st != MH_OK && st != MH_ERROR_ALREADY_INITIALIZED) return false;
    bool a = Hook(Game::Find("drawSingleTextLine"), (void*)&HookLine, (void**)&g_line, "drawSingleTextLine");
    bool b = Hook(Game::Find("drawPhoneText"), (void*)&HookPhone, (void**)&g_phone, "drawPhoneText");
    bool c = Hook(Game::Find("sghdDrawInteractiveMail"), (void*)&HookMail, (void**)&g_mail, "drawInteractiveMail");
    return a && b && c;
}

void EndFrame() {
    std::lock_guard<std::mutex> lock(g_mu);
    g_last.swap(g_building);
    g_building.clear();
    if (g_debug) {
        std::string dump;
        for (auto& d : g_last) {
            char head[96];
            sprintf_s(head, "[%d %d,%d c%08X l%d] ", d.func, d.x, d.y, d.color, d.selectedLink);
            dump += head + d.text;
            for (auto& l : d.links) dump += " {" + l + "}";
            dump += "\n";
        }
        if (dump != g_lastDump) {
            g_lastDump = dump;
            Log("drawn text:\n%s", dump.c_str());
        }
    }
}

const std::vector<DrawnText>& LastFrame() { return g_last; }

void SetDebugLog(bool on) { g_debug = on; }

}  // namespace TextCap
}  // namespace sga
