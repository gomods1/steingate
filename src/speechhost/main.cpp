// SpeechHost.exe (x64): owns Prism for the 32-bit game.
// The mod DLL inside Game.exe creates a named pipe and starts this process with
// "<pipe name> <parent pid>". Protocol: one UTF-8 line per command,
//   S<text>  speak queued      I<text>  speak interrupting
//   X        stop speech       Q        quit
// The host exits when the pipe breaks or the parent process ends.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>

#include <cstdio>
#include <share.h>
#include <string>

#include "prism.h"

namespace {

FILE* g_log = nullptr;

void Log(const char* fmt, const std::string& a = std::string()) {
    if (!g_log) return;
    SYSTEMTIME t;
    GetLocalTime(&t);
    fprintf(g_log, "%02d:%02d:%02d.%03d ", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
    fprintf(g_log, fmt, a.c_str());
    fputc('\n', g_log);
    fflush(g_log);
}

std::wstring ExeDir() {
    wchar_t buf[MAX_PATH];
    DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring p(buf, n);
    return p.substr(0, p.find_last_of(L"\\/") + 1);
}

struct Prism {
    PrismContext* ctx = nullptr;
    PrismBackend* backend = nullptr;
    bool useOutput = false;
    DWORD lastCreate = 0;

    bool Create() {
        lastCreate = GetTickCount();
        if (!ctx) ctx = prism_init(nullptr);
        if (!ctx) {
            Log("prism_init failed");
            return false;
        }
        backend = prism_registry_create_best(ctx);
        if (!backend) {
            Log("no speech backend available");
            return false;
        }
        uint64_t f = prism_backend_get_features(backend);
        useOutput = (f & PRISM_BACKEND_SUPPORTS_OUTPUT) != 0;
        Log("backend: %s", prism_backend_name(backend));
        return true;
    }

    void Recreate() {
        // Rate-limited: a screen reader started after the game is picked up.
        if (GetTickCount() - lastCreate < 3000) return;
        if (backend) prism_backend_free(backend);
        backend = nullptr;
        Create();
    }

    void Say(const std::string& text, bool interrupt) {
        if (!backend) Recreate();
        if (!backend) return;
        PrismError e = useOutput ? prism_backend_output(backend, text.c_str(), interrupt)
                                 : prism_backend_speak(backend, text.c_str(), interrupt);
        if (e != PRISM_OK) {
            Log("speak error: %s", prism_error_string(e));
            if (e == PRISM_ERROR_INTERNAL || e == PRISM_ERROR_NOT_INITIALIZED) Recreate();
        }
    }

    void Stop() {
        if (backend) (void)prism_backend_stop(backend);
    }
};

}  // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argc < 3) return 1;
    std::wstring dir = ExeDir();
    g_log = _wfsopen((dir + L"speechhost.log").c_str(), L"w", _SH_DENYWR);
    Log("SpeechHost start");
    // prism.dll is delay-loaded: load it by full path first, because the
    // install folder "STEINS;GATE" contains a semicolon, which breaks the
    // DLL search path.
    if (!LoadLibraryW((dir + L"prism.dll").c_str())) {
        Log("cannot load prism.dll");
        return 3;
    }

    DWORD parentPid = wcstoul(argv[2], nullptr, 10);
    HANDLE parent = OpenProcess(SYNCHRONIZE, FALSE, parentPid);

    HANDLE pipe = INVALID_HANDLE_VALUE;
    for (int i = 0; i < 100 && pipe == INVALID_HANDLE_VALUE; ++i) {
        pipe = CreateFileW(argv[1], GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (pipe == INVALID_HANDLE_VALUE) Sleep(50);
    }
    if (pipe == INVALID_HANDLE_VALUE) {
        Log("could not open pipe");
        return 2;
    }

    Prism prism;
    prism.Create();

    std::string pending;
    char buf[8192];
    for (;;) {
        DWORD got = 0;
        if (!ReadFile(pipe, buf, sizeof buf, &got, nullptr) || got == 0) {
            Log("pipe closed");
            break;
        }
        pending.append(buf, got);
        size_t nl;
        while ((nl = pending.find('\n')) != std::string::npos) {
            std::string line = pending.substr(0, nl);
            pending.erase(0, nl + 1);
            if (line.empty()) continue;
            char cmd = line[0];
            std::string text = line.substr(1);
            if (cmd == 'S' || cmd == 'I') {
                prism.Say(text, cmd == 'I');
            } else if (cmd == 'X') {
                prism.Stop();
            } else if (cmd == 'Q') {
                return 0;
            }
        }
        if (parent && WaitForSingleObject(parent, 0) == WAIT_OBJECT_0) break;
    }
    return 0;
}
