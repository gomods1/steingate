#include "game.h"

#include <windows.h>

#include <cstring>
#include <string>
#include <vector>

#include "log.h"

namespace sga {

// ScriptThreadState offsets, verified with the Frida prototype
// (tools/re/proto_dialogue.py): pc at 0x14C, script buffer id at 0x74.
struct ScriptThread {
    uint8_t raw[0x150];
};

namespace Game {
namespace {

uintptr_t g_base = 0;
size_t g_textSize = 0;

using Sc3EvalProc = int(__cdecl*)(ScriptThread*, int*);
using GetStringProc = const uint8_t*(__cdecl*)(int scriptBuffer, int stringId);

Sc3EvalProc g_eval = nullptr;
GetStringProc g_getString = nullptr;
InstProc* g_tables[3] = {};
uint8_t** g_flags = nullptr;  // pointer to the flag bit array
int** g_work = nullptr;       // pointer to the ScrWork int array

// Byte pattern with ?? wildcards, matched against .text.
struct Sig {
    const char* name;
    const char* pattern;
    int occurrence;
    int offset;
};

std::vector<int> Parse(const char* p) {
    std::vector<int> out;
    while (*p) {
        if (*p == ' ') {
            ++p;
            continue;
        }
        if (p[0] == '?') {
            out.push_back(-1);
            p += 2;
        } else {
            out.push_back((int)strtoul(std::string(p, 2).c_str(), nullptr, 16));
            p += 2;
        }
    }
    return out;
}

uintptr_t Scan(const Sig& s, bool quiet = false) {
    std::vector<int> pat = Parse(s.pattern);
    const uint8_t* text = (const uint8_t*)(g_base + 0x1000);
    int found = 0;
    for (size_t i = 0; i + pat.size() <= g_textSize; ++i) {
        size_t j = 0;
        for (; j < pat.size(); ++j)
            if (pat[j] >= 0 && text[i + j] != pat[j]) break;
        if (j == pat.size() && found++ == s.occurrence) return (uintptr_t)(text + i) + s.offset;
    }
    if (!quiet) Log("signature NOT FOUND: %s", s.name);
    return 0;
}

uint32_t Read32(uintptr_t a) { return a ? *(uint32_t*)a : 0; }

// Signatures from Committee of Zero's sghd-patch gamedef.json, each checked
// unique on this build with tools/re/sigscan.py.
const Sig kEval{"sc3Eval", "55 8B EC 83 EC 10 33 C0 53 8B 5D 08 A3 ?? ?? ?? ?? 8B ?? ?? ?? ?? ?? 56 57 38 01 0F 84", 0, 0};
const Sig kGetString{"getStringFromScript", "55 8B EC 8B 45 08 56 8B ?? ?? ?? ?? ?? ?? 0F B6 4E 07 0F B6 46 06", 0, 0};
const Sig kTblUser1{"instTableUser1", "83 E8 0F 75 ?? 8B 04 8D ?? ?? ?? ??", 0, 8};
const Sig kTblGraph{"instTableGraph", "EB ?? 8B 04 8D ?? ?? ?? ?? 56 FF D0", 0, 5};
const Sig kTblSystem{"instTableSystem", "EB ?? 8B 04 8D ?? ?? ?? ?? 56 FF D0", 1, 5};
// getFlag: mov eax,[flags] sits at +0x17 (A1 imm32), see tools/re/disasm.py 70890.
const Sig kGetFlag{"getFlag", "55 8B EC 8B 4D 08 56 8B F1 C1 EE 03", 0, 0x18};
const Sig kScrWork{"useOfScrWork", "A3 ?? ?? ?? ?? FF D7 50 FF D6 68 10 27 00 00", 0, 1};

// Draw functions (CoZ sghd-patch gamedef.json; unique on this build).
const Sig kNamed[] = {
    {"drawSingleTextLine", "55 8B EC B8 ?? ?? ?? ?? E8 ?? ?? ?? ?? A1 ?? ?? ?? ?? 33 C5 89 45 FC 8B 45 1C", 0, 0},
    {"drawPhoneText", "55 8B EC B8 ?? ?? ?? ?? E8 ?? ?? ?? ?? A1 ?? ?? ?? ?? 33 C5 89 45 FC 8B 45 14 8B 4D 18 85 C0 BA ?? ?? ?? ?? 0F 44 C2 53 89 45 14", 0, 0},
    {"sghdDrawInteractiveMail", "55 8B EC B8 ?? ?? ?? ?? E8 ?? ?? ?? ?? A1 ?? ?? ?? ?? 33 C5 89 45 FC 8B 45 14 8B 4D 18 85 C0 BA ?? ?? ?? ?? 0F 44 C2 89 45 14 8B 45 0C 89 ?? ?? ?? ?? ?? 33 C0", 0, 0},
};

}  // namespace

uintptr_t Base() { return g_base; }

uintptr_t Find(const char* name) {
    for (const Sig& s : kNamed)
        if (strcmp(s.name, name) == 0) return Scan(s);
    Log("no signature named %s", name);
    return 0;
}

bool Resolve() {
    g_base = (uintptr_t)GetModuleHandleW(nullptr);
    auto dos = (IMAGE_DOS_HEADER*)g_base;
    auto nt = (IMAGE_NT_HEADERS*)(g_base + dos->e_lfanew);
    auto sec = IMAGE_FIRST_SECTION(nt);
    for (int i = 0; i < nt->FileHeader.NumberOfSections; ++i)
        if (memcmp(sec[i].Name, ".text", 5) == 0) g_textSize = sec[i].Misc.VirtualSize;
    Log("Game.exe base %08X, .text size %X", (unsigned)g_base, (unsigned)g_textSize);

    // SteamStub decrypts and relocates .text while the game starts; our thread
    // can race it (seen: one signature found, the others not yet, a pointer
    // read before relocation). Retry until every signature resolves to an
    // address inside the image, for up to 30 s; log misses on the last try.
    uintptr_t lo = g_base, hi = g_base + nt->OptionalHeader.SizeOfImage;
    auto inImage = [&](const void* p) { return (uintptr_t)p >= lo && (uintptr_t)p < hi; };
    bool ok = false;
    for (int i = 0; i < 60 && !ok; ++i) {
        bool quiet = i < 59;
        g_eval = (Sc3EvalProc)Scan(kEval, quiet);
        g_getString = (GetStringProc)Scan(kGetString, quiet);
        g_tables[(int)Table::User1] = (InstProc*)Read32(Scan(kTblUser1, quiet));
        g_tables[(int)Table::Graph] = (InstProc*)Read32(Scan(kTblGraph, quiet));
        g_tables[(int)Table::System] = (InstProc*)Read32(Scan(kTblSystem, quiet));
        g_flags = (uint8_t**)Read32(Scan(kGetFlag, quiet));
        g_work = (int**)Read32(Scan(kScrWork, quiet));
        ok = g_eval && g_getString && inImage(g_tables[0]) && inImage(g_tables[1]) && inImage(g_tables[2]) &&
             inImage(g_flags) && inImage(g_work);
        if (!ok) Sleep(500);
    }
    Log("eval %p getString %p tables %p %p %p flags %p work %p", g_eval, g_getString,
        g_tables[0], g_tables[1], g_tables[2], g_flags, g_work);
    if (!ok) return false;
    // The tables are filled during start-up; wait until they are populated.
    for (int i = 0; i < 300 && !(g_tables[1][0x0C] && g_tables[2][0x01]); ++i) Sleep(50);
    return g_tables[1][0x0C] != nullptr;
}

uint8_t*& ThreadPc(ScriptThread* t) { return *(uint8_t**)(t->raw + 0x14C); }
int ThreadScriptBuffer(ScriptThread* t) { return *(int*)(t->raw + 0x74); }

int Eval(ScriptThread* t) {
    int v = 0;
    g_eval(t, &v);
    return v;
}

const uint8_t* ScriptString(int scriptBuffer, int stringId) {
    return g_getString(scriptBuffer, stringId);
}

InstProc HookInstruction(Table table, int op, InstProc replacement) {
    InstProc* slot = &g_tables[(int)table][op];
    InstProc orig = *slot;
    DWORD old;
    VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &old);
    *slot = replacement;
    VirtualProtect(slot, sizeof(void*), old, &old);
    Log("hooked instruction table %d op %02X (orig %p)", (int)table, op, orig);
    return orig;
}

bool Flag(int id) {
    uint8_t* f = *g_flags;
    return f && ((f[id >> 3] >> (id & 7)) & 1);
}

int Work(int id) {
    int* w = *g_work;
    return w ? w[id] : 0;
}

int ReadInt(uintptr_t rva) { return *(volatile int*)(g_base + rva); }

// Skip mode flag: 0 -> 1 when E turns skip on (tools/re/memdiff.py toggle,
// read and written by input code around RVA 0x37AE5).
bool SkipMode() { return ReadInt(0x23034D0) != 0; }

}  // namespace Game
}  // namespace sga
