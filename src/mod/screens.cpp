#include "screens.h"

#include <windows.h>

#include <algorithm>
#include <cstdlib>
#include <deque>
#include <set>
#include <vector>

#include "buffers.h"
#include "game.h"
#include "loc.h"
#include "log.h"
#include "speech.h"
#include "sc3text.h"
#include "textcap.h"

namespace sga {
namespace Screens {

namespace {

// One slot per hooked menu instruction (opcode group, index). Opcodes from
// MgsScriptTools spec-bank base_00/base_01/base_10 (flag set steins_gate_hd).
struct Slot {
    Game::Table table;
    int op;
    int mainSub;          // sub-opcode of the per-frame *Main instruction, -1 = any
    const char* nameKey;  // string key of the spoken screen name
    int priority;         // higher wins when several run in one frame
};
const Slot kSlots[] = {
    {Game::Table::User1, 0x34, 0x01, "scr_title", 1},         // TitleMenuMain
    {Game::Table::System, 0x44, 0x01, "scr_system", 2},       // SystemMenuMain
    {Game::Table::User1, 0x23, 0x01, "scr_save", 3},          // SaveMenuMain (mode from *MenuInit)
    {Game::Table::User1, 0x13, 0x01, "scr_config", 3},        // OptionMain
    {Game::Table::User1, 0x1A, 0x01, "scr_help", 3},          // HelpMain
    {Game::Table::User1, 0x1B, 0x01, "scr_achievements", 3},  // AchievementMain
    {Game::Table::User1, 0x1D, 0x01, "scr_music", 3},         // SoundMenuMain
    {Game::Table::User1, 0x1F, 0x01, "scr_album", 3},         // AlbumMain
    {Game::Table::User1, 0x20, 0x01, "scr_movies", 3},        // MovieModeMain
    {Game::Table::User1, 0x21, -1, "scr_clearlist", 3},       // ClistInit / ClistInit2
    {Game::Table::User1, 0x33, 0x02, "scr_tips", 3},          // EncycMenuMain
    {Game::Table::Graph, 0x10, 0x01, "scr_backlog", 3},       // MesRevMain
    {Game::Table::System, 0x51, -1, "scr_message", 5},        // GlobalSystemMessage_0x
};
// A screen counts as left after this many frames without its instruction.
const unsigned kLeaveFrames = 20;
const int kSlotCount = sizeof(kSlots) / sizeof(kSlots[0]);

InstProc g_orig[kSlotCount];
unsigned g_seen[kSlotCount];    // frame counter when last executed
uint8_t g_sub[kSlotCount];      // sub-opcode byte of the last execution
uint8_t g_sub2[kSlotCount];     // the byte after it (SaveMenuInit mode)
unsigned g_frame = 1;

int g_screen = -1;              // active slot, -1 = dialogue / none
std::string g_focus;            // last spoken focus text
int g_saveMode = -1;            // 0 quick load, 1 save, 2 load (from SaveMenuInit)
std::string g_lastName;         // last announced screen name
bool g_readScreen = false;      // speak the screen text once it is drawn
// Notices: white text drawn outside menus (tutorial boxes, system messages),
// spoken once it has been stable for kNoticeFrames.
std::string g_notice, g_noticePrev;
unsigned g_noticeStable = 0;
const unsigned kNoticeFrames = 10;
std::string g_option;  // selected dialog option last spoken

template <int N>
void __cdecl Hook(ScriptThread* t) {
    __try {
        const uint8_t* pc = Game::ThreadPc(t);
        if (kSlots[N].mainSub < 0 || pc[2] == kSlots[N].mainSub) g_seen[N] = g_frame;
        if (N == 2 && pc[2] == 0) g_saveMode = pc[3];  // QLoad/Save/LoadMenuInit
        g_sub[N] = pc[2];
        g_sub2[N] = pc[3];
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
    g_orig[N](t);
}

template <int N>
void InstallFrom() {
    if constexpr (N < kSlotCount) {
        g_orig[N] = Game::HookInstruction(kSlots[N].table, kSlots[N].op, &Hook<N>);
        InstallFrom<N + 1>();
    }
}

// ---- readers: focused item of each screen ----------------------------------

// Title menu (native): cursor int at RVA 0x1244464 (0 Start .. 4 Help);
// a submenu is open when the int at RVA 0x12615E4 is non-zero; Load submenu
// cursor RVA 0x124446C, Extra submenu cursor RVA 0x1244470. Extra has more
// items when flag 860 is set (draw code RVA 0x41030 -> 0x40C30).
std::string TitleFocus() {
    static const char* main[] = {"title_start", "title_load", "title_extra", "title_config", "title_help"};
    int cur = Game::ReadInt(0x1244464);
    if (cur < 0 || cur > 4) return "";
    std::string item = Loc::T(main[cur]);
    if (Game::ReadInt(0x12615E4) != 0) {
        if (cur == 1) {
            static const char* sub[] = {"title_load_quick", "title_load_load"};  // seen on screen
            int s = Game::ReadInt(0x124446C);
            if (s >= 0 && s < 2) return Loc::Format("submenu_item", {{"menu", item}, {"item", Loc::T(sub[s])}});
        } else if (cur == 2) {
            int s = Game::ReadInt(0x1244470);
            if (!Game::Flag(860)) {
                static const char* sub[] = {"title_extra_clearlist", "title_extra_tips"};
                if (s >= 0 && s < 2) return Loc::Format("submenu_item", {{"menu", item}, {"item", Loc::T(sub[s])}});
            }
            return Loc::Format("submenu_item", {{"menu", item}, {"item", Loc::Format("item_n", {{"n", std::to_string(s + 1)}})}});
        }
    }
    return item;
}

// Strings drawn this frame inside a rectangle, in draw order, without the
// black shadow copies the game draws around each string.
std::vector<std::string> TextIn(int x0, int y0, int x1, int y1) {
    std::vector<std::string> out;
    for (const DrawnText& d : TextCap::LastFrame()) {
        if ((d.color & 0xFFFFFF) == 0) continue;
        if (d.x < x0 || d.x > x1 || d.y < y0 || d.y > y1) continue;
        bool dup = false;
        for (auto& o : out) dup |= o == d.text;
        if (!dup) out.push_back(d.text);
    }
    return out;
}

std::string Key(const DrawnText& d) { return std::to_string(d.x) + "," + std::to_string(d.y) + "," + d.text; }

// Strings drawn while the last menu was in front. A dialog opened over a menu
// stops the menu's *Main instruction; its own text is what is not in here.
std::set<std::string> g_baseline;
// Snapshots of the last menu frames; the oldest becomes the baseline, since
// the frame where a menu hands over to its dialog already draws the dialog.
std::deque<std::set<std::string>> g_menuFrames;

// All white text on screen as reading-order lines: strings within 4 px of the
// same y form a row, pieces joined by gap (numbers are drawn glyph by glyph).
// With exclude, strings in that set are left out.
std::vector<std::string> ScreenLines(const std::set<std::string>* exclude = nullptr) {
    std::vector<const DrawnText*> v;
    for (const DrawnText& d : TextCap::LastFrame())
        if (d.func == 0 && (d.color & 0xFFFFFF) == 0xFFFFFF && !(exclude && exclude->count(Key(d)))) v.push_back(&d);
    std::sort(v.begin(), v.end(), [](const DrawnText* a, const DrawnText* b) {
        if (std::abs(a->y - b->y) > 4) return a->y < b->y;
        return a->x < b->x;
    });
    std::vector<std::string> lines;
    std::string cur;
    const DrawnText* prev = nullptr;
    for (const DrawnText* d : v) {
        if (prev && std::abs(d->y - prev->y) <= 4 && d->x == prev->x && d->text == prev->text) continue;
        if (!prev || std::abs(d->y - prev->y) > 4) {
            if (!cur.empty()) lines.push_back(cur);
            cur = d->text;
        } else {
            int gap = d->x - prev->x;
            cur += gap < 45 ? "" : gap < 90 ? " " : ", ";
            cur += d->text;
        }
        prev = d;
    }
    if (!cur.empty()) lines.push_back(cur);
    return lines;
}

// Save/Load (native): 8 slots per page in two columns of four. Cursor in the
// page at RVA 0x1261734, page at RVA 0x126173C (memdiff; Right on the right
// column turns the page). Slot text: drawSingleTextLine at x 255 / 834,
// rows y 101, 228, 355, 482 (debug text log of the Load screen).
std::string SaveFocus() {
    int cur = Game::ReadInt(0x1261734), page = Game::ReadInt(0x126173C);
    if (cur < 0 || cur > 7 || page < 0 || page > 5) return "";
    int x = cur < 4 ? 255 : 834, y = 101 + (cur % 4) * 127;
    std::string info;
    for (auto& t : TextIn(x - 10, y - 10, x + 560, y + 100)) {
        // Slot number label and time labels: their digits are sprites.
        if (t.rfind("No .", 0) == 0 || t == "Play Time:" || t == "Save Time:") continue;
        if (!info.empty()) info += ", ";
        info += t;
    }
    if (info.empty()) return "";  // not drawn yet this frame
    return Loc::Format("save_slot", {{"n", std::to_string(page * 8 + cur + 1)}, {"info", info}});
}

// Config (native, three pages). Page RVA 0x1243878, cursor of page p at RVA
// 0x1243880 + 4p. Field of each row found by changing it and diffing the
// config block written by the defaults routine at RVA 0x708C0
// (tools/re/configmap.py, tools/re/sliderrange.py).
enum ValueKind { kBool, kPercent, kSkipMode, kAutoSave, kWindowMode, kResolution, kCharaVoice };
struct ConfigRow {
    const char* label;
    ValueKind kind;
    uintptr_t rva;
    int lo, hi;  // percent range
};
const ConfigRow kPage0[] = {
    {"cfg_phone_skip", kBool, 0x2303294, 0, 0},
    {"cfg_tips_info", kBool, 0x230328C, 0, 0},
    {"cfg_auto_qsave", kAutoSave, 0x230326C, 0, 0},
    {"cfg_fullscreen", kWindowMode, 0, 0, 0},
    {"cfg_resolution", kResolution, 0, 0, 0},
    {"cfg_msg_speed", kPercent, 0x230325C, 256, 4096},
    {"cfg_auto_speed", kPercent, 0x2303260, 256, 2048},
    {"cfg_skip_mode", kSkipMode, 0x2303268, 0, 0},
};
const ConfigRow kPage1[] = {
    {"cfg_voice_vol", kPercent, 0x2303248, 0, 128},
    {"cfg_music_vol", kPercent, 0x230324C, 0, 128},
    {"cfg_se_vol", kPercent, 0x2303250, 0, 128},
    {"cfg_movie_vol", kPercent, 0x2303258, 0, 128},
    {"cfg_sync_voice", kBool, 0x2303264, 0, 0},
    {"cfg_skip_voice", kBool, 0x2303288, 0, 0},
};
// Page 3 rows (names as shown) and their index in the per-voice arrays:
// volume at RVA 0x23033B8 + 4i (0..128), on/off at RVA 0x2303330 + 4i.
const char* kVoiceRows[] = {"Kurisu M.", "Mayuri S.", "Suzuha A.", "Luka U.", "Faris NyanNyan", "Moeka K.",
                            "Rintaro O.", "Itaru H.", "Yugo T.", "Nae T.", "Doctor N.", "Others"};
const int kVoiceIndex[] = {0, 1, 4, 3, 5, 2, 6, 7, 8, 9, 10, 11};

std::string Percent(int v, int lo, int hi) {
    int p = (v - lo) * 100 / (hi - lo);
    return std::to_string(p < 0 ? 0 : p > 100 ? 100 : p) + "%";
}

HWND GameWindow() {
    struct F {
        static BOOL CALLBACK Cb(HWND h, LPARAM l) {
            DWORD pid;
            GetWindowThreadProcessId(h, &pid);
            if (pid == GetCurrentProcessId() && IsWindowVisible(h)) {
                *(HWND*)l = h;
                return FALSE;
            }
            return TRUE;
        }
    };
    HWND h = nullptr;
    EnumWindows(F::Cb, (LPARAM)&h);
    return h;
}

std::string ConfigValue(const ConfigRow& r) {
    switch (r.kind) {
        case kBool: return Loc::T(Game::ReadInt(r.rva) ? "on" : "off");
        case kPercent: return Percent(Game::ReadInt(r.rva), r.lo, r.hi);
        case kSkipMode: return Loc::T(Game::ReadInt(r.rva) ? "cfg_skip_all" : "cfg_skip_read");
        case kAutoSave: return Loc::T(Game::ReadInt(r.rva) ? "cfg_autosave_scene" : "off");
        case kWindowMode: {
            HWND h = GameWindow();
            bool popup = h && (GetWindowLongW(h, GWL_STYLE) & WS_CAPTION) != WS_CAPTION;
            return Loc::T(popup ? "cfg_full_screen" : "cfg_window");
        }
        case kResolution: {
            RECT rc{};
            HWND h = GameWindow();
            if (h) GetClientRect(h, &rc);
            return std::to_string(rc.right) + " by " + std::to_string(rc.bottom);
        }
        default: return "";
    }
}

std::string ConfigFocus() {
    int page = Game::ReadInt(0x1243878);
    if (page < 0 || page > 2) return "";
    int cur = Game::ReadInt(0x1243880 + 4 * page);
    std::string label, value;
    if (page == 0 && cur >= 0 && cur < 8) {
        label = Loc::T(kPage0[cur].label);
        value = ConfigValue(kPage0[cur]);
    } else if (page == 1 && cur >= 0 && cur < 6) {
        label = Loc::T(kPage1[cur].label);
        value = ConfigValue(kPage1[cur]);
    } else if (page == 2 && cur >= 0 && cur < 12) {
        int i = kVoiceIndex[cur];
        label = Loc::Format("cfg_chara_voice", {{"name", kVoiceRows[cur]}});
        value = Percent(Game::ReadInt(0x23033B8 + 4 * i), 0, 128) + ", " +
                Loc::T(Game::ReadInt(0x2303330 + 4 * i) ? "on" : "off");
    } else {
        return "";
    }
    return Loc::Format("cfg_row", {{"page", std::to_string(page + 1)}, {"label", label}, {"value", value}});
}

// Tips list (native): row cursor at RVA 0x12E66C (memdiff). Rows are the
// strings drawn in the left column (x < 360), one per 20 px; the open tip's
// title and text are drawn in the right panel (x >= 384).
// HYPOTHESIS: the list does not scroll while all rows fit; a scroll offset
// will be needed once categories with many tips are expanded.
std::string g_tipText;
std::string TipsFocus() {
    std::vector<const DrawnText*> rows, panel;
    for (const DrawnText& d : TextCap::LastFrame()) {
        if ((d.color & 0xFFFFFF) == 0) continue;  // shadow copies
        (d.x < 360 ? rows : panel).push_back(&d);
    }
    std::sort(rows.begin(), rows.end(), [](const DrawnText* a, const DrawnText* b) { return a->y < b->y; });
    int cur = Game::ReadInt(0x12E66C);
    std::string item;
    if (cur >= 0 && cur < (int)rows.size()) item = rows[cur]->text;
    // Tip text: speak it when it changes (it is not part of the focus string).
    std::sort(panel.begin(), panel.end(), [](const DrawnText* a, const DrawnText* b) { return a->y < b->y; });
    std::string text;
    for (auto* p : panel) text += (text.empty() ? "" : " ") + p->text;
    if (text != g_tipText) {
        g_tipText = text;
        if (!text.empty()) {
            Buffers::Get(Buffers::Details).Set({text});
            Buffers::Select(Buffers::Details);
            Speech::Say(text);
        }
    }
    return item;
}

// Help (images): page at RVA 0x1243A00 (memdiff with Q/E). Page 1 shows the
// keyboard and mouse controls, page 2 the controller (manual.mpk PAGE1/PAGE2).
std::string HelpFocus() {
    int page = Game::ReadInt(0x1243A00);
    if (page < 0 || page > 1) return "";
    return Loc::T(page == 0 ? "help_page_keyboard" : "help_page_pad");
}

// In-game System menu (key 1): cursor ScrWork 3338 (impacto common profile
// names it SW_SYSMENUCNO; probe diff while moving). Item labels are images,
// read from screenshots at each position (tools/playtest/sysmenu_labels.py).
std::string SystemFocus() {
    static const char* items[] = {"sys_backlog", "sys_qsave", "sys_qload", "sys_save", "sys_load", "sys_tips",
                                  "sys_config", "sys_fullscreen", "sys_help", "sys_title", "sys_close", "sys_exit"};
    int cur = Game::Work(3338);
    if (cur < 0 || cur > 11) return "";
    return Loc::Format("menu_pos", {{"item", Loc::T(items[cur])}, {"pos", std::to_string(cur + 1)}, {"count", "12"}});
}

// Backlog (native, F2/Insert): the game keeps laid-out glyphs, decoded the
// way its draw code (RVA 0x46C90) reads them: entry count at RVA 0x1343D04,
// entry -> line at RVA 0x14FB470, line -> first glyph at RVA 0x13D64D0 and
// glyph count at RVA 0x1407210, 16-bit glyph ids at RVA 0x1343D08 (0x8000 |
// 1 = name start, 2 = name end, 3 = end). Cursor (entry) at RVA 0x14FB450.
// Verified with tools/re/backlog.py: "<1>Itaru<2>"Yeah?"<3>".
std::string BacklogEntry(int e) {
    int line = Game::ReadInt(0x14FB470 + 4 * e);
    int start = Game::ReadInt(0x13D64D0 + 4 * line);
    int n = Game::ReadInt(0x1407210 + 4 * line);
    if (line < 0 || start < 0 || n <= 0 || n > 4000) return "";
    const uint16_t* g = (const uint16_t*)(Game::Base() + 0x1343D08) + start;
    std::vector<uint8_t> sc3;  // rebuild an SC3 string and reuse the decoder
    for (int i = 0; i < n; ++i) {
        uint16_t v = g[i];
        if (v & 0x8000) {
            int code = v & 0x7FFF;
            if (code == 3) break;
            if (code == 1 || code == 2) sc3.push_back((uint8_t)code);
            continue;
        }
        sc3.push_back((uint8_t)(0x80 | (v >> 8)));
        sc3.push_back((uint8_t)(v & 0xFF));
    }
    sc3.push_back(0xFF);
    Sc3Line l = Sc3::Decode(sc3.data(), sc3.size());
    return l.name.empty() ? l.text : l.name + ": " + l.text;
}

std::string BacklogFocus() {
    int count = Game::ReadInt(0x1343D04);
    int cur = Game::ReadInt(0x14FB450);
    if (count <= 0 || cur < 0 || cur >= count) return "";
    std::string t = BacklogEntry(cur);
    if (t.empty()) return "";
    return Loc::Format("menu_pos", {{"item", t}, {"pos", std::to_string(cur + 1)}, {"count", std::to_string(count)}});
}

std::string Focus(int slot) {
    if (slot < 0) return "";
    switch (slot) {
        case 11: return BacklogFocus();
        case 1: return SystemFocus();
        case 10: return TipsFocus();
        case 4: return HelpFocus();
        case 0: return TitleFocus();
        case 2: return SaveFocus();
        case 3: return ConfigFocus();
        default: return "";
    }
}

std::string Name(int slot) {
    if (slot < 0) return "";
    if (slot == 2) {  // SaveMenu: Init byte after the sub-opcode: 0 quick load, 1 save, 2 load
        if (g_saveMode == 1) return Loc::T("scr_save");
        if (g_saveMode == 0) return Loc::T("scr_quickload");
        return Loc::T("scr_load");
    }
    return Loc::T(kSlots[slot].nameKey);
}

}  // namespace

void InstallHooks() { InstallFrom<0>(); }

bool InMenu() { return g_screen >= 0; }
std::string CurrentName() { return Name(g_screen); }
std::string CurrentFocus() { return Focus(g_screen); }
std::vector<std::string> ScreenText() { return ScreenLines(); }

void Tick() {
    // Did the in-front menu's instruction run during the frame that just ended?
    bool menuRan = g_screen >= 0 && g_seen[g_screen] == g_frame;
    ++g_frame;
    // A screen is active while its instruction ran in the last two frames.
    int best = -1;
    for (int i = 0; i < kSlotCount; ++i)
        if (g_seen[i] && g_frame - g_seen[i] <= kLeaveFrames && (best < 0 || kSlots[i].priority > kSlots[best].priority)) best = i;
    if (best != g_screen) {
        int prev = g_screen;
        g_screen = best;
        g_focus.clear();
        Log("screen %d -> %d", prev, best);
        if (best < 0) g_lastName.clear();
        if (best >= 0 && best != 12) {
            std::string n = Name(best);
            if (n != g_lastName) {
                g_lastName = n;
                Buffers::Get(Buffers::Events).Add(n);
                Speech::Say(n);
            }
            g_readScreen = best == 9;  // Clear list: statistics, no cursor
        }
    }
    if (menuRan && g_screen != 12) {
        std::set<std::string> snap;
        for (const DrawnText& d : TextCap::LastFrame()) snap.insert(Key(d));
        g_menuFrames.push_back(std::move(snap));
        if (g_menuFrames.size() > 15) g_menuFrames.pop_front();
        g_baseline = g_menuFrames.front();
    }
    if (g_screen < 0 || g_screen == 12) {
        std::string joined;
        std::vector<std::string> lines = ScreenLines(&g_baseline);
        if (lines.empty() && ScreenLines().empty()) {  // the old menu is gone
            g_baseline.clear();
            g_menuFrames.clear();
        }
        for (auto& l : lines) joined += (joined.empty() ? "" : " ") + l;
        // Dialog options (OK/Cancel, Yes/No), left to right; the selected one
        // is the int at RVA 0x1242DC8 (0 first, 1 second; memdiff on the
        // "Load this Data?" dialog).
        std::vector<const DrawnText*> opts;
        for (const DrawnText& d : TextCap::LastFrame())
            if (d.func == 0 && (d.color & 0xFFFFFF) == 0xFFFFFF && !g_baseline.count(Key(d)) &&
                (d.text == "OK" || d.text == "Cancel" || d.text == "Yes" || d.text == "No") &&
                std::none_of(opts.begin(), opts.end(), [&](const DrawnText* o) { return o->text == d.text; }))
                opts.push_back(&d);
        std::sort(opts.begin(), opts.end(), [](const DrawnText* a, const DrawnText* b) { return a->x < b->x; });
        if (opts.size() >= 2 && g_notice == joined && !joined.empty()) {
            int sel = Game::ReadInt(0x1242DC8);
            std::string o = sel >= 0 && sel < (int)opts.size() ? opts[sel]->text : "";
            if (o != g_option) {
                g_option = o;
                if (!o.empty()) Speech::Say(o);
            }
        } else if (opts.empty()) {
            g_option.clear();
        }
        g_noticeStable = joined == g_noticePrev ? g_noticeStable + 1 : 0;
        g_noticePrev = joined;
        if (joined.empty()) g_notice.clear();
        else if (g_noticeStable == kNoticeFrames && joined != g_notice) {
            g_notice = joined;
            Buffers::Get(Buffers::Events).Add(joined);
            Buffers::Get(Buffers::Details).Set({joined});
            Buffers::Select(Buffers::Details);
            Speech::Say(joined);
        }
    }
    if (g_screen < 0) return;
    if (g_readScreen) {
        std::vector<std::string> lines = ScreenLines();
        if (!lines.empty()) {
            g_readScreen = false;
            Buffers::Get(Buffers::Details).Set(lines);
            Buffers::Select(Buffers::Details);
            for (auto& l : lines) Speech::Say(l);
        }
    }
    std::string f = Focus(g_screen);
    if (!f.empty() && f != g_focus) {
        g_focus = f;
        Speech::Say(f, false);
    }
}

}  // namespace Screens
}  // namespace sga
