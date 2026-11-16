// SGALaunch.exe: starts STEINS;GATE with the accessibility mod.
//
// It replaces the game's Launcher.exe, whose custom-drawn buttons a screen
// reader cannot reach. That launcher starts "Game.exe mages_sgate" (observed
// with Win32_Process); this does the same, but through a directory junction
// whose path has no semicolon (%LOCALAPPDATA%\SteinsGateAccess\game ->
// the STEINS;GATE folder), so Windows finds the mod's dinput8.dll in the game
// folder. Nothing is injected.
// Usable as a Steam launch option: "<path>\SGALaunch.exe" %command%
// (the %command% arguments are ignored).
#include <windows.h>
#include <shlobj.h>

#include <string>

namespace {

std::wstring Dir(const std::wstring& p) { return p.substr(0, p.find_last_of(L"\\/") + 1); }

void Fail(const wchar_t* msg) { MessageBoxW(nullptr, msg, L"Steins;Gate Access", MB_OK | MB_ICONERROR); }

bool Exists(const std::wstring& p) { return GetFileAttributesW(p.c_str()) != INVALID_FILE_ATTRIBUTES; }

bool Run(std::wstring cmd) {
    STARTUPINFOW si{sizeof si};
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(nullptr, &cmd[0], nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
        return false;
    WaitForSingleObject(pi.hProcess, 10000);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return code == 0;
}

// The junction must point at this game folder; recreate it if it points elsewhere.
bool EnsureJunction(const std::wstring& link, const std::wstring& gameDir) {
    std::wstring probe = link + L"\\SteinsGateAccess\\SGALaunch.exe";
    wchar_t self[MAX_PATH];
    GetModuleFileNameW(nullptr, self, MAX_PATH);
    if (Exists(link)) {
        HANDLE a = CreateFileW(probe.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                               OPEN_EXISTING, 0, nullptr);
        HANDLE b = CreateFileW(self, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                               OPEN_EXISTING, 0, nullptr);
        BY_HANDLE_FILE_INFORMATION ia{}, ib{};
        bool same = a != INVALID_HANDLE_VALUE && b != INVALID_HANDLE_VALUE && GetFileInformationByHandle(a, &ia) &&
                    GetFileInformationByHandle(b, &ib) && ia.nFileIndexHigh == ib.nFileIndexHigh &&
                    ia.nFileIndexLow == ib.nFileIndexLow && ia.dwVolumeSerialNumber == ib.dwVolumeSerialNumber;
        if (a != INVALID_HANDLE_VALUE) CloseHandle(a);
        if (b != INVALID_HANDLE_VALUE) CloseHandle(b);
        if (same) return true;
        RemoveDirectoryW(link.c_str());  // removes the junction only, never the target
    }
    std::wstring target = gameDir.substr(0, gameDir.size() - 1);
    return Run(L"cmd.exe /c mklink /J \"" + link + L"\" \"" + target + L"\"") && Exists(probe);
}

}  // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    wchar_t buf[MAX_PATH];
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring modDir = Dir(buf);  // ...\STEINS;GATE\SteinsGateAccess\ .
    std::wstring gameDir = Dir(modDir.substr(0, modDir.size() - 1));
    if (!Exists(gameDir + L"Game.exe")) {
        Fail(L"Game.exe not found. The SteinsGateAccess folder must be inside the STEINS;GATE folder.");
        return 1;
    }
    PWSTR local = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &local))) return 1;
    std::wstring base = std::wstring(local) + L"\\SteinsGateAccess";
    CoTaskMemFree(local);
    CreateDirectoryW(base.c_str(), nullptr);
    std::wstring link = base + L"\\game";
    std::wstring runDir = gameDir;
    if (link.find(L';') == std::wstring::npos && EnsureJunction(link, gameDir)) runDir = link + L"\\";
    else Fail(L"Could not create the folder link the mod needs. The game will start without the mod.");

    // Started outside Steam, the Steam DRM needs the app id to avoid a relaunch.
    if (!GetEnvironmentVariableW(L"SteamAppId", nullptr, 0)) {
        SetEnvironmentVariableW(L"SteamAppId", L"412830");
        SetEnvironmentVariableW(L"SteamGameId", L"412830");
    }
    std::wstring exe = runDir + L"Game.exe";
    std::wstring cmd = L"\"" + exe + L"\" mages_sgate";
    STARTUPINFOW si{sizeof si};
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(exe.c_str(), &cmd[0], nullptr, nullptr, FALSE, 0, nullptr, runDir.c_str(), &si, &pi)) {
        Fail(L"Could not start Game.exe.");
        return 1;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return 0;
}
