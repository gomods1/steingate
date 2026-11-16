#include "log.h"

#include <windows.h>

#include <cstdarg>
#include <cstdio>
#include <share.h>
#include <mutex>

namespace sga {

namespace {
FILE* g_log = nullptr;
FILE* g_spoken = nullptr;
std::mutex g_mu;

void Stamp(FILE* f) {
    SYSTEMTIME t;
    GetLocalTime(&t);
    fprintf(f, "%02d:%02d:%02d.%03d ", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
}
}  // namespace

void LogInit(const std::wstring& dir) {
    // Shared read so the logs can be read while the game runs.
    g_log = _wfsopen((dir + L"sga.log").c_str(), L"w", _SH_DENYWR);
    g_spoken = _wfsopen((dir + L"spoken.log").c_str(), L"w", _SH_DENYWR);
}

void Log(const char* fmt, ...) {
    std::lock_guard<std::mutex> lock(g_mu);
    if (!g_log) return;
    Stamp(g_log);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(g_log, fmt, ap);
    va_end(ap);
    fputc('\n', g_log);
    fflush(g_log);
}

void LogSpoken(const std::string& line) {
    std::lock_guard<std::mutex> lock(g_mu);
    if (!g_spoken) return;
    Stamp(g_spoken);
    fputs(line.c_str(), g_spoken);
    fputc('\n', g_spoken);
    fflush(g_spoken);
}

std::string WideToUtf8(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), &s[0], n, nullptr, nullptr);
    return s;
}

std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &w[0], n);
    return w;
}

}  // namespace sga
