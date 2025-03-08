#pragma once

#include "rack.hpp"

namespace sanguineRandom {
    static const float kDoublePi = 2.f * M_PI;

    // Based on Rack's random.hpp, tweaked to avoid a multiplication.
    /** Returns a normal random number with mean 0 and standard deviation 1 */
    inline float normal() {
        // Box-Muller transform
        float radius = std::sqrt(-2.f * std::log(1.f - rack::random::get<float>()));
        float theta = kDoublePi * rack::random::get<float>();
        return radius * std::sin(theta);
    }
}