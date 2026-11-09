// Quick checks of the pixel conversion used on the real screen (run by
// `make test` via `ti4 --selftest`). Returns the number of failures.
#include <cstdio>
#include <vector>

#include "platform/pixelconv.h"

int runSelfTest() {
    int fails = 0;
    Canvas c(4, 3);
    c.clear(rgb(0, 0, 0));
    c.pixel(0, 0, rgb(255, 0, 0));    // top-left red
    c.pixel(3, 2, rgb(0, 0, 255));    // bottom-right blue
    c.pixel(1, 0, rgb(0, 255, 0));    // green next to it
    auto check = [&](bool ok, const char* what) {
        if (!ok) {
            std::printf("SELFTEST FAIL: %s\n", what);
            ++fails;
        }
    };
    // 32 bpp, same size, no rotation
    {
        std::vector<uint32_t> d(12, 0xDEADBEEF);
        PixelFormatDesc f;
        convertFrame(c, reinterpret_cast<uint8_t*>(d.data()), 16, 4, 3, f, false);
        check(d[0] == 0xFF0000 && d[11] == 0x0000FF && d[1] == 0x00FF00, "32bpp copy");
    }
    // 32 bpp rotated: top-left must land bottom-right
    {
        std::vector<uint32_t> d(12, 0);
        PixelFormatDesc f;
        convertFrame(c, reinterpret_cast<uint8_t*>(d.data()), 16, 4, 3, f, true);
        check(d[11] == 0xFF0000 && d[0] == 0x0000FF && d[10] == 0x00FF00, "32bpp rotate");
    }
    // 32 bpp BGR order
    {
        std::vector<uint32_t> d(12, 0);
        PixelFormatDesc f;
        f.rShift = 0;
        f.bShift = 16;
        convertFrame(c, reinterpret_cast<uint8_t*>(d.data()), 16, 4, 3, f, false);
        check(d[0] == 0x0000FF && d[11] == 0xFF0000, "32bpp BGR");
    }
    // 16 bpp RGB565, centred in a larger screen with rotation
    {
        std::vector<uint16_t> d(6 * 5, 0);
        PixelFormatDesc f;
        f.bpp = 16;
        f.rShift = 11;
        f.gShift = 5;
        f.bShift = 0;
        f.rBits = 5;
        f.gBits = 6;
        f.bBits = 5;
        convertFrame(c, reinterpret_cast<uint8_t*>(d.data()), 12, 6, 5, f, true);
        // canvas (0,0) -> screen (1,1) unrotated -> rotated (6-1-1, 5-1-1) = (4,3)
        check(d[3 * 6 + 4] == 0xF800, "16bpp rotate red");
        check(d[1 * 6 + 1] == 0x001F, "16bpp rotate blue");
    }
    // colour mixing stays inside each channel
    {
        uint32_t m = mix(rgb(255, 0, 0), rgb(0, 0, 255), 128);
        int r = int(m >> 16) & 255, g = int(m >> 8) & 255, b = int(m) & 255;
        check(r >= 125 && r <= 130 && g == 0 && b >= 125 && b <= 130, "mix halves");
    }
    check(mix(rgb(255, 255, 255), rgb(0, 0, 0), 0) == rgb(255, 255, 255), "mix none");
    std::printf("selftest: %d failure(s)\n", fails);
    return fails;
}
