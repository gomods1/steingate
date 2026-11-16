#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace sga {

// Captures the UI strings the game draws each frame (save slots, phone,
// system messages...) by hooking its text-drawing functions.
struct DrawnText {
    int func;          // which draw function (see textcap.cpp)
    int x, y;          // position argument as passed by the game
    uint32_t color;
    int selectedLink;  // interactive mail: highlighted link index, else -1
    std::string text;  // decoded UTF-8 (line breaks as spaces)
    std::vector<std::string> links;  // interactive mail: link texts in order
};

namespace TextCap {
bool Install();
// Called once per frame: makes the strings drawn since the previous call
// available through LastFrame() and starts a new capture.
void EndFrame();
const std::vector<DrawnText>& LastFrame();
// Developer aid: log the set of strings whenever it changes.
void SetDebugLog(bool on);
}  // namespace TextCap

}  // namespace sga
