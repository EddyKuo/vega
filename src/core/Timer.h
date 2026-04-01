#pragma once
#include <windows.h>
#include <cstdint>

namespace vega
{

/// High-resolution timer using QueryPerformanceCounter.
class Timer
{
public:
    Timer()
    {
        QueryPerformanceFrequency(&frequency_);
        reset();
    }

    void reset()
    {
        QueryPerformanceCounter(&start_);
    }

    /// Elapsed time in seconds.
    double elapsed() const
    {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        return static_cast<double>(now.QuadPart - start_.QuadPart)
             / static_cast<double>(frequency_.QuadPart);
    }

    /// Elapsed time in milliseconds.
    double elapsed_ms() const { return elapsed() * 1000.0; }

    /// Elapsed time in microseconds.
    double elapsed_us() const { return elapsed() * 1000000.0; }

private:
    LARGE_INTEGER frequency_;
    LARGE_INTEGER start_;
};

/// RAII scope timer — logs elapsed time on destruction.
class ScopeTimer
{
public:
    explicit ScopeTimer(const char* name) : name_(name) {}

    ~ScopeTimer();

private:
    const char* name_;
    Timer timer_;
};

} // namespace vega
