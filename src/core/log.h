// Plain-text log written next to the app (ti4.log) so problems on the
// device can be diagnosed by reading the file off the SD card.
#pragma once

namespace logx {
void open(const char* path);
void write(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void close();
// 0.30.0: what the game was doing last (scene / question / event) - written to the log if the game crashes
void crumb(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
const char* lastCrumb();
int fd();  // 0.43.3: the log's file descriptor (-1 if closed) - the crash handler writes the stack straight into it
}  // namespace logx

#define LOG(...) logx::write(__VA_ARGS__)
