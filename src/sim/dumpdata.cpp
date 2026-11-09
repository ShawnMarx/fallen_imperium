// --dumpdata (0.26.0): the systems and planets as JSON, for the Retrowave art
// baker (tools/gen_retro.py, `make retro`).
#include <cstdio>

#include "engine/data.h"

namespace ti {

namespace {
void str(const char* s) {
    std::putchar('"');
    for (; *s; ++s) {
        if (*s == '"' || *s == '\\') std::putchar('\\');
        std::putchar(*s);
    }
    std::putchar('"');
}
}  // namespace

int runDumpData() {
    std::printf("{\"planets\":[");
    for (int i = 0; i < kPlanetCount; ++i) {
        const PlanetDef& p = kPlanets[i];
        std::printf("%s{\"key\":", i ? "," : "");
        str(p.key);
        std::printf(",\"name\":");
        str(p.name);
        std::printf(",\"res\":%d,\"inf\":%d,\"trait\":%d,\"skip\":%d,\"legendary\":%d,\"home\":%d,\"system\":%d}", p.res, p.inf,
                    int(p.trait), int(p.skip), p.legendary ? 1 : 0, p.home ? 1 : 0, p.system);
    }
    std::printf("],\"systems\":[");
    for (int i = 0; i < kSystemCount; ++i) {
        const SystemDef& s = kSystems[i];
        std::printf("%s{\"key\":", i ? "," : "");
        str(s.key);
        std::printf(",\"name\":");
        str(s.name);
        std::printf(",\"back\":%d,\"wormholes\":%d,\"anomalies\":%d,\"planets\":[", int(s.back), s.wormholes, s.anomalies);
        for (int k = 0; k < s.planetCount; ++k) std::printf("%s%d", k ? "," : "", s.planets[k]);
        std::printf("]}");
    }
    std::printf("]}\n");
    return 0;
}

}  // namespace ti
