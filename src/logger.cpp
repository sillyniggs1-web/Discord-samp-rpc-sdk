#include "logger.h"
#include <atomic>
#include <cstdarg>
#include <cstdio>
#ifdef __ANDROID__
#include <android/log.h>
#endif
namespace srpc {
static std::atomic<bool> g_on{true};
static std::atomic<unsigned> g_count{0};
void SetLoggingEnabled(bool on) { g_on = on; }
void Log(int level, const char* fmt, ...) {
    if (!g_on.load()) return;
    // Hard cap so a retry loop can never flood the log.
    if (g_count.fetch_add(1) > 2000) return;
    char buf[384];
    va_list ap; va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
#ifdef __ANDROID__
    static const int prio[] = {ANDROID_LOG_INFO, ANDROID_LOG_WARN, ANDROID_LOG_ERROR};
    __android_log_print(prio[level < 0 ? 0 : level > 2 ? 2 : level], "SampDiscordRPC", "%s", buf);
#else
    fprintf(stderr, "[SampDiscordRPC] %s\n", buf);
#endif
}
}
