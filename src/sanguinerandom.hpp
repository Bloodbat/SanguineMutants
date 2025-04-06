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

    // Adapted from Mutable Instruments' stmlib.
    class MutableRandom {
    public:
        inline uint32_t state() {
            return rng_state_;
        }

        inline void Seed(uint32_t seed) {
            rng_state_ = seed;
        }

        inline uint32_t GetWord() {
            rng_state_ = rng_state_ * 1664525L + 1013904223L;
            return state();
        }

        inline int16_t GetSample() {
            return static_cast<int16_t>(GetWord() >> 16);
        }

        inline float GetFloat() {
            return static_cast<float>(GetWord()) / 4294967296.0f;
        }

    private:
        uint32_t rng_state_;
    };
}