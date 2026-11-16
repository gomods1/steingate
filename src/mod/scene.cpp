#include "scene.h"

#include <fstream>
#include <map>
#include <set>
#include <sstream>

#include "game.h"
#include "loc.h"
#include "log.h"

namespace sga {
namespace Scene {

namespace {

struct Character {
    std::string name;
    std::vector<std::string> speakerNames;  // dialogue names that reveal it
    std::string intro;                       // appearance before the name is known
};

std::map<int, std::pair<std::string, std::string>> g_bg;   // file id -> (file, description)
std::map<int, std::string> g_charaFile;                     // chara.mpk id -> sprite base name
std::map<std::string, Character> g_chara;                   // prefix -> character
std::map<std::string, std::string> g_sprite;                // sprite base -> outfit/pose
std::set<std::string> g_known;                              // prefixes whose name is known
std::wstring g_knownPath;

int g_bgLayer[32];
int g_lastBg = -1;
std::string g_lastBgText;
int g_chaLayer[32];

std::vector<std::vector<std::string>> ReadTsv(const std::wstring& path) {
    std::vector<std::vector<std::string>> rows;
    std::ifstream f(path, std::ios::binary);
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.size() >= 3 && (uint8_t)line[0] == 0xEF) line.erase(0, 3);
        if (line.empty() || line[0] == '#') continue;
        std::vector<std::string> cols;
        std::stringstream ss(line);
        std::string c;
        while (std::getline(ss, c, '\t')) cols.push_back(c);
        rows.push_back(cols);
    }
    return rows;
}

void SaveKnown() {
    std::ofstream f(g_knownPath, std::ios::binary | std::ios::trunc);
    for (auto& k : g_known) f << k << "\n";
}

bool IsColorFill(int id) { return id < 0 || (unsigned)id >= 0x01000000u; }

std::string CharaText(int fileId, bool withOutfit) {
    auto it = g_charaFile.find(fileId);
    if (it == g_charaFile.end()) return Loc::Format("chara_unknown", {{"id", std::to_string(fileId)}});
    const std::string& base = it->second;  // e.g. MAY_ALA
    std::string prefix = base.substr(0, 3);
    auto c = g_chara.find(prefix);
    std::string who;
    if (c == g_chara.end()) who = base;
    else if (g_known.count(prefix)) who = c->second.name;
    else who = c->second.intro;
    if (!withOutfit) return who;
    auto s = g_sprite.find(base);
    if (s == g_sprite.end() || s->second.empty()) return who;
    return who + ", " + s->second;
}

std::string BgText(int fileId) {
    auto it = g_bg.find(fileId);
    if (it == g_bg.end()) return Loc::Format("bg_unknown", {{"id", std::to_string(fileId)}});
    if (!it->second.second.empty()) return it->second.second;
    return Loc::Format("bg_undescribed", {{"file", it->second.first}});
}

int BitIndex(int mask) {
    for (int i = 0; i < 32; ++i)
        if (mask & (1 << i)) return i;
    return -1;
}

}  // namespace

void Load(const std::wstring& dataDir, const std::wstring& stateDir) {
    for (int i = 0; i < 32; ++i) g_bgLayer[i] = g_chaLayer[i] = -1;
    for (auto& r : ReadTsv(dataDir + L"bg.tsv"))
        if (r.size() >= 2) g_bg[std::stoi(r[0])] = {r[1], r.size() > 2 ? r[2] : ""};
    for (auto& r : ReadTsv(dataDir + L"chara_files.tsv"))
        if (r.size() >= 2) g_charaFile[std::stoi(r[0])] = r[1];
    for (auto& r : ReadTsv(dataDir + L"chara.tsv")) {
        if (r.size() < 4) continue;
        Character c{r[1], {}, r[3]};
        std::stringstream ss(r[2]);
        std::string n;
        while (std::getline(ss, n, ',')) c.speakerNames.push_back(n);
        g_chara[r[0]] = c;
    }
    for (auto& r : ReadTsv(dataDir + L"sprites.tsv"))
        if (r.size() >= 2) g_sprite[r[0]] = r[1];
    g_knownPath = stateDir + L"known_characters.txt";
    std::ifstream f(g_knownPath);
    std::string k;
    while (std::getline(f, k))
        if (!k.empty()) g_known.insert(k);
    Log("scene data: %u backgrounds, %u sprites files, %u characters, %u known",
        (unsigned)g_bg.size(), (unsigned)g_charaFile.size(), (unsigned)g_chara.size(),
        (unsigned)g_known.size());
}

// Layer contents from the load instructions (bit i of the buffer mask = layer i).
void OnBgLoad(int mask, int fileId) {
    for (int i = 0; i < 32; ++i)
        if (mask & (1 << i)) g_bgLayer[i] = fileId;
}

void OnBgRelease(int mask) {
    for (int i = 0; i < 32; ++i)
        if (mask & (1 << i)) g_bgLayer[i] = -1;
}

void OnCharaLoad(int mask, int fileId) {
    for (int i = 0; i < 32; ++i)
        if (mask & (1 << i)) g_chaLayer[i] = fileId;
}

void OnCharaRelease(int mask) {
    for (int i = 0; i < 32; ++i)
        if (mask & (1 << i)) g_chaLayer[i] = -1;
}

// Layer i is shown while flag 2400+i (backgrounds) / 2410+i (characters) is
// set: scripts do "ResetFlag 2400 / BGload 1, id / SetFlag 2400", and
// MACROSYS2 rebuilds both flag groups from $W(2) bits 0-7 and 8-15. Checked
// after loading a save: only flag 2400 set, and layer 0 held the picture on
// screen while layers 1 and 2 held hidden ones. The character flags are a
// HYPOTHESIS by the same pattern until verified with a sprite on screen.
const int kLayers = 8;

// After loading a save the engine restores the layers without running the
// load instructions, so the mod has not seen their file ids. The engine keeps
// each layer's state in a ScrWork block: backgrounds at 4500 + 40 i, characters
// at 5100 + 40 i (MACROSYS2 tables 460 and 459; scripts write $W(5260),
// $W(5272) after CHAloadAtlas 16). The file number sits at +7 (SW_BG1NO) and
// +9 (SW_CHA1NO) by impacto's common profile: HYPOTHESIS until the resync log
// line below matches a picture on screen.
int Resync(int* layers, int i, int base, const char* what) {
    int v = Game::Work(base + 40 * i);
    if (v < 0 || v == 0xFFFF || v == 0xFFFFFF) return -1;
    layers[i] = v;
    Log("scene resync: %s layer %d = %d", what, i, v);
    return v;
}

int VisibleBg(int i) {
    int id = g_bgLayer[i];
    if (id < 0 && Game::Flag(2400 + i)) id = Resync(g_bgLayer, i, 4507, "bg");
    return id >= 0 && !IsColorFill(id) && Game::Flag(2400 + i) ? id : -1;
}
int VisibleChara(int i) {
    int id = g_chaLayer[i];
    if (id < 0 && Game::Flag(2410 + i)) id = Resync(g_chaLayer, i, 5109, "chara");
    return id >= 0 && Game::Flag(2410 + i) ? id : -1;
}
int g_bgShown[kLayers] = {-1, -1, -1, -1, -1, -1, -1, -1};
int g_chaShown[kLayers] = {-1, -1, -1, -1, -1, -1, -1, -1};

std::string Prefix(int charaFileId) {
    auto it = g_charaFile.find(charaFileId);
    return it == g_charaFile.end() ? "" : it->second.substr(0, 3);
}


std::string Appears(int fileId) {
    std::string who = CharaText(fileId, false);
    auto f = g_charaFile.find(fileId);
    auto s = f == g_charaFile.end() ? g_sprite.end() : g_sprite.find(f->second);
    if (s == g_sprite.end() || s->second.empty()) return Loc::Format("chara_appears", {{"who", who}});
    return Loc::Format("chara_appears_outfit", {{"who", who}, {"outfit", s->second}});
}

std::set<std::string> g_announced;             // prefixes announced as on screen
std::map<std::string, unsigned> g_absentSince;  // prefix -> frame it went missing
const unsigned kLeaveFrames = 30;

std::string NameFor(const std::string& prefix) {
    auto c = g_chara.find(prefix);
    if (c == g_chara.end()) return prefix;
    return g_known.count(prefix) ? c->second.name : c->second.intro;
}

std::vector<std::string> Poll(bool backgrounds, bool characters) {
    std::vector<std::string> out;
    for (int i = 0; i < kLayers; ++i) {
        int id = VisibleBg(i);
        if (id == g_bgShown[i]) continue;
        g_bgShown[i] = id;
        if (id < 0) continue;
        g_lastBg = id;
        std::string text = BgText(id);
        if (text != g_lastBgText) {  // the same picture shown again: stay quiet
            g_lastBgText = text;
            if (backgrounds) out.push_back(text);
        }
    }
    // Characters: by name prefix, so pose changes and layer swaps stay quiet.
    // A leave is announced only after the character has been absent for
    // kLeaveFrames (sprites are re-loaded between layers mid-scene).
    static unsigned s_frame = 0;
    ++s_frame;
    int now[kLayers];
    for (int i = 0; i < kLayers; ++i) now[i] = VisibleChara(i);
    std::map<std::string, int> present;  // prefix -> sprite id
    for (int i = 0; i < kLayers; ++i)
        if (now[i] >= 0 && !Prefix(now[i]).empty()) present[Prefix(now[i])] = now[i];
    for (auto& kv : present) {
        g_absentSince.erase(kv.first);
        if (g_announced.insert(kv.first).second && characters)
            out.push_back(Appears(kv.second));
    }
    for (auto it = g_announced.begin(); it != g_announced.end();) {
        if (present.count(*it)) {
            ++it;
            continue;
        }
        auto a = g_absentSince.find(*it);
        if (a == g_absentSince.end()) {
            g_absentSince[*it] = s_frame;
            ++it;
        } else if (s_frame - a->second >= kLeaveFrames) {
            if (characters) out.push_back(Loc::Format("chara_leaves", {{"who", NameFor(*it)}}));
            g_absentSince.erase(a);
            it = g_announced.erase(it);
        } else {
            ++it;
        }
    }
    for (int i = 0; i < kLayers; ++i) g_chaShown[i] = now[i];    return out;
}
void OnSpeaker(const std::string& name) {
    for (auto& kv : g_chara) {
        if (g_known.count(kv.first)) continue;
        for (auto& n : kv.second.speakerNames) {
            if (n == name) {
                g_known.insert(kv.first);
                SaveKnown();
                Log("character name now known: %s", kv.first.c_str());
                break;
            }
        }
    }
}

std::vector<std::string> Describe() {
    std::vector<std::string> out;
    for (int i = 0; i < kLayers; ++i)
        if (VisibleBg(i) >= 0) out.push_back(BgText(VisibleBg(i)));
    for (int i = 0; i < kLayers; ++i)
        if (VisibleChara(i) >= 0) out.push_back(CharaText(VisibleChara(i), true));
    if (out.empty()) out.push_back(Loc::T("scene_nothing"));
    return out;
}

}  // namespace Scene
}  // namespace sga