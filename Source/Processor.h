#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "Engine.h"

class RadioProcessor final:public juce::AudioProcessor {
public:
    RadioProcessor();
    const juce::String getName() const override{return "RadioRoom";}
    void prepareToPlay(double,int) override;
    void releaseResources() override{}
    void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&,juce::MidiBuffer&) override;
    bool isBusesLayoutSupported(const BusesLayout& b) const override {
        auto out=b.getMainOutputChannelSet();return out==b.getMainInputChannelSet()&&(out==juce::AudioChannelSet::mono()||out==juce::AudioChannelSet::stereo());}
    bool acceptsMidi() const override{return false;}
    bool producesMidi() const override{return false;}
    double getTailLengthSeconds() const override{return 2.5;}
    bool hasEditor() const override{return true;}
    juce::AudioProcessorEditor* createEditor() override;
    juce::AudioProcessorParameter* getBypassParameter() const override{return state.getParameter("bypass");}
    int getNumPrograms() override{return 1;}
    int getCurrentProgram() override{return 0;}
    void setCurrentProgram(int) override{}
    const juce::String getProgramName(int) override{return "RadioRoom";}
    void changeProgramName(int,const juce::String&) override{}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*,int) override;
    void applyPreset(int);
    static std::array<float,ControlCount> presetValues(int);
    void setControl(int,float);
    juce::AudioProcessorValueTreeState state;
    std::atomic<int> skin{0};
    std::atomic<bool> standalone{false},playing{false};
    std::atomic<float> inputMeter{0},outputMeter{0},reductionMeter{0};
private:
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    std::array<std::atomic<float>*,ControlCount> params{};
    std::atomic<float>* bypassParam=nullptr;
    RadioEngine engine;
    void process(juce::AudioBuffer<float>&,bool);
};
