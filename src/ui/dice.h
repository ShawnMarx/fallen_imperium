// Combat dice (0.22.0, owner's Polish list): black-grey ten-sided dice with the
// roller's faction emblem in the corner. They tumble for a moment, settle one
// after another, hits turn green, then they fade away.
#pragma once
#include <cstdint>
#include <vector>

#include "gfx/canvas.h"

namespace dice {

// One D10 centred on (cx, cy), `size` pixels tall. value 1-10 (0 shows 10).
// spin: 0..1 while it is still rolling (tumbling, flickering numbers), < 0 once
// settled. hit: a settled hit is green. faction: emblem in the corner (-1: none).
void drawD10(Canvas& c, float cx, float cy, float size, int value, bool hit, float spin, int faction, int alpha = 255);

// Where one roll is in its little life, from the time it started (ms):
// before `appear` nothing shows; die i rolls until settle(i); fadeAt > 0 starts
// the fade (350 ms, scaled).
struct Timing {
    uint32_t start = 0;   // when the roll was presented
    uint32_t appear = 0;  // delay before the dice show up
    uint32_t roll = 0;    // how long the slowest die tumbles
    uint32_t fadeAt = 0;  // 0 = not fading yet
    uint32_t fade = 350;
    // spin of die i (of n) at time now: 0..1 rolling, -1 settled
    float spin(uint32_t now, int i, int n) const;
    int alpha(uint32_t now) const;  // 0..255 (0 = gone)
    bool gone(uint32_t now) const { return fadeAt && int32_t(now - fadeAt) >= int32_t(fade); }
    bool settled(uint32_t now, int n) const;
};

// A row (or rows) of dice: at most `perRow` across, rows going up from the
// bottom-centre point (cx, bottom). Returns the height used.
int drawRoll(Canvas& c, float cx, float bottom, float size, int perRow, const std::vector<int8_t>& values, const std::vector<int8_t>& need,
             int faction, const Timing& t, uint32_t now);

}  // namespace dice
