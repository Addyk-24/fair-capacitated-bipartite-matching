#pragma once

#include <chrono>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace daa {

// Wall-clock stopwatch with sub-millisecond resolution.
//
// On Windows it uses QueryPerformanceCounter directly: older MinGW runtimes
// implement std::chrono::steady_clock with a ~15 ms tick, which makes small
// benchmarks read 0 ms or jump in 15 ms steps. Elsewhere steady_clock is fine.
class Stopwatch {
public:
    Stopwatch() { restart(); }

    void restart() {
#ifdef _WIN32
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        start_ = now.QuadPart;
#else
        start_ = std::chrono::steady_clock::now();
#endif
    }

    double elapsedMilliseconds() const {
#ifdef _WIN32
        LARGE_INTEGER now, freq;
        QueryPerformanceCounter(&now);
        QueryPerformanceFrequency(&freq);
        return 1000.0 * static_cast<double>(now.QuadPart - start_) / static_cast<double>(freq.QuadPart);
#else
        return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start_)
            .count();
#endif
    }

private:
#ifdef _WIN32
    long long start_;
#else
    std::chrono::steady_clock::time_point start_;
#endif
};

}  // namespace daa
