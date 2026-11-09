// Small deterministic random number generator (PCG32). All game randomness
// goes through this so a saved game replays exactly the same way.
#pragma once
#include <cstdint>
#include <utility>
#include <vector>

namespace ti {

class Rng {
public:
    explicit Rng(uint64_t seed = 1) { reseed(seed); }
    void reseed(uint64_t seed) {
        calls_ = 0;
        state_ = 0;
        inc_ = (seed << 1u) | 1u;
        next();
        state_ += seed ^ 0x9E3779B97F4A7C15ull;
        next();
    }
    uint32_t calls() const { return calls_; }
    uint32_t next() {
        ++calls_;
        uint64_t old = state_;
        state_ = old * 6364136223846793005ull + inc_;
        uint32_t xorshifted = uint32_t(((old >> 18u) ^ old) >> 27u);
        uint32_t rot = uint32_t(old >> 59u);
        return (xorshifted >> rot) | (xorshifted << ((32 - rot) & 31));
    }
    // Uniform integer in [0, n)
    int below(int n) {
        if (n <= 1) return 0;
        uint32_t bound = uint32_t(n);
        uint32_t threshold = uint32_t(-bound) % bound;
        for (;;) {
            uint32_t r = next();
            if (r >= threshold) return int(r % bound);
        }
    }
    int d10() { return 1 + below(10); }
    template <class T>
    void shuffle(std::vector<T>& v) {
        for (int i = int(v.size()) - 1; i > 0; --i) std::swap(v[i], v[below(i + 1)]);
    }

private:
    uint64_t state_ = 0, inc_ = 1;
    uint32_t calls_ = 0;
};

}  // namespace ti
