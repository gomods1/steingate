#include "movie.h"

#include <windows.h>

#include <algorithm>
#include <fstream>
#include <map>
#include <mutex>
#include <vector>

#include "buffers.h"
#include "game.h"
#include "loc.h"
#include "log.h"
#include "settings.h"
#include "speech.h"

namespace sga {
namespace Movie {

namespace {

// Bink 2 SDK: struct BINK starts with Width, Height, Frames, FrameNum,
// LastFrameNum, FrameRate, FrameRateDiv (all U32).
struct BinkHead {
    uint32_t width, height, frames, frameNum, lastFrameNum, frameRate, frameRateDiv;
};

using BinkOpenProc = void*(__stdcall*)(const char* name, uint32_t flags);
using BinkCloseProc = void(__stdcall*)(void* bink);

// Game.exe IAT slots (rva) for bink2w32 imports, from the PE import table.
const uintptr_t kIatBinkOpen = 0xB7210;
const uintptr_t kIatBinkClose = 0xB721C;

BinkOpenProc g_open = nullptr;
BinkCloseProc g_close = nullptr;
std::wstring g_dir;
std::mutex g_mu;

struct Cue {
    uint32_t ms;
    std::string text;
};
struct Playing {
    BinkHead* bink = nullptr;
    std::string name;
    std::vector<Cue> cues;
    size_t next = 0;
    uint32_t lastMs = 0;
} g_play;
std::map<std::string, std::string> g_titles;  // movie name -> spoken title/description

std::string BaseName(const char* path) {
    std::string s(path ? path : "");
    size_t slash = s.find_last_of("/\\");
    if (slash != std::string::npos) s = s.substr(slash + 1);
    size_t dot = s.find_last_of('.');
    if (dot != std::string::npos) s = s.substr(0, dot);
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)tolower(c); });
    return s;
}

std::vector<Cue> LoadTrack(const std::string& name) {
    std::vector<Cue> cues;
    std::ifstream f(g_dir + L"movies\\" + std::wstring(name.begin(), name.end()) + L".tsv", std::ios::binary);
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.size() >= 3 && (uint8_t)line[0] == 0xEF) line.erase(0, 3);
        if (line.empty() || line[0] == '#') continue;
        size_t tab = line.find('\t');
        if (tab == std::string::npos) continue;
        try {
            cues.push_back({(uint32_t)std::stoul(line.substr(0, tab)), line.substr(tab + 1)});
        } catch (...) {
        }
    }
    std::sort(cues.begin(), cues.end(), [](const Cue& a, const Cue& b) { return a.ms < b.ms; });
    return cues;
}

void Say(const std::string& s) {
    Buffers::Get(Buffers::Scene).Add(s);
    Speech::Say(s);
}

void OnOpen(const char* name, void* b) {
    {
        std::lock_guard<std::mutex> lock(g_mu);
        std::string base = BaseName(name);
        Log("BinkOpen %s -> %p", name ? name : "(null)", b);
        g_play = Playing();
        g_play.bink = (BinkHead*)b;
        g_play.name = base;
        g_play.cues = LoadTrack(base);
        if (Settings::Get("movie_subs")) {
            auto t = g_titles.find(base);
            if (t != g_titles.end() && !t->second.empty()) Say(t->second);
        }
    }
}

void* __stdcall HookOpen(const char* name, uint32_t flags) {
    void* b = g_open(name, flags);
    __try {
        OnOpen(name, b);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log("exception in BinkOpen hook");
    }
    return b;
}

void __stdcall HookClose(void* b) {
    {
        std::lock_guard<std::mutex> lock(g_mu);
        if (b == g_play.bink) {
            Log("BinkClose %s", g_play.name.c_str());
            g_play = Playing();
        }
    }
    g_close(b);
}

bool PatchIat(uintptr_t rva, void* fn, void** orig, HMODULE expect) {
    void** slot = (void**)(Game::Base() + rva);
    uintptr_t cur = (uintptr_t)*slot;
    uintptr_t lo = (uintptr_t)expect;
    auto nt = (IMAGE_NT_HEADERS*)(lo + ((IMAGE_DOS_HEADER*)lo)->e_lfanew);
    if (cur < lo || cur >= lo + nt->OptionalHeader.SizeOfImage) {
        Log("IAT slot %X does not point into bink2w32.dll", (unsigned)rva);
        return false;
    }
    *orig = *slot;
    DWORD old;
    VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &old);
    *slot = fn;
    VirtualProtect(slot, sizeof(void*), old, &old);
    return true;
}

}  // namespace

bool InstallHooks(const std::wstring& dataDir) {
    g_dir = dataDir;
    std::ifstream f(dataDir + L"movies.tsv", std::ios::binary);
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.size() >= 3 && (uint8_t)line[0] == 0xEF) line.erase(0, 3);
        if (line.empty() || line[0] == '#') continue;
        size_t tab = line.find('\t');
        if (tab != std::string::npos) g_titles[line.substr(0, tab)] = line.substr(tab + 1);
    }
    HMODULE bink = GetModuleHandleW(L"bink2w32.dll");
    if (!bink) {
        Log("bink2w32.dll not loaded");
        return false;
    }
    bool a = PatchIat(kIatBinkOpen, (void*)&HookOpen, (void**)&g_open, bink);
    bool b = PatchIat(kIatBinkClose, (void*)&HookClose, (void**)&g_close, bink);
    Log("Bink hooks: open %s, close %s, %u movie titles", a ? "ok" : "FAILED", b ? "ok" : "FAILED",
        (unsigned)g_titles.size());
    return a && b;
}

void Tick() {
    std::lock_guard<std::mutex> lock(g_mu);
    if (!g_play.bink || g_play.cues.empty()) return;
    BinkHead* h = g_play.bink;
    if (!h->frameRate) return;
    uint32_t ms = (uint32_t)((uint64_t)h->frameNum * 1000ull * h->frameRateDiv / h->frameRate);
    if (ms + 500 < g_play.lastMs) g_play.next = 0;  // looped or restarted
    g_play.lastMs = ms;
    if (!Settings::Get("movie_subs")) return;
    // Cues reached since the last frame; if many were jumped over (skip), only the last is spoken.
    size_t first = g_play.next;
    while (g_play.next < g_play.cues.size() && g_play.cues[g_play.next].ms <= ms) ++g_play.next;
    if (g_play.next > first) {
        if (g_play.next - first > 2) first = g_play.next - 1;
        for (size_t i = first; i < g_play.next; ++i) Say(g_play.cues[i].text);
    }
}

}  // namespace Movie
}  // namespace sga
