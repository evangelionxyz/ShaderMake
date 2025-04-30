/*
Copyright (c) 2014-2025, NVIDIA CORPORATION. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#pragma once

#ifdef _WIN32
#   include <windows.h>
#else
#   include <unistd.h>
#   include <limits.h>
#endif

#include <stdint.h>

namespace ShaderMake {

    class Timer
    {
    public:
        double ticksToMilliseconds = 0.0;

        Timer()
        {
#ifdef _WIN32
            uint64_t ticksPerSecond = 1;
            QueryPerformanceFrequency((LARGE_INTEGER *)&ticksPerSecond);

            ticksToMilliseconds = 1000.0 / ticksPerSecond;
#else
            ticksToMilliseconds = 1.0 / 1000000.0;
#endif
        }

        double ConvertTicksToMilliseconds(uint64_t ticks)
        {
            return (double)ticks * ticksToMilliseconds;
        }

        uint64_t GetTicks()
        {
#ifdef _WIN32
            uint64_t ticks;
            QueryPerformanceCounter((LARGE_INTEGER *)&ticks);

            return ticks;
#else
            struct timespec spec;
            clock_gettime(CLOCK_REALTIME, &spec);

            return uint64_t(spec.tv_sec) * 1000000000ull + spec.tv_nsec;
#endif
        };

    };
}
