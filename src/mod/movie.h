#pragma once
#include <string>

namespace sga {

// Movies (Bink 2). The game's burned-in subtitles and pictures are invisible to
// a screen reader, so each movie has a timed track in data/movies/<name>.tsv:
// "start_ms<TAB>text" lines, spoken when playback reaches them. Timing comes
// from the Bink handle's frame counter, so pauses and skips stay in sync.
namespace Movie {
bool InstallHooks(const std::wstring& dataDir);
void Tick();  // game thread, once per frame
}  // namespace Movie

}  // namespace sga
