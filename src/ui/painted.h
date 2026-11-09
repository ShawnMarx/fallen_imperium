// Voidfall's painted ground units (0.51.1, owner: "that aesthetic for the Voidfall ground troops"). A unit is drawn
// alone - on black and on white, for its see-through parts - and then re-lit like the Voidfall tiles: a warm key
// light from up and to the left, the lower right in violet shadow, the dark parts pulled toward the void's violet, a
// warm edge along the upper left of the outline and a violet rim along the upper right (the void's light from
// behind). Bright, glowing parts keep their light. Each picture is made once per key (the caller rounds the pose so
// pictures are shared and reused) and forgotten when not drawn for a while.
#pragma once
#include <cstdint>
#include <functional>

#include "gfx/image.h"

class Canvas;

namespace painted {

// How strongly each part of the light is laid on (0..255).
struct Light {
    int violet = 75;   // the darks toward violet (not so far that dark units sink into the violet ground)
    int shade = 105;   // the lower right in shadow
    int lift = 60;     // the key light on the upper left
    int rim = 200;     // the violet rim on the upper right outline
    int key = 90;      // the warm edge on the upper left outline
};

struct Lit {
    Image img;           // premultiplied, in real pixels at the canvas's scale
    int ax = 0, ay = 0;  // the anchor's pixel in it
    uint32_t used = 0;
};

// draw(o, ax, ay) draws the unit with its anchor at (ax, ay) in o's game units; (left, top, w, h) is the box round
// the anchor that holds it (game units). nullptr: nothing to draw.
const Lit* relight(const Canvas& c, uint64_t key, float left, float top, float w, float h,
                   const std::function<void(Canvas&, float, float)>& draw, uint32_t now, const Light& light = Light());
// Draws it with its anchor at (x, y) (game units).
void drawLit(Canvas& c, const Lit& l, float x, float y, int alpha);

// The light's colours (also used by the hand-painted soldier and the painted unit pictures).
constexpr uint32_t kShade = 0x2C1250;   // the void's shadow
constexpr uint32_t kDeep = 0x1A0A34;    // the lower right
constexpr uint32_t kRim = 0xC496FF;     // the void's light on the edges
constexpr uint32_t kWarm = 0xFFDEB6;    // the key light
constexpr uint32_t kWarmHi = 0xFFECD2;  // the brightest light

}  // namespace painted
