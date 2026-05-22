#pragma once

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace netlib_benchmark {

using clock = std::chrono::steady_clock;

inline std::size_t parse_iterations(int argc, char** argv, std::size_t default_iters = 1'000) {
    if (argc > 1) {
        int const n = std::atoi(argv[1]);
        if (n > 0) {
            return static_cast<std::size_t>(n);
        }
    }
    return default_iters;
}

inline double elapsed_ms(clock::time_point const& start, clock::time_point const& end) {
    return std::chrono::duration<double, std::milli>(end - start).count();
}

inline void print_throughput(char const* name, std::size_t ops, double ms) {
    double const ops_per_s = ms > 0.0 ? (static_cast<double>(ops) * 1000.0) / ms : 0.0;
    std::printf("%s: %zu ops, %.2f ms, %.0f ops/s\n", name, ops, ms, ops_per_s);
}

}  // namespace netlib_benchmark
