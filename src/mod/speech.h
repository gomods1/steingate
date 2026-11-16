#pragma once
#include <string>

namespace sga {

// The only code that talks to the speech host. Thread-safe: lines are queued
// and a dedicated thread writes them to SpeechHost.exe over a named pipe.
namespace Speech {
bool Init(const std::wstring& modDir);
// interrupt=false queues after current speech (the default everywhere).
void Say(const std::string& text, bool interrupt = false);
void Stop();
}  // namespace Speech

}  // namespace sga
