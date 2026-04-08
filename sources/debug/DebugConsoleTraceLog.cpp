#include "debug/DebugConsoleTraceLog.h"

#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <string>
#include <vector>

#include "debug/DebugConsole.h"
#include "raylib.h"

struct PendingDebugTraceLine {
    int logLevel = LOG_INFO;
    std::string text;
};

static std::mutex gPendingDebugTraceMutex;
static std::vector<PendingDebugTraceLine> gPendingDebugTraceLines;

static const char* TraceLogLevelToText(int logLevel)
{
    switch (logLevel) {
        case LOG_TRACE:   return "TRACE";
        case LOG_DEBUG:   return "DEBUG";
        case LOG_INFO:    return "INFO";
        case LOG_WARNING: return "WARNING";
        case LOG_ERROR:   return "ERROR";
        case LOG_FATAL:   return "FATAL";
        default:          return "LOG";
    }
}

static Color TraceLogLevelToColor(int logLevel)
{
    switch (logLevel) {
        case LOG_TRACE:
            return GRAY;

        case LOG_DEBUG:
            return LIGHTGRAY;

        case LOG_INFO:
            return SKYBLUE;

        case LOG_WARNING:
            return YELLOW;

        case LOG_ERROR:
            return ORANGE;

        case LOG_FATAL:
            return RED;

        default:
            return LIGHTGRAY;
    }
}

static void DebugConsoleTraceLogCallback(int logLevel, const char* text, va_list args)
{
    char buffer[2048];
    buffer[0] = '\0';

    vsnprintf(buffer, sizeof(buffer), text, args);

    const char* levelText = TraceLogLevelToText(logLevel);
    FILE* out = (logLevel >= LOG_ERROR) ? stderr : stdout;

    std::fprintf(out, "[%s] %s\n", levelText, buffer);
    std::fflush(out);

    PendingDebugTraceLine line;
    line.logLevel = logLevel;
    line.text = std::string("[") + levelText + "] " + buffer;

    std::lock_guard<std::mutex> lock(gPendingDebugTraceMutex);

    gPendingDebugTraceLines.push_back(std::move(line));

    static constexpr size_t MAX_PENDING_TRACE_LINES = 512;
    if (gPendingDebugTraceLines.size() > MAX_PENDING_TRACE_LINES) {
        const size_t overflow = gPendingDebugTraceLines.size() - MAX_PENDING_TRACE_LINES;
        gPendingDebugTraceLines.erase(
                gPendingDebugTraceLines.begin(),
                gPendingDebugTraceLines.begin() + static_cast<std::ptrdiff_t>(overflow));
    }
}

void InstallDebugConsoleTraceLogHook()
{
    SetTraceLogCallback(DebugConsoleTraceLogCallback);
}

void FlushPendingDebugConsoleTraceLog(GameState& state)
{
    std::vector<PendingDebugTraceLine> pending;

    {
        std::lock_guard<std::mutex> lock(gPendingDebugTraceMutex);
        pending.swap(gPendingDebugTraceLines);
    }

    for (const PendingDebugTraceLine& line : pending) {
        DebugConsoleAddLine(state, line.text, TraceLogLevelToColor(line.logLevel));
    }
}
