#include "speech.h"

#include <windows.h>

#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>

#include "log.h"

namespace sga {
namespace Speech {

namespace {
std::mutex g_mu;
std::condition_variable g_cv;
std::deque<std::string> g_queue;
std::wstring g_hostPath;
std::wstring g_pipeName;
HANDLE g_pipe = INVALID_HANDLE_VALUE;

bool StartHost() {
    if (g_pipe != INVALID_HANDLE_VALUE) {
        CloseHandle(g_pipe);
        g_pipe = INVALID_HANDLE_VALUE;
    }
    g_pipe = CreateNamedPipeW(g_pipeName.c_str(), PIPE_ACCESS_OUTBOUND,
                              PIPE_TYPE_BYTE | PIPE_WAIT, 1, 64 * 1024, 0, 0, nullptr);
    if (g_pipe == INVALID_HANDLE_VALUE) {
        Log("speech: CreateNamedPipe failed %lu", GetLastError());
        return false;
    }
    std::wstring cmd = L"\"" + g_hostPath + L"\" " + g_pipeName + L" " +
                       std::to_wstring(GetCurrentProcessId());
    STARTUPINFOW si{sizeof si};
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(g_hostPath.c_str(), &cmd[0], nullptr, nullptr, FALSE,
                        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        Log("speech: cannot start SpeechHost.exe (%lu)", GetLastError());
        return false;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    BOOL ok = ConnectNamedPipe(g_pipe, nullptr) || GetLastError() == ERROR_PIPE_CONNECTED;
    Log("speech: host %s (pid %lu)", ok ? "connected" : "failed to connect", pi.dwProcessId);
    return ok != FALSE;
}

bool Write(const std::string& line) {
    DWORD w = 0;
    return WriteFile(g_pipe, line.data(), (DWORD)line.size(), &w, nullptr) && w == line.size();
}

void Worker() {
    bool up = StartHost();
    DWORD lastRestart = GetTickCount();
    for (;;) {
        std::string line;
        {
            std::unique_lock<std::mutex> lock(g_mu);
            g_cv.wait(lock, [] { return !g_queue.empty(); });
            line = std::move(g_queue.front());
            g_queue.pop_front();
        }
        if (!up || !Write(line)) {
            // Host died: restart it at most every 5 seconds and resend.
            if (GetTickCount() - lastRestart > 5000) {
                lastRestart = GetTickCount();
                up = StartHost();
                if (up) Write(line);
            }
        }
    }
}

std::string Clean(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) out += (c == '\n' || c == '\r') ? ' ' : c;
    return out;
}
}  // namespace

bool Init(const std::wstring& modDir) {
    g_hostPath = modDir + L"SpeechHost.exe";
    g_pipeName = L"\\\\.\\pipe\\SteinsGateAccess_" + std::to_wstring(GetCurrentProcessId());
    std::thread(Worker).detach();
    return true;
}

void Say(const std::string& text, bool interrupt) {
    std::string t = Clean(text);
    if (t.empty()) return;
    LogSpoken(t);
    std::lock_guard<std::mutex> lock(g_mu);
    g_queue.push_back(std::string(interrupt ? "I" : "S") + t + "\n");
    g_cv.notify_one();
}

void Stop() {
    std::lock_guard<std::mutex> lock(g_mu);
    g_queue.push_back("X\n");
    g_cv.notify_one();
}

}  // namespace Speech
}  // namespace sga
