#include "gfx/picture.h"

#include <cstdio>
#include <vector>

#include "core/log.h"

// JPEG / PNG only, no floating-point HDR (keeps powf / expf out of the Miyoo build), no SIMD (one code path everywhere)
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_NO_SIMD
#define STBI_NO_STDIO
#define STB_IMAGE_IMPLEMENTATION
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#pragma GCC diagnostic ignored "-Wtype-limits"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#endif
#include "stb/stb_image.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

namespace {
bool readFile(const std::string& path, std::vector<unsigned char>& out) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    const long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (n <= 0 || n > 64L * 1024 * 1024) {
        std::fclose(f);
        return false;
    }
    out.resize(size_t(n));
    const bool ok = std::fread(out.data(), 1, out.size(), f) == out.size();
    std::fclose(f);
    return ok;
}
}  // namespace

bool decodePicture(const unsigned char* data, size_t n, Image& out) {
    out = Image();
    int w = 0, h = 0, ch = 0;
    unsigned char* px = stbi_load_from_memory(data, int(n), &w, &h, &ch, 4);
    if (!px) return false;
    out.w = w;
    out.h = h;
    out.px.resize(size_t(w) * size_t(h));
    for (size_t i = 0; i < out.px.size(); ++i) {
        const unsigned char* p = px + i * 4;
        const uint32_t a = p[3];
        out.px[i] = (a << 24) | (uint32_t(p[0] * a / 255) << 16) | (uint32_t(p[1] * a / 255) << 8) | uint32_t(p[2] * a / 255);
    }
    stbi_image_free(px);
    return true;
}

bool loadPicture(const std::string& name, Image& out) {
    out = Image();
#if defined(_WIN32) || defined(TI4_SDL2)
    const char* sizes[] = {"-pc", "-miyoo"};
#else
    const char* sizes[] = {"-miyoo", "-pc"};
#endif
    for (const char* dir : {"art/", "assets/art/", "../assets/art/"})
        for (const char* sz : sizes)
            for (const char* ext : {".jpg", ".png"}) {
                const std::string path = std::string(dir) + name + sz + ext;
                std::vector<unsigned char> bytes;
                if (!readFile(path, bytes)) continue;
                int w = 0, h = 0, n = 0;
                unsigned char* px = stbi_load_from_memory(bytes.data(), int(bytes.size()), &w, &h, &n, 4);
                if (!px) {
                    LOG("picture: can't read %s (%s)", path.c_str(), stbi_failure_reason());
                    continue;
                }
                out.w = w;
                out.h = h;
                out.px.resize(size_t(w) * size_t(h));
                for (size_t i = 0; i < out.px.size(); ++i) {
                    const unsigned char* p = px + i * 4;
                    const uint32_t a = p[3];
                    out.px[i] = (a << 24) | (uint32_t(p[0] * a / 255) << 16) | (uint32_t(p[1] * a / 255) << 8) | uint32_t(p[2] * a / 255);
                }
                stbi_image_free(px);
                return true;
            }
    return false;
}
