// Sound outputs (0.27.0): each pulls audio::mix() on its own thread.
#pragma once
#include <memory>

namespace audio {

class Output {
public:
    virtual ~Output() = default;
    // rate: asked for in, what the device gives out. False = no sound here.
    virtual bool open(int& rate) = 0;
    virtual void close() = 0;
    virtual const char* name() const = 0;
};

// Windows: waveOut. Steam Deck / Linux (TI4_SDL2): SDL2 audio.
// Miyoo (and other Linux): OSS /dev/dsp.
std::unique_ptr<Output> makeOutput();

}  // namespace audio
