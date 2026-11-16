#include "phone.h"

#include <algorithm>
#include <map>
#include <set>

#include <windows.h>

#include "buffers.h"
#include "game.h"
#include "loc.h"
#include "log.h"
#include "settings.h"
#include "speech.h"
#include "textcap.h"

namespace sga {
namespace Phone {

namespace {

std::vector<std::string> g_lines;      // phone text this frame, reading order
std::vector<std::string> g_prevLines;  // previous frame, to wait for stable text
unsigned g_stable = 0;                 // frames the phone text has not changed
std::set<std::string> g_spoken;        // phone text already announced on this screen
int g_screen = -1;                     // ScrWork 6802
std::string g_link;                    // last announced reply link
unsigned g_quietFrames = 0;
int g_work[32];                        // ScrWork 6800..6831, for the debug log

// Mail text is typed out letter by letter; speak only once it has stopped
// changing for this many frames (about a quarter second).
const unsigned kStableFrames = 15;
std::string g_focus;                   // last announced focused entry
// Blinking notices ("Receiving mail") come and go; the same text on the same
// phone screen is not repeated within kRepeatMs.
std::map<std::string, DWORD> g_saidAt;
const DWORD kRepeatMs = 30000;

// Long runs without spaces (the garbled mails) are cut short for speech;
// the full text stays in the review buffer.
std::string Speakable(const std::string& s) {
    if (s.size() < 200 || s.find(' ') != std::string::npos) return s;
    return Loc::Format("phone_garbled", {{"start", s.substr(0, 60)}, {"count", std::to_string(s.size() - 60)}});
}

// The focused entry. Selected items are drawn red (0xFF1111) on most phone
// screens (Inbox/Outbox, actions such as "Call"); the address book (phone
// screen 6) shows a highlight bar instead, with its row index in ScrWork 6821
// (CoZ LanguageBarrier; checked: 0 Daru, 1 PhoneWave, 2 Faris).
std::string Focus(const std::vector<const DrawnText*>& v, int screen) {
    for (auto* d : v)
        if ((d->color & 0xFFFFFF) == 0xFF1111) return d->text;
    if (screen == 23) {
        // Dial pad: ScrWork 6831 = key under the cursor, 3 columns, row by row
        // (mapped with the arrow keys: up from "2" wraps to "0").
        static const char* keys[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "*", "0", "#"};
        int k = Game::Work(6831);
        if (k >= 0 && k < 12) return Loc::Format("phone_key", {{"key", keys[k]}});
    }
    // Lists with a highlight bar: address book (screen 6, ScrWork 6821) and the
    // inbox/outbox mail list (screen 3, ScrWork 6806; 6807 holds the mail id).
    if (screen == 6 || screen == 3) {
        std::vector<std::string> rows;
        for (auto* d : v)
            if ((d->color & 0xFFFFFF) == 0 && std::find(rows.begin(), rows.end(), d->text) == rows.end())
                rows.push_back(d->text);
        int cur = Game::Work(screen == 6 ? 6821 : 6806);
        if (cur >= 0 && cur < (int)rows.size())
            return Loc::Format("phone_entry", {{"entry", rows[cur]}, {"n", std::to_string(cur + 1)},
                                               {"count", std::to_string(rows.size())}});
    }
    return "";
}

void LogWork() {
    std::string ch;
    for (int i = 0; i < 32; ++i) {
        int v = Game::Work(6800 + i);
        if (v != g_work[i]) {
            ch += " " + std::to_string(6800 + i) + "=" + std::to_string(v);
            g_work[i] = v;
        }
    }
    if (!ch.empty()) Log("phone work:%s", ch.c_str());
}

// Screens without text (the dial pad) still have a focus to report.
void TickFocusOnly(const std::vector<const DrawnText*>& v, int screen, bool skipping) {
    std::string f = Focus(v, screen);
    if (f != g_focus) {
        g_focus = f;
        if (!f.empty() && !skipping) Speech::Say(f);
    }
}

// In-scene documents (board posts, PC screens): new lines are read once the
// text has been stable for kStableFrames, in reading order.
std::vector<std::string> g_board, g_boardPrev;
std::set<std::string> g_boardSpoken;
unsigned g_boardStable = 0, g_boardQuiet = 0;

void TickBoard(std::vector<const DrawnText*>& v, bool skipping) {
    std::sort(v.begin(), v.end(), [](const DrawnText* a, const DrawnText* b) {
        return a->y != b->y ? a->y < b->y : a->x < b->x;
    });
    std::vector<std::string> lines;
    for (auto* d : v)
        if (std::find(lines.begin(), lines.end(), d->text) == lines.end()) lines.push_back(d->text);
    if (lines.empty()) {
        if (++g_boardQuiet > 60) g_boardSpoken.clear();
        g_board.clear();
        g_boardPrev.clear();
        return;
    }
    g_boardQuiet = 0;
    g_board = lines;
    g_boardStable = lines == g_boardPrev ? g_boardStable + 1 : 0;
    g_boardPrev = lines;
    if (g_boardStable != kStableFrames) return;
    for (auto& l : lines) {
        if (g_boardSpoken.count(l)) continue;
        g_boardSpoken.insert(l);
        Buffers::Get(Buffers::Events).Add(l);
        if (!skipping) Speech::Say(l);
    }
    Buffers::Get(Buffers::Details).Set(lines);
}

}  // namespace

std::vector<std::string> BoardLines() { return g_board; }

bool Active() { return !g_lines.empty(); }
std::vector<std::string> Lines() { return g_lines; }

void Tick(bool skipping) {
    if (Settings::Get("debug_text")) LogWork();
    // ScrWork 6801 slides 0 -> 20 while the phone comes up and back to 0 when
    // it is put away (debug log while pressing Z and Backspace).
    static int s_slide = 0;
    int slide = Game::Work(6801);
    if (slide >= 20 && s_slide < 20 && !skipping) Speech::Say(Loc::T("phone_open"));
    if (slide == 0 && s_slide > 0 && !skipping) Speech::Say(Loc::T("phone_closed"));
    s_slide = slide;
    // Dial pad: ScrWork 6832 counts the keys entered ("20#" showed 3); when it
    // changes (grows, or restarts at 1 after a failed number), the key under
    // the cursor (6831) was pressed.
    static int s_count = 0;
    int count = Game::Work(6832);
    if (Game::Work(6802) == 23 && count != s_count && count > 0 && !skipping) {
        static const char* keys[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "*", "0", "#"};
        int k = Game::Work(6831);
        if (k >= 0 && k < 12) Speech::Say(Loc::Format("phone_pressed", {{"key", keys[k]}}));
    }
    s_count = count;
    // Phone text: white headers, black entries on the light screen, red
    // actions ("Call", "Sending mail"); all of it is content.
    // The phone is drawn at x >= 880 (1280-wide layout). The same mail-text
    // function also draws in-scene documents on the left: the @channel board
    // and PC screens (seen: "229 :JOHN TITOR ... I see that everyone would
    // like to know more about my time machine." at x 232); those are read by
    // TickBoard as screen text.
    std::vector<const DrawnText*> v, board;
    for (const DrawnText& d : TextCap::LastFrame()) {
        if (d.func != 1 && d.func != 2) continue;
        if (d.x >= 880) v.push_back(&d);
        else if (d.func == 2 && (d.color & 0xFFFFFF) != 0xFFFFFF) board.push_back(&d);
    }
    TickBoard(board, skipping);
    std::sort(v.begin(), v.end(), [](const DrawnText* a, const DrawnText* b) {
        return a->y != b->y ? a->y < b->y : a->x < b->x;
    });
    std::vector<std::string> lines;
    const DrawnText* mail = nullptr;
    for (auto* d : v) {
        if (std::find(lines.begin(), lines.end(), d->text) == lines.end()) lines.push_back(d->text);
        if (d->func == 2) mail = d;
    }
    // The phone screen changed: forget what was said so the new screen is read.
    int screen = Game::Work(6802);
    if (screen != g_screen) {
        Log("phone screen %d -> %d", g_screen, screen);
        if (screen == 23 && !skipping) Speech::Say(Loc::T("phone_dialpad"));
        g_screen = screen;
        // A new phone screen is read in full, except the contact actions
        // (screens 7 and 39), drawn over the address book: only its actions are new.
        if (screen != 7 && screen != 39) g_spoken.clear();
        g_link.clear();
        g_focus.clear();
    }
    if (lines.empty()) {
        TickFocusOnly(v, screen, skipping);
        // Text gone for a while: the phone was put away.
        if (++g_quietFrames > 30) {
            g_spoken.clear();
            g_link.clear();
        }
        g_lines.clear();
        g_prevLines.clear();
        return;
    }
    g_quietFrames = 0;
    g_lines = lines;
    g_stable = lines == g_prevLines ? g_stable + 1 : 0;
    g_prevLines = lines;
    std::vector<std::string> fresh;
    if (g_stable == kStableFrames) {
        // Announce text that newly appeared on the phone.
        for (auto& l : lines)
            if (!g_spoken.count(l)) fresh.push_back(l);
        DWORD now = GetTickCount();
        for (auto& l : fresh) {
            std::string key = std::to_string(screen) + "|" + l;
            auto it = g_saidAt.find(key);
            if (it != g_saidAt.end() && now - it->second < kRepeatMs) continue;
            g_saidAt[key] = now;
            Buffers::Get(Buffers::Events).Add(l);
            if (!skipping) Speech::Say(Speakable(l));
        }
        // What is on the phone now; text that goes away and comes back is new again.
        g_spoken = std::set<std::string>(lines.begin(), lines.end());
        if (!fresh.empty()) {
            Buffers::Get(Buffers::Details).Set(lines);
            Buffers::Select(Buffers::Details);
        }
    }
    if (g_stable >= kStableFrames) {
        std::string f = Focus(v, screen);
        if (f != g_focus) {
            g_focus = f;
            bool justRead = std::find(fresh.begin(), fresh.end(), f) != fresh.end();
            if (!f.empty() && !skipping && !justRead) Speech::Say(f);
        }
    }
    // Interactive mail: the highlighted link word is the reply to send.
    if (mail && !mail->links.empty()) {
        int sel = mail->selectedLink;
        std::string link;
        if (sel >= 0 && sel < (int)mail->links.size())
            link = Loc::Format("phone_link", {{"word", mail->links[sel]}, {"n", std::to_string(sel + 1)},
                                              {"count", std::to_string(mail->links.size())}});
        if (link != g_link) {
            if (g_link.empty() && !skipping)  // first link of this mail: say how to reply
                Speech::Say(Loc::Format("phone_links_hint", {{"count", std::to_string(mail->links.size())}}));
            g_link = link;
            if (!link.empty() && !skipping) Speech::Say(link);
        }
    }
}


}  // namespace Phone
}  // namespace sga
