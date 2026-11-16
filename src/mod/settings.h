#pragma once
#include <string>
#include <vector>

namespace sga {

// Mod settings. Changed only from the mod menu (H); settings.ini is storage.
struct Setting {
    const char* key;      // ini key and string key "set_<key>"
    int value;
    int min, max;         // booleans are 0..1
};

namespace Settings {
void Load(const std::wstring& path);
void Save();
int Get(const char* key);
std::vector<Setting>& All();
}  // namespace Settings

}  // namespace sga
