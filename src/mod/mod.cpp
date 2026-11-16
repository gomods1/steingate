#include "mod.h"

#include <fstream>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <vector>

#include "buffers.h"
#include "dinput_hook.h"
#include "game.h"
#include "input.h"
#include "loc.h"
#include "log.h"
#include "movie.h"
#include "phone.h"
#include "sc3text.h"
#include "scene.h"
#include "screens.h"
#include "settings.h"
#include "speech.h"
#include "textcap.h"

namespace sga {
namespace Mod {

namespace {

std::wstring g_modDir;   // <game>\SteinsGateAccess\ (data, logs, speech host)
std::wstring g_dataDir;  // <mod>\data\ .
std::recursive_mutex g_mu;
bool g_ready = false;
std::set<int> g_claimed;

// ---- speech helpers ----------------------------------------------------

void Say(const std::string& s, bool interrupt = false) { Speech::Say(s, interrupt); }

void Event(const std::string& s) {
    Buffers::Get(Buffers::Events).Add(s);
    Say(s);
}

// ---- dialogue ------------------------------------------------------------

// A line is spoken once it has stayed on screen for kHoldMs: while the game
// skips, lines replace each other faster than that and only the line where
// skipping stops is spoken (the skip state itself is native, not a script flag).
const DWORD kHoldMs = 150;
struct Pending {
    bool active = false;
    Sc3Line line;
    DWORD time = 0;
} g_pending;
const uint8_t* g_lastMesPc = nullptr;
DWORD g_lastMesTime = 0;
std::string g_lastLine;

std::string StripQuotes(std::string t) {
    static const std::string open = "\xE2\x80\x9C", close = "\xE2\x80\x9D";  // “ ”
    if (t.size() >= 6 && t.compare(0, 3, open) == 0 && t.compare(t.size() - 3, 3, close) == 0)
        t = t.substr(3, t.size() - 6);
    return t;
}

std::string FormatLine(const Sc3Line& l) {
    std::string text = l.text;
    if (!l.name.empty() && Settings::Get("strip_quotes")) text = StripQuotes(text);
    if (l.name.empty() || !Settings::Get("speak_names")) return text;
    return Loc::Format("line_named", {{"name", l.name}, {"text", text}});
}

// The game is fast-forwarding: E skip mode, or Ctrl held long enough to reach
// the game (force skip, see input.cpp).
bool Skipping() { return Game::SkipMode() || Input::ForceSkipHeld(); }

// Scene changes seen while skipping are spoken once skipping stops.
bool g_sceneDirty = false;

void FlushPending(DWORD now, bool force) {
    if (!g_pending.active) return;
    if (!force && (now - g_pending.time < kHoldMs || Skipping())) return;
    g_pending.active = false;
    std::string s = FormatLine(g_pending.line);
    g_lastLine = s;
    Buffers::Get(Buffers::Dialogue).Add(s);
    Say(s, Settings::Get("interrupt_lines") != 0);
}

void OnMessage(ScriptThread* t) {
    uint8_t* pc = Game::ThreadPc(t);
    DWORD now = GetTickCount();
    if (pc == g_lastMesPc && now - g_lastMesTime < 2000) return;  // re-executed
    g_lastMesPc = pc;
    g_lastMesTime = now;
    uint8_t sub = pc[2];
    if (sub & 0x80) return;  // MSB variants are not used by this game
    const uint8_t* p = pc + 3;
    int exprs = (sub & 3) == 3 ? 3 : 1;
    for (int i = 0; i < exprs; ++i) p = Sc3::SkipExpr(p);
    int id = *(const int16_t*)p;
    const uint8_t* s = Game::ScriptString(Game::ThreadScriptBuffer(t), id);
    if (!s) return;
    Sc3Line line = Sc3::Decode(s);
    if (line.text.empty()) return;
    if (!line.name.empty()) Scene::OnSpeaker(line.name);
    // A newer line replaces one still waiting (skip mode).
    g_pending.active = true;
    g_pending.line = line;
    g_pending.time = now;
}

// ---- operand evaluation --------------------------------------------------

// Evaluates n expression operands starting at pc+skip without disturbing the
// thread: pc is restored before the original handler runs.
void EvalOperands(ScriptThread* t, int skip, int n, int* out) {
    uint8_t*& pc = Game::ThreadPc(t);
    uint8_t* save = pc;
    pc += skip;
    for (int i = 0; i < n; ++i) out[i] = Game::Eval(t);
    pc = save;
}

// ---- scene ---------------------------------------------------------------

void OnBgLoad(ScriptThread* t) {
    int v[2];
    EvalOperands(t, 2, 2, v);
    Scene::OnBgLoad(v[0], v[1]);
}

void OnBgRelease(ScriptThread* t) {
    int v[1];
    EvalOperands(t, 2, 1, v);
    Scene::OnBgRelease(v[0]);
}

void OnCharaLoad(ScriptThread* t) {
    int v[2];
    EvalOperands(t, 3, 2, v);
    Scene::OnCharaLoad(v[0], v[1]);
}

void OnCharaRelease(ScriptThread* t) {
    int v[1];
    EvalOperands(t, 2, 1, v);
    Scene::OnCharaRelease(v[0]);
}

// Pictures and characters that became visible this frame.
void PollScene(bool skipping) {
    std::vector<std::string> lines = Scene::Poll(Settings::Get("scene_desc") != 0, Settings::Get("chara_desc") != 0);
    if (lines.empty()) return;
    for (auto& l : lines) Buffers::Get(Buffers::Scene).Add(l);
    if (skipping) {
        g_sceneDirty = true;
        return;
    }
    FlushPending(GetTickCount(), true);
    for (auto& l : lines) Say(l);
}
// ---- date ------------------------------------------------------------------

// The date shown at the top left: ScrWork 1430 = month * 100 + day (scripts
// set it, e.g. SG01_01 "$W(1430) = 728"; probe: 728 while 7/28 (WED) shown).
// The story takes place in 2010.
int g_lastDate = -1;

std::string DateText(int mmdd) {
    static const char* months[] = {"jan", "feb", "mar", "apr", "may", "jun",
                                   "jul", "aug", "sep", "oct", "nov", "dec"};
    static const char* days[] = {"sun", "mon", "tue", "wed", "thu", "fri", "sat"};
    int m = mmdd / 100, d = mmdd % 100;
    if (m < 1 || m > 12 || d < 1 || d > 31) return "";
    // Day of week (Sakamoto's method) for 2010.
    static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    int y = 2010 - (m < 3);
    int wd = (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
    return Loc::Format("date", {{"weekday", Loc::T((std::string("day_") + days[wd]).c_str())},
                                {"month", Loc::T((std::string("month_") + months[m - 1]).c_str())},
                                {"day", std::to_string(d)}});
}

// Text window hidden by the player (Backspace or Shift in dialogue): the int
// at RVA 0x2304E0C is 256 while the window shows and 0 while hidden (memdiff
// toggling it with Backspace). Only changes right after those keys are
// announced, so script-driven window fades stay quiet.
unsigned g_hideKeyFrames = 0;
int g_windowAlpha = -1;

void CheckWindowHidden() {
    int a = Game::ReadInt(0x2304E0C);
    if (g_hideKeyFrames > 0) {
        --g_hideKeyFrames;
        if (a != g_windowAlpha && (a == 0 || a == 256) && !Screens::InMenu())
            Say(Loc::T(a == 0 ? "window_hidden" : "window_shown"));
    }
    g_windowAlpha = a;
}

void CheckDate() {
    int v = Game::Work(1430);
    if (v == g_lastDate) return;
    bool first = g_lastDate < 0;
    g_lastDate = v;
    std::string s = DateText(v);
    if (s.empty() || first || Screens::InMenu()) return;
    FlushPending(GetTickCount(), true);
    Event(s);
}


// ---- tips ------------------------------------------------------------------

// data/tips.tsv: SetDic operand -> category, name, description (tools/build_tips.py).
struct Tip {
    std::string name, text;
};
std::map<int, Tip> g_tips;

void LoadTips() {
    std::ifstream f(g_dataDir + L"tips.tsv", std::ios::binary);
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        std::vector<std::string> c;
        size_t a = 0, b;
        while ((b = line.find('\t', a)) != std::string::npos) {
            c.push_back(line.substr(a, b - a));
            a = b + 1;
        }
        c.push_back(line.substr(a));
        if (c.size() >= 4) g_tips[std::stoi(c[0])] = {c[2], c[3]};
    }
    Log("tips: %u", (unsigned)g_tips.size());
}

// SetDic (10 27, one expression: tip id) unlocks a tip; the game shows
// "NEW TIP" when its Tips information option (RVA 0x230328C) is on.
void OnSetDic(ScriptThread* t) {
    int v[1];
    EvalOperands(t, 2, 1, v);
    auto it = g_tips.find(v[0]);
    if (it == g_tips.end()) return;
    // The instruction can re-run every frame while the game waits; one
    // announcement per tip per 10 seconds.
    static std::map<int, DWORD> s_said;
    DWORD now = GetTickCount();
    auto said = s_said.find(v[0]);
    if (said != s_said.end() && now - said->second < 10000) return;
    s_said[v[0]] = now;
    std::string s = Loc::Format("tip_new", {{"name", it->second.name}});
    Buffers::Get(Buffers::Events).Add(s + " " + it->second.text);
    if (!Settings::Get("tips_notify") || Game::ReadInt(0x230328C) == 0 || Skipping()) return;
    FlushPending(GetTickCount(), true);
    Say(s);
}

// ---- instruction hooks -----------------------------------------------------

InstProc g_origMes, g_origBgLoad, g_origBgRelease, g_origChaLoad, g_origChaRelease, g_origSetDic;

template <void (*Handler)(ScriptThread*)>
void Locked(ScriptThread* t) {
    std::lock_guard<std::recursive_mutex> lock(g_mu);
    Handler(t);
}

// Fault isolation: an error in a handler must never break the game's own
// instruction, which always runs afterwards.
template <void (*Handler)(ScriptThread*)>
void Guarded(ScriptThread* t, const char* what) {
    __try {
        Locked<Handler>(t);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log("exception in %s handler", what);
    }
}

void __cdecl HookMes(ScriptThread* t) {
    Guarded<OnMessage>(t, "message");
    g_origMes(t);
}
void __cdecl HookBgLoad(ScriptThread* t) {
    Guarded<OnBgLoad>(t, "BGload");
    g_origBgLoad(t);
}
void __cdecl HookBgRelease(ScriptThread* t) {
    Guarded<OnBgRelease>(t, "BGrelease");
    g_origBgRelease(t);
}
void __cdecl HookChaLoad(ScriptThread* t) {
    Guarded<OnCharaLoad>(t, "CHAload");
    g_origChaLoad(t);
}
void __cdecl HookChaRelease(ScriptThread* t) {
    Guarded<OnCharaRelease>(t, "CHArelease");
    g_origChaRelease(t);
}

void __cdecl HookSetDic(ScriptThread* t) {
    Guarded<OnSetDic>(t, "SetDic");
    g_origSetDic(t);
}

void InstallHooks() {
    g_origSetDic = Game::HookInstruction(Game::Table::User1, 0x27, &HookSetDic);  // SetDic
    using Game::Table;
    // Opcodes from MgsScriptTools spec-bank (base_01/base_10, flag set steins_gate_hd).
    g_origMes = Game::HookInstruction(Table::Graph, 0x0C, &HookMes);           // Mes*SetMesScx
    g_origBgLoad = Game::HookInstruction(Table::User1, 0x01, &HookBgLoad);     // BGload
    g_origChaLoad = Game::HookInstruction(Table::User1, 0x05, &HookChaLoad);   // CHAloadAtlas
    g_origBgRelease = Game::HookInstruction(Table::User1, 0x10, &HookBgRelease);
    g_origChaRelease = Game::HookInstruction(Table::User1, 0x11, &HookChaRelease);
}

// ---- mod menu (help and settings) ------------------------------------------

struct MenuItem {
    std::string label;
    int setting = -1;  // index into Settings::All(), or -1 for a help line
};
bool g_menuOpen = false;
std::vector<MenuItem> g_menu;
int g_menuPos = 0;

std::string SettingText(const Setting& s) {
    std::string label = Loc::T((std::string("set_") + s.key).c_str());
    if (s.max == 1) return Loc::Format("menu_toggle", {{"label", label}, {"state", Loc::T(s.value ? "on" : "off")}});
    return Loc::Format("menu_value", {{"label", label}, {"value", std::to_string(s.value)}});
}

std::string MenuItemText(int i) {
    const MenuItem& m = g_menu[i];
    std::string t = m.setting >= 0 ? SettingText(Settings::All()[m.setting]) : m.label;
    return Loc::Format("menu_pos", {{"item", t}, {"pos", std::to_string(i + 1)}, {"count", std::to_string(g_menu.size())}});
}

void OpenMenu() {
    g_menu.clear();
    for (const char* k : {"help_dialogue", "help_buffers", "help_repeat", "help_scene", "help_menu",
                          "help_game_keys", "help_phone"})
        g_menu.push_back({Loc::T(k), -1});
    for (size_t i = 0; i < Settings::All().size(); ++i) g_menu.push_back({"", (int)i});
    g_menuOpen = true;
    g_menuPos = 0;
    Say(Loc::T("menu_open"));
    Say(MenuItemText(0));
}

bool MenuKey(int dik) {
    int n = (int)g_menu.size();
    switch (dik) {
        case KEY_UP: g_menuPos = (g_menuPos + n - 1) % n; Say(MenuItemText(g_menuPos)); break;
        case KEY_DOWN: g_menuPos = (g_menuPos + 1) % n; Say(MenuItemText(g_menuPos)); break;
        case KEY_HOME: g_menuPos = 0; Say(MenuItemText(g_menuPos)); break;
        case KEY_END: g_menuPos = n - 1; Say(MenuItemText(g_menuPos)); break;
        case KEY_RET: case KEY_SPACE: case KEY_LEFT: case KEY_RIGHT: {
            MenuItem& m = g_menu[g_menuPos];
            if (m.setting < 0) {
                Say(m.label);
                break;
            }
            Setting& s = Settings::All()[m.setting];
            int d = dik == KEY_LEFT ? -1 : 1;
            s.value += d;
            if (s.value > s.max) s.value = s.min;
            if (s.value < s.min) s.value = s.max;
            Settings::Save();
            Say(SettingText(s));
            break;
        }
        case KEY_ESC: case KEY_BACK: case KEY_H:
            g_menuOpen = false;
            Say(Loc::T("menu_closed"));
            break;
        default: break;
    }
    return true;
}

}  // namespace

// ---- public ------------------------------------------------------------------

void EarlyInit() {
    static std::once_flag once;
    std::call_once(once, [] {
        wchar_t buf[MAX_PATH];
        GetModuleFileNameW(nullptr, buf, MAX_PATH);
        std::wstring exe(buf);
        g_modDir = exe.substr(0, exe.find_last_of(L"\\/") + 1) + L"SteinsGateAccess\\";
        g_dataDir = g_modDir + L"data\\";
        LogInit(g_modDir);
        Log("Steins;Gate Access starting");
    });
}

void Start(HMODULE) {
    EarlyInit();
    Settings::Load(g_modDir + L"settings.ini");
    Loc::Load(g_dataDir, "en");
    Speech::Init(g_modDir);
    Sc3::LoadCharset(g_dataDir + L"charset.utf8");
    Scene::Load(g_dataDir, g_modDir);
    LoadTips();
    if (!Game::Resolve()) {
        Log("unsupported game build: hooks not installed");
        Say(Loc::T("unsupported_build"));
        return;
    }
    {
        std::lock_guard<std::recursive_mutex> lock(g_mu);
        InstallHooks();
        Screens::InstallHooks();
        g_ready = true;
    }
    if (!InstallKeyboardHook()) Log("no keyboard hook: mod keys unavailable");
    Movie::InstallHooks(g_dataDir);
    if (!TextCap::Install()) Log("text capture incomplete");
    TextCap::SetDebugLog(Settings::Get("debug_text") != 0);
    Say(Loc::T("mod_loaded"));
}

void Tick() {
    if (!g_ready) return;
    std::lock_guard<std::recursive_mutex> lock(g_mu);
    // Frame-rate and mod cost check (logged every 10 s).
    static DWORD s_t0 = GetTickCount();
    static unsigned s_frames = 0;
    static LARGE_INTEGER s_cost{}, s_freq{};
    LARGE_INTEGER a, b;
    if (!s_freq.QuadPart) QueryPerformanceFrequency(&s_freq);
    QueryPerformanceCounter(&a);
    ++s_frames;
    if (GetTickCount() - s_t0 >= 10000) {
        Log("perf: %u frames in %lu ms, mod %.3f ms/frame", s_frames, GetTickCount() - s_t0,
            s_cost.QuadPart * 1000.0 / s_freq.QuadPart / s_frames);
        s_t0 = GetTickCount();
        s_frames = 0;
        s_cost.QuadPart = 0;
    }
    TextCap::EndFrame();
    bool skipping = Skipping();
    if (!skipping && g_sceneDirty) {
        // Skipping just stopped: describe where we are now.
        g_sceneDirty = false;
        if (Settings::Get("scene_desc")) {
            std::string all;
            for (auto& l : Scene::Describe()) {
                all += l;
                char c = l.empty() ? ' ' : l.back();
                all += (c == '.' || c == '!' || c == '?') ? " " : ". ";
            }
            // Seen at a Phone Trigger wait (session 1): the same summary spoken
            // twice a few seconds apart when skip toggled; one per 10 s.
            static std::string s_lastSummary;
            static DWORD s_lastSummaryAt = 0;
            DWORD now = GetTickCount();
            if (all != s_lastSummary || now - s_lastSummaryAt >= 10000) {
                s_lastSummary = all;
                s_lastSummaryAt = now;
                Say(all);
            }
        }
    }
    PollScene(skipping);
    FlushPending(GetTickCount(), false);
    CheckDate();
    CheckWindowHidden();
    Screens::Tick();
    Phone::Tick(skipping);
    Movie::Tick();
    QueryPerformanceCounter(&b);
    s_cost.QuadPart += b.QuadPart - a.QuadPart;
}

bool CapturesAllKeys() { return g_menuOpen; }

bool OnKey(int dik, bool ctrl, bool shift) {
    (void)shift;
    if (!g_ready) return false;
    std::lock_guard<std::recursive_mutex> lock(g_mu);
    if (dik == KEY_BACK || dik == KEY_LSHIFT || dik == KEY_RSHIFT) g_hideKeyFrames = 40;
    if (g_menuOpen) return MenuKey(dik);
    if (ctrl) {
        switch (dik) {
            case KEY_UP: Say(Buffers::Prev());  return true;
            case KEY_DOWN: Say(Buffers::Next());  return true;
            case KEY_LEFT: Say(Buffers::SwitchLeft());  return true;
            case KEY_RIGHT: Say(Buffers::SwitchRight());  return true;
            default: return false;
        }
    }
    switch (dik) {
        case KEY_R:
            Say(g_lastLine.empty() ? Loc::T("nothing_to_repeat") : g_lastLine);
            return true;
        case KEY_D: {
            if (Screens::InMenu()) {
                // In a menu: the screen name, focused item and all text drawn on it.
                std::vector<std::string> lines = Screens::ScreenText();
                std::string focus = Screens::CurrentFocus();
                if (!focus.empty()) lines.insert(lines.begin(), focus);
                lines.insert(lines.begin(), Screens::CurrentName());
                Buffers::Get(Buffers::Details).Set(lines);
                Buffers::Select(Buffers::Details);
                std::string all;
                for (auto& l : lines) all += l + ". ";
                Say(all);
                return true;
            }
            std::vector<std::string> lines = Scene::Describe();
            std::vector<std::string> board = Phone::BoardLines();
            if (!board.empty()) {
                board.insert(board.begin(), Loc::T("screen_shows"));
                lines.insert(lines.end(), board.begin(), board.end());
            }
            if (Phone::Active()) {
                std::vector<std::string> ph = Phone::Lines();
                ph.insert(ph.begin(), Loc::T("phone_shows"));
                lines.insert(lines.end(), ph.begin(), ph.end());
            }
            std::string date = DateText(Game::Work(1430));
            if (!date.empty()) lines.insert(lines.begin(), date);
            Buffers::Get(Buffers::Details).Set(lines);
            Buffers::Select(Buffers::Details);
            std::string all;
            for (auto& l : lines) {
                all += l;
                char c = l.empty() ? ' ' : l.back();
                all += (c == '.' || c == '!' || c == '?' || c == ':') ? " " : ". ";
            }
            Say(all);
            return true;
        }
        case KEY_H:
            OpenMenu();
            return true;
        default:
            return false;
    }
}

void ClaimKey(int dik) { g_claimed.insert(dik); }
bool IsKeyClaimed(int dik) { return g_claimed.count(dik) != 0; }
void ReleaseKey(int dik) { g_claimed.erase(dik); }

}  // namespace Mod
}  // namespace sga
