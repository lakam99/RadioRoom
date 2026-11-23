#pragma once
#include "Processor.h"
#include "VfdDisplay.h"

class RadioLook final:public juce::LookAndFeel_V4 {
public:
    int skin=0;
    juce::Colour ink() const{return skin==1?juce::Colour(0xffece0c9):juce::Colour(0xff332c23);}
    void drawRotarySlider(juce::Graphics&,int,int,int,int,float,float,float,juce::Slider&) override;
    void drawButtonBackground(juce::Graphics&,juce::Button&,const juce::Colour&,bool,bool) override;
    void drawButtonText(juce::Graphics&,juce::TextButton&,bool,bool) override;
};
class RadioEditor final:public juce::AudioProcessorEditor,private juce::Timer {
public:
    explicit RadioEditor(RadioProcessor&);
    ~RadioEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void parentHierarchyChanged() override{if(auto* peer=getPeer())peer->setCurrentRenderingEngine(0);}
    void visibilityChanged() override{if(auto* peer=getPeer())peer->setCurrentRenderingEngine(0);}
    std::function<void()> onOpen,onPlay,onStop,onRewind,onLoop,onDevices;
    std::function<juce::String()> fileStatus;
    std::function<bool()> isLooping;
    std::function<bool()> isLoaded;
    std::function<void()> onUpdates;
    std::function<std::pair<double,juce::String>()> updateProgress;
    void setUpdateBusy(bool busy){updateBusy=busy;updates.setEnabled(!busy);repaint();}
private:
    RadioProcessor& p;RadioLook look;juce::TooltipWindow tooltips{this,500};
    std::array<juce::Slider,ControlCount> knobs;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>,ControlCount> attachments;
    std::array<VfdLine,ControlCount> values;
    std::array<juce::Rectangle<int>,ControlCount> cells;
    VfdLine status,details,finishLine,sourceLine;
    juce::TextButton preset{"PRESET"},skinButton{"FINISH"},bypass{"BYPASS"},open{"OPEN MIX"},play,stop,rewind,loop,device{"AUDIO"},updates{"UPDATES"};
    bool updateBusy=false;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;
    float input=0,output=0,reduction=0;
    void timerCallback() override;
    void meter(juce::Graphics&,juce::Rectangle<float>,float,bool,const juce::String&);
};
