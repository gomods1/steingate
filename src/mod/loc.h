#pragma once
#include <string>
#include <utility>
#include <vector>

namespace sga {

// Every string the mod speaks comes from here (rpg.md section 20). English is
// built in; data/lang/<code>.txt ("key=value" lines) overrides it.
namespace Loc {
void Load(const std::wstring& dataDir, const std::string& lang);
std::string T(const char* key);
// Named placeholders: "{name}" replaced by the value with that name.
std::string Format(const char* key, const std::vector<std::pair<std::string, std::string>>& args);
}  // namespace Loc

}  // namespace sga
