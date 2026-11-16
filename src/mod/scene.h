#pragma once
#include <string>
#include <vector>

namespace sga {

// What is on screen: backgrounds, event CGs, insert images and character
// sprites, and their audio descriptions (data/bg.tsv, data/chara.tsv,
// data/sprites.tsv). Layer contents come from the load instructions; whether
// a layer is shown comes from the script's layer flags (polled every frame),
// so pictures loaded in advance are not announced until they appear.
namespace Scene {
void Load(const std::wstring& dataDir, const std::wstring& stateDir);

void OnBgLoad(int bufferMask, int fileId);
void OnBgRelease(int bufferMask);
void OnCharaLoad(int bufferMask, int fileId);
void OnCharaRelease(int bufferMask);
void OnSpeaker(const std::string& name);  // marks a character's name as known

// Once per frame: lines for pictures and characters that just became visible.
std::vector<std::string> Poll(bool backgrounds, bool characters);

// Full description of the current scene (D key), one item per line.
std::vector<std::string> Describe();
}  // namespace Scene

}  // namespace sga
