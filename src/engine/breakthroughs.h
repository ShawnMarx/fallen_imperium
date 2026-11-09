// Thunder's Edge breakthroughs (0.34.0): the per-faction table (breakthroughs.cpp).
#pragma once
#include "engine/data.h"

namespace ti {

struct BreakthroughDef {
    const char* faction;  // kFactions key
    const char* name;
    TechColor a, b;       // synergy between these two colours (None: no synergy)
    const char* text;     // our own wording
    bool done;            // the ability is implemented (the synergy always is)
};

const BreakthroughDef* breakthroughOf(const char* factionKey);

}  // namespace ti
