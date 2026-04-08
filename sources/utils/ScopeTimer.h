//
// Created by bison on 05-03-26.
//

#ifndef PHOMBIES_SCOPETIMER_H
#define PHOMBIES_SCOPETIMER_H

#include <chrono>
#include <cstdio>

struct ScopeTimer
{
    const char* name;
    std::chrono::steady_clock::time_point start;

    ScopeTimer(const char* n)
            : name(n), start(std::chrono::steady_clock::now()) {}

    ~ScopeTimer()
    {
        auto end = std::chrono::steady_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();
        printf("%s: %.3f ms\n", name, ms);
    }
};

#endif //PHOMBIES_SCOPETIMER_H
