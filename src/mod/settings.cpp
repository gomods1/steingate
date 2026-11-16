#include "settings.h"

#include <cstring>
#include <fstream>

#include "log.h"

namespace sga {
namespace Settings {

namespace {
std::wstring g_path;
std::vector<Setting> g_all = {
    {"speak_names", 1, 0, 1},       // say the speaker's name before each line
    {"strip_quotes", 1, 0, 1},      // drop the quote marks around spoken lines
    {"scene_desc", 1, 0, 1},        // describe backgrounds and event pictures
    {"chara_desc", 1, 0, 1},        // announce characters who appear
    {"movie_subs", 1, 0, 1},        // speak movie subtitles and descriptions
    {"tips_notify", 1, 0, 1},       // announce newly unlocked TIPS
    {"interrupt_lines", 0, 0, 1},   // a new dialogue line cuts the previous one
    {"debug_text", 0, 0, 1},        // developer aid: log the text the game draws
};
}  // namespace

void Load(const std::wstring& path) {
    g_path = path;
    std::ifstream f(path);
    std::string line;
    while (std::getline(f, line)) {
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string k = line.substr(0, eq);
        for (auto& s : g_all) {
            if (k != s.key) continue;
            try {
                int v = std::stoi(line.substr(eq + 1));
                if (v < s.min || v > s.max) throw 0;
                s.value = v;
            } catch (...) {
                Log("settings: bad value for %s, keeping default", s.key);
            }
        }
    }
}

void Save() {
    std::ofstream f(g_path, std::ios::trunc);
    for (auto& s : g_all) f << s.key << "=" << s.value << "\n";
}

int Get(const char* key) {
    for (auto& s : g_all)
        if (strcmp(s.key, key) == 0) return s.value;
    return 0;
}

std::vector<Setting>& All() { return g_all; }

}  // namespace Settings
}  // namespace sga
