#include <cstdio>
#include <ctime>
#include <string>

#include "platform/platform.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

void monotonicNow(timespec* ts) {
    static const long long freq = [] {
        LARGE_INTEGER f;
        QueryPerformanceFrequency(&f);
        return f.QuadPart;
    }();
    LARGE_INTEGER c;
    QueryPerformanceCounter(&c);
    ts->tv_sec = time_t(c.QuadPart / freq);
    ts->tv_nsec = long((c.QuadPart % freq) * 1000000000LL / freq);
}

void sleepMs(uint32_t ms) { Sleep(ms); }  // 1 ms accurate: the window backend calls timeBeginPeriod(1)

bool replaceFile(const std::string& from, const std::string& to) {
    return MoveFileExA(from.c_str(), to.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
}

std::unique_ptr<Platform> makeFbdevPlatform() { return nullptr; }
std::unique_ptr<Platform> makeSdlPlatform() { return nullptr; }

#else
#include <sys/syscall.h>
#include <unistd.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
// web: the ordinary clock, and a sleep that hands the browser a turn (Asyncify)
void monotonicNow(timespec* ts) {
    if (clock_gettime(CLOCK_MONOTONIC, ts) != 0) {
        ts->tv_sec = 0;
        ts->tv_nsec = 0;
    }
}

void sleepMs(uint32_t ms) { emscripten_sleep(ms); }

std::unique_ptr<Platform> makeFbdevPlatform() { return nullptr; }  // (Miyoo backends: not in the web build)
std::unique_ptr<Platform> makeSdlPlatform() { return nullptr; }
#else
// clock_gettime is called through syscall() so the program only needs glibc
// features that every Miyoo firmware has (the libc wrapper needs glibc 2.17).
void monotonicNow(timespec* ts) {
    if (syscall(SYS_clock_gettime, CLOCK_MONOTONIC, ts) != 0) {
        ts->tv_sec = 0;
        ts->tv_nsec = 0;
    }
}

void sleepMs(uint32_t ms) {
    timespec ts{};
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = long(ms % 1000) * 1000000L;
    nanosleep(&ts, nullptr);
}
#endif

bool replaceFile(const std::string& from, const std::string& to) { return std::rename(from.c_str(), to.c_str()) == 0; }

std::unique_ptr<Platform> makeWin32Platform() { return nullptr; }
#endif
#ifndef TI4_SDL2
std::unique_ptr<Platform> makeSdl2Platform() { return nullptr; }
#endif

uint32_t monotonicMs() {
    timespec ts{};
    monotonicNow(&ts);
    return uint32_t(uint64_t(ts.tv_sec) * 1000u + uint64_t(ts.tv_nsec) / 1000000u);
}
