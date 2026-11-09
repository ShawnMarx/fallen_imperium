// The comms codec (0.57.0, owner: a Metal Gear-style codec for the negotiations, "more space themed"): the
// pieces the negotiation screen, the deal banners and the betrayal screen are made of - the space backdrop,
// portrait windows that switch on, name plates, the subspace signal, the speech box with its typed line.
#pragma once
#include <cstdint>
#include <string>

#include "gfx/canvas.h"
#include "ui/speech.h"

namespace codec {

// The space backdrop (nebula, stars, a planet horizon with a neon rim) across the whole width between y0 and y1
// (game units); alert = the red betrayal one. Falls back to a dark fill without the art files.
void backdrop(Canvas& c, uint32_t now, bool alert, int y0, int y1);
// A neon frame: a soft glow and a bright line.
void glowFrame(Canvas& c, Rect r, uint32_t col, int radius = 8, int alpha = 255);
// A portrait window: the faction's animated portrait (its emblem without the art), switching on during its first
// 320 ms (`age`), framed in `col`. phase: the loop's offset.
void window(Canvas& c, int faction, Rect r, uint32_t col, uint32_t now, uint32_t age, int alpha = 255, int phase = 0, bool alert = false);
// The name plate under a window: emblem, name in `col`, a line under it.
void plate(Canvas& c, int faction, Rect r, const std::string& name, const std::string& sub, uint32_t col, int alpha = 255);
// The subspace signal centred on cx from y: label, frequency, bars that jump while someone talks; lost = the
// red flat line of a broken channel.
void signal(Canvas& c, int cx, int y, const std::string& freq, uint32_t col, uint32_t now, bool talking, bool lost, int alpha = 255);
// The speech box: who talks (in `col`), the words typed so far, a blinking cursor while typing.
void speechBox(Canvas& c, Rect r, const std::string& who, uint32_t col, const speech::Speech& s, int shown, uint32_t now,
               bool alert, int alpha = 255);
// The text area of a speech box of rectangle r (where speechBox puts the words).
Rect speechArea(Rect r);
// "7.21": a frequency for a pair of players.
std::string frequency(uint32_t seed);

}  // namespace codec
