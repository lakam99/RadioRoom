#pragma once
#include <array>
#include <cstdint>
#include <cstddef>

// Streaming marked-Poisson surface model. No audio loops, allocations or locks.
class VinylTexture {
public:
    void prepare(double sampleRate) noexcept;
    std::array<float,2> tick(float amount) noexcept;
private:
    struct Grain {float fast=0,slow=0,fastPole=0,slowPole=0,pan=0;};
    std::array<Grain,32> grains{};
    uint64_t randomState=0x9e3779b97f4a7c15ULL;
    double sr=48000,fineClock=0,popClock=0,driftClock=0;
    std::size_t nextGrain=0;
    float drift=1,driftTarget=1,driftPole=0,surfaceLow=0,surfaceHigh=0,lowPole=0,highPole=0;
    float uniform() noexcept;
    double interval(float rate) noexcept;
    void trigger(bool pop) noexcept;
};
