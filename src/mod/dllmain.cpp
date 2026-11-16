// dinput8.dll proxy, placed in the game folder. Game.exe imports
// DirectInput8Create; this forwards it to the system DLL and starts the mod.
//
// The game folder "STEINS;GATE" contains a semicolon, which splits the DLL
// search path, so Windows only finds this proxy when Game.exe is started
// through a path without one: SGALaunch.exe starts it through a directory
// junction (%LOCALAPPDATA%\SteinsGateAccess\game). Started any other way
// (Steam's own launcher) the system dinput8.dll loads and the game runs
// without the mod.
#include <windows.h>

#include <string>

#include "mod.h"

using namespace sga;

namespace {
HMODULE g_self = nullptr;
HMODULE g_real = nullptr;

FARPROC Real(const char* name) {
    if (!g_real) {
        wchar_t sys[MAX_PATH];
        GetSystemDirectoryW(sys, MAX_PATH);  // SysWOW64 through WoW64 redirection
        g_real = LoadLibraryW((std::wstring(sys) + L"\\dinput8.dll").c_str());
    }
    return g_real ? GetProcAddress(g_real, name) : nullptr;
}
}  // namespace

extern "C" HRESULT WINAPI DirectInput8Create(HINSTANCE inst, DWORD ver, REFIID riid, LPVOID* out,
                                             LPUNKNOWN outer) {
    using Proc = HRESULT(WINAPI*)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN);
    auto p = (Proc)Real("DirectInput8Create");
    return p ? p(inst, ver, riid, out, outer) : E_FAIL;
}

extern "C" HRESULT WINAPI DllCanUnloadNow() { return S_FALSE; }

extern "C" HRESULT WINAPI DllGetClassObject(REFCLSID clsid, REFIID riid, LPVOID* out) {
    using Proc = HRESULT(WINAPI*)(REFCLSID, REFIID, LPVOID*);
    auto p = (Proc)Real("DllGetClassObject");
    return p ? p(clsid, riid, out) : E_FAIL;
}

extern "C" HRESULT WINAPI DllRegisterServer() { return E_NOTIMPL; }
extern "C" HRESULT WINAPI DllUnregisterServer() { return E_NOTIMPL; }

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_self = inst;
        DisableThreadLibraryCalls(inst);
        // Real start-up runs on its own thread, outside the loader lock.
        HANDLE t = CreateThread(nullptr, 0, [](LPVOID) -> DWORD {
            Mod::Start(g_self);
            return 0;
        }, nullptr, 0, nullptr);
        if (t) CloseHandle(t);
    }
    return TRUE;
}
