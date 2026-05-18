#pragma once

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <random>

namespace sysscope {

inline double now_seconds() {
    using clock = std::chrono::system_clock;
    return std::chrono::duration<double>(clock::now().time_since_epoch()).count();
}

inline double rand_range(double lo, double hi) {
    static thread_local std::mt19937 gen{std::random_device{}()};
    std::uniform_real_distribution<double> dist(lo, hi);
    return dist(gen);
}

}  // namespace sysscope
