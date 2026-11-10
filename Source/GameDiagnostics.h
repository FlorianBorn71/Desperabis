#pragma once

#include <cstdio>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>

namespace Desperabis::Diagnostics
{
    using Sink = void (*)(const char*);

    inline void DefaultSink(const char* message) { std::fputs(message, stderr); }

    struct State
    {
        std::mutex mutex;
        std::set<std::string> reported;
        Sink sink = DefaultSink;
    };

    inline State& GetState()
    {
        static State state;
        return state;
    }

    // Configure before starting game/renderer updates. Sinks must not throw.
    inline void SetSink(Sink sink)
    {
        if (!sink) { throw std::invalid_argument("Game diagnostics require a nonnull sink."); }
        auto& state = GetState();
        std::lock_guard<std::mutex> lock(state.mutex);
        state.sink = sink;
    }

    inline bool ReportOnce(std::string_view context, std::string_view message)
    {
        constexpr std::size_t limit = 256;
        const std::string text = std::string(context.substr(0, 512)) + ": " + std::string(message.substr(0, 1024));
        auto& state = GetState();
        Sink sink;
        bool reachedLimit;
        {
            std::lock_guard<std::mutex> lock(state.mutex);
            if (state.reported.size() >= limit || !state.reported.insert(text).second) { return false; }
            sink = state.sink;
            reachedLimit = state.reported.size() == limit;
        }
        const auto line = "Desperabis: " + text + "\n";
        sink(line.c_str());
        if (reachedLimit) { sink("Desperabis: Diagnostic limit reached; further messages suppressed.\n"); }
        return true;
    }

    template<typename Exception>
    inline void Recover(const Exception& error, const char* file, int line)
    {
#ifndef NDEBUG
        (void)file;
        (void)line;
        throw error;
#else
        ReportOnce(std::string(file) + ":" + std::to_string(line), error.what());
#endif
    }
}

// Only use where the caller has an explicit, safe recovery path.
#define GAME_ERROR(error) ::Desperabis::Diagnostics::Recover((error), __FILE__, __LINE__)
#define GAME_ERROR_AND_RETURN(error) do { GAME_ERROR(error); return; } while (false)
#define GAME_ERROR_AND_RETURN_VALUE(error, ...) do { GAME_ERROR(error); return __VA_ARGS__; } while (false)
