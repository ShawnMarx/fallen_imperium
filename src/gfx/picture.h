// Pictures loaded from files next to the game (0.30.0: the campaign backdrops).
// JPEG and PNG only (stb_image, public domain). A name "eve_of_the_fall" is
// looked up as art/eve_of_the_fall-pc.jpg on Windows / the Steam Deck (the
// larger one) or art/...-miyoo.jpg on the Miyoo, falling back to the other size
// and to assets/art in a build tree.
#pragma once
#include <cstddef>
#include <string>

#include "gfx/image.h"

bool loadPicture(const std::string& name, Image& out);
// The same from bytes in memory (0.32.0: the Voidfall pack). Premultiplied 0xAARRGGBB.
bool decodePicture(const unsigned char* data, size_t n, Image& out);
