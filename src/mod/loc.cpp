#include "loc.h"

#include <fstream>
#include <unordered_map>

#include "log.h"

namespace sga {
namespace Loc {

namespace {
std::unordered_map<std::string, std::string> g_en, g_lang;

void ReadFile(const std::wstring& path, std::unordered_map<std::string, std::string>& into) {
    std::ifstream f(path, std::ios::binary);
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.size() >= 3 && (uint8_t)line[0] == 0xEF) line.erase(0, 3);  // BOM
        if (line.empty() || line[0] == '#') continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        into[line.substr(0, eq)] = line.substr(eq + 1);
    }
}
}  // namespace

void Load(const std::wstring& dataDir, const std::string& lang) {
    ReadFile(dataDir + L"lang\\en.txt", g_en);
    if (lang != "en") ReadFile(dataDir + L"lang\\" + Utf8ToWide(lang) + L".txt", g_lang);
    Log("strings: %u English, %u %s", (unsigned)g_en.size(), (unsigned)g_lang.size(), lang.c_str());
}

std::string T(const char* key) {
    auto it = g_lang.find(key);
    if (it != g_lang.end()) return it->second;
    it = g_en.find(key);
    if (it != g_en.end()) return it->second;
    Log("missing string: %s", key);
    return key;
}

std::string Format(const char* key, const std::vector<std::pair<std::string, std::string>>& args) {
    std::string s = T(key);
    for (auto& a : args) {
        std::string tag = "{" + a.first + "}";
        size_t p;
        while ((p = s.find(tag)) != std::string::npos) s.replace(p, tag.size(), a.second);
    }
    return s;
}

}  // namespace Loc
}  // namespace sga
