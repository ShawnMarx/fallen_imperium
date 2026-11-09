// Sound outputs (0.27.0). See audio.h.
#include "audio/output.h"

#include <atomic>
#include <cstring>
#include <thread>
#include <vector>

#include "audio/audio.h"
#include "core/log.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <mmsystem.h>
#elif defined(TI4_SDL2)
#ifndef __EMSCRIPTEN__
#include <dlfcn.h>
#endif
#include <SDL2/SDL.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>
#include <unistd.h>
#endif

namespace audio {
namespace {

#if defined(_WIN32)
// ------------------------------------------------------------------ Windows
class WaveOut : public Output {
public:
    const char* name() const override { return "Windows waveOut"; }
    bool open(int& rate) override {
        WAVEFORMATEX wf{};
        wf.wFormatTag = WAVE_FORMAT_PCM;
        wf.nChannels = 2;
        wf.nSamplesPerSec = DWORD(rate);
        wf.wBitsPerSample = 16;
        wf.nBlockAlign = 4;
        wf.nAvgBytesPerSec = wf.nSamplesPerSec * 4;
        event_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (waveOutOpen(&wo_, WAVE_MAPPER, &wf, DWORD_PTR(event_), 0, CALLBACK_EVENT) != MMSYSERR_NOERROR) {
            LOG("audio: waveOutOpen failed");
            CloseHandle(event_);
            event_ = nullptr;
            return false;
        }
        for (int i = 0; i < kBufs; ++i) {
            mem_[i].assign(size_t(kFrames) * 2, 0);
            std::memset(&hdr_[i], 0, sizeof hdr_[i]);
            hdr_[i].lpData = reinterpret_cast<LPSTR>(mem_[i].data());
            hdr_[i].dwBufferLength = DWORD(kFrames * 4);
            waveOutPrepareHeader(wo_, &hdr_[i], sizeof hdr_[i]);
        }
        quit_ = false;
        thread_ = std::thread([this]() { loop(); });
        return true;
    }
    void close() override {
        quit_ = true;
        if (event_) SetEvent(event_);
        if (thread_.joinable()) thread_.join();
        if (wo_) {
            waveOutReset(wo_);
            for (int i = 0; i < kBufs; ++i) waveOutUnprepareHeader(wo_, &hdr_[i], sizeof hdr_[i]);
            waveOutClose(wo_);
            wo_ = nullptr;
        }
        if (event_) CloseHandle(event_);
        event_ = nullptr;
    }

private:
    static constexpr int kBufs = 4, kFrames = 1024;  // ~93 ms in flight
    void loop() {
        while (!quit_) {
            for (int i = 0; i < kBufs; ++i) {
                if (hdr_[i].dwFlags & WHDR_INQUEUE) continue;
                mix(mem_[i].data(), kFrames);
                waveOutWrite(wo_, &hdr_[i], sizeof hdr_[i]);
            }
            WaitForSingleObject(event_, 40);
        }
    }
    HWAVEOUT wo_ = nullptr;
    HANDLE event_ = nullptr;
    WAVEHDR hdr_[kBufs];
    std::vector<int16_t> mem_[kBufs];
    std::thread thread_;
    std::atomic<bool> quit_{false};
};

#elif defined(TI4_SDL2)
// ------------------------------------------------------------- Steam Deck
class SdlOut : public Output {
public:
    const char* name() const override { return "SDL2 audio"; }
    bool open(int& rate) override {
#ifdef __EMSCRIPTEN__
        // web: SDL2 is linked in (Emscripten's port; the browser's Web Audio underneath)
        initSub_ = &SDL_InitSubSystem;
        openDev_ = &SDL_OpenAudioDevice;
        pause_ = &SDL_PauseAudioDevice;
        closeDev_ = &SDL_CloseAudioDevice;
        getError_ = &SDL_GetError;
#else
        lib_ = dlopen("libSDL2-2.0.so.0", RTLD_NOW | RTLD_LOCAL);
        if (!lib_) lib_ = dlopen("./lib/libSDL2-2.0.so.0", RTLD_NOW | RTLD_LOCAL);
        if (!lib_) return false;
        auto sym = [&](const char* n) { return dlsym(lib_, n); };
        initSub_ = reinterpret_cast<int (*)(Uint32)>(sym("SDL_InitSubSystem"));
        openDev_ = reinterpret_cast<SDL_AudioDeviceID (*)(const char*, int, const SDL_AudioSpec*, SDL_AudioSpec*, int)>(
            sym("SDL_OpenAudioDevice"));
        pause_ = reinterpret_cast<void (*)(SDL_AudioDeviceID, int)>(sym("SDL_PauseAudioDevice"));
        closeDev_ = reinterpret_cast<void (*)(SDL_AudioDeviceID)>(sym("SDL_CloseAudioDevice"));
        getError_ = reinterpret_cast<const char* (*)()>(sym("SDL_GetError"));
#endif
        if (!initSub_ || !openDev_ || !pause_ || !closeDev_) return false;
        if (initSub_(SDL_INIT_AUDIO) != 0) {
            LOG("audio: SDL audio init failed: %s", getError_ ? getError_() : "?");
            return false;
        }
        SDL_AudioSpec want{}, have{};
        want.freq = rate;
        want.format = AUDIO_S16SYS;
        want.channels = 2;
        want.samples = 1024;
        want.callback = &SdlOut::callback;
        dev_ = openDev_(nullptr, 0, &want, &have, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
        if (dev_ == 0) {
            LOG("audio: SDL_OpenAudioDevice failed: %s", getError_ ? getError_() : "?");
            return false;
        }
        rate = have.freq;
        pause_(dev_, 0);
        return true;
    }
    void close() override {
        if (dev_ && closeDev_) closeDev_(dev_);
        dev_ = 0;
    }

private:
    static void callback(void*, Uint8* stream, int len) { mix(reinterpret_cast<int16_t*>(stream), len / 4); }
    void* lib_ = nullptr;
    SDL_AudioDeviceID dev_ = 0;
    int (*initSub_)(Uint32) = nullptr;
    SDL_AudioDeviceID (*openDev_)(const char*, int, const SDL_AudioSpec*, SDL_AudioSpec*, int) = nullptr;
    void (*pause_)(SDL_AudioDeviceID, int) = nullptr;
    void (*closeDev_)(SDL_AudioDeviceID) = nullptr;
    const char* (*getError_)() = nullptr;
};

#else
// ------------------------------------------------- Miyoo: OSS /dev/dsp
class OssOut : public Output {
public:
    const char* name() const override { return "OSS /dev/dsp"; }
    bool open(int& rate) override {
        fd_ = ::open("/dev/dsp", O_WRONLY);
        if (fd_ < 0) {
            LOG("audio: can't open /dev/dsp (errno %d)", errno);
            return false;
        }
        int frag = (8 << 16) | 11;  // 8 fragments of 2 KB: ~90 ms
        ioctl(fd_, SNDCTL_DSP_SETFRAGMENT, &frag);
        int fmt = AFMT_S16_LE;
        if (ioctl(fd_, SNDCTL_DSP_SETFMT, &fmt) < 0 || fmt != AFMT_S16_LE) LOG("audio: format %d", fmt);
        channels_ = 2;
        if (ioctl(fd_, SNDCTL_DSP_CHANNELS, &channels_) < 0) channels_ = 2;
        int speed = rate;
        if (ioctl(fd_, SNDCTL_DSP_SPEED, &speed) >= 0 && speed > 0) rate = speed;
        LOG("audio: /dev/dsp %d Hz, %d channel(s)", rate, channels_);
        quit_ = false;
        thread_ = std::thread([this]() { loop(); });
        return true;
    }
    void close() override {
        quit_ = true;
        if (thread_.joinable()) thread_.join();
        if (fd_ >= 0) ::close(fd_);
        fd_ = -1;
    }

private:
    void loop() {
        constexpr int kFrames = 512;
        std::vector<int16_t> buf(size_t(kFrames) * 2);
        int errors = 0;
        while (!quit_) {
            mix(buf.data(), kFrames);
            size_t bytes = size_t(kFrames) * 4;
            if (channels_ == 1) {  // fold the two sides together
                for (int i = 0; i < kFrames; ++i) buf[size_t(i)] = int16_t((int(buf[size_t(i) * 2]) + int(buf[size_t(i) * 2 + 1])) / 2);
                bytes = size_t(kFrames) * 2;
            }
            const char* p = reinterpret_cast<const char*>(buf.data());
            while (bytes > 0 && !quit_) {
                const ssize_t n = ::write(fd_, p, bytes);
                if (n < 0) {
                    if (errno == EINTR) continue;
                    if (++errors == 1) LOG("audio: write to /dev/dsp failed (errno %d)", errno);
                    usleep(20000);
                    break;
                }
                p += n;
                bytes -= size_t(n);
            }
        }
    }
    int fd_ = -1;
    int channels_ = 2;
    std::thread thread_;
    std::atomic<bool> quit_{false};
};
#endif

}  // namespace

std::unique_ptr<Output> makeOutput() {
#if defined(_WIN32)
    return std::unique_ptr<Output>(new WaveOut());
#elif defined(TI4_SDL2)
    return std::unique_ptr<Output>(new SdlOut());
#else
    return std::unique_ptr<Output>(new OssOut());
#endif
}

}  // namespace audio
