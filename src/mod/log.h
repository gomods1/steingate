#pragma once
#include <string>

namespace sga {

// Mod log (SteinsGateAccess\sga.log) and spoken-line log (spoken.log).
void LogInit(const std::wstring& dir);
void Log(const char* fmt, ...);
void LogSpoken(const std::string& line);

std::string WideToUtf8(const std::wstring& w);
std::wstring Utf8ToWide(const std::string& s);

}  // namespace sga
