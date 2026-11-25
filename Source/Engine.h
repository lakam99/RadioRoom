#pragma once
#include <juce_dsp/juce_dsp.h>
#include "VinylTexture.h"

enum Control { Era, Bandwidth, Tubes, Speaker, Body, Presence, Glue, Input,
               Room, Decay, Distance, Width, Mix, Output, Crackle, ControlCount };
inline constexpr const char* ids[]{"era","bandwidth","tubes","speaker","body","presence","glue","input","room","decay","distance","width","mix","output","crackle"};
inline constexpr const char* names[]{"Era","Bandwidth","Tubes","Speaker","Body","Presence","Glue","Input","Room","Decay","Distance","Width","Mix","Output","Crackle"};
inline constexpr float defaults[]{1952,.62f,.24f,.38f,1,-1,.28f,0,.22f,.48f,.25f,.25f,1,-2,0};
inline constexpr float minimums[]{1920,0,0,0,-6,-6,0,-18,0,.15f,0,0,0,-24,0};
inline constexpr float maximums[]{1959,1,1,1,6,6,1,18,1,1.8f,1,1,1,12,1};
inline constexpr const char* descriptions[]{
 "Morph between narrow 1920s radio and a fuller 1950s broadcast character.",
 "Open the radio passband. Low settings trim bass and treble; high settings retain more of the mix.",
 "Level-compensated, 4x oversampled tube saturation. Rounds peaks rather than simply adding volume.",
 "Blend in the resonant midrange and cabinet roll-off of a small radio speaker.",
 "Broad low-mid balance, in dB. Add weight or reduce boxiness across the entire mix.",
 "Broad upper-mid balance, in dB. Bring voices forward or soften an aggressive mix.",
 "Stereo-linked soft-knee mix compression. The shared detector keeps the stereo image stable.",
 "Input gain before the room, compressor and tubes, in dB.",
 "Feed the whole mix into one shared acoustic space with early reflections and a dense room tail.",
 "Room decay time in seconds. Changes feedback smoothly without moving delay taps.",
 "Move the ensemble away from the common microphone: more reflections and softer detail.",
 "Stereo width. Zero is mono; 100 percent preserves the input's original width.",
 "Latency-aligned dry / processed blend. Zero passes the delayed original exactly.",
 "Final processed output trim, in dB. Dry and bypass remain at original level.",
 "Procedural vinyl surface wear: fine irregular ticks, occasional dusty pops and a soft surface floor. Zero is off. Automatable; follows playback and the radio's tone."};

struct Filter {
    float z1=0,z2=0;
    double b0=1,b1=0,b2=0,a1=0,a2=0;
    float tick(float x) noexcept {float y=(float)(b0*x+z1);z1=(float)(b1*x-a1*y+z2);z2=(float)(b2*x-a2*y);return y;}
    void clear() noexcept {z1=z2=0;}
    void design(double sr,double hz,double q,double db,int kind) noexcept;
};

class RadioEngine {
public:
    void prepare(double sampleRate,int maxBlock);
    void targets(const std::array<float,ControlCount>&,bool bypass,bool immediate=false) noexcept;
    void process(juce::AudioBuffer<float>&,int transport=-1) noexcept;
    int latency() const noexcept{return latencySamples;}
    float inputPeak=0,outputPeak=0,reduction=0;
private:
    double sr=48000;
    static constexpr int quantum=32;
    int latencySamples=0,dryWrite=0,earlyWrite=0,noiseHold=0;
    uint32_t coefficientCounter=0;
    VinylTexture vinyl;
    float noiseGate=0,noiseAttack=0,noiseRelease=0;
    std::array<juce::SmoothedValue<float>,ControlCount> smooth;
    juce::SmoothedValue<float> wetEnable;
    juce::dsp::Oversampling<float> oversampling{2,2,juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple,true,true};
    juce::AudioBuffer<float> wet;
    std::array<std::vector<float>,2> dry;
    std::vector<float> early;
    struct Line {std::vector<float> data;int pos=0;float damp=0;};
    std::array<Line,8> lines;
    std::array<std::array<Filter,7>,2> filters;
    std::array<Filter,2> dc;
    std::array<float,quantum> drive{},tubeAmount{},blend{},gain{},active{},dryL{},dryR{};
    std::array<float,ControlCount> current{};
    float detector=0,compressGain=1,attack=0,release=0,detectorLow=0,dampCoefficient=0,detectorCoefficient=0,gainPole=0;
    std::array<float,8> feedback{};
    void coefficients() noexcept;
    void room(float&,float&) noexcept;
};
