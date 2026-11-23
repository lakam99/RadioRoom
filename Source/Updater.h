#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// Owned only by the standalone executable. No update thread/code runs in the VST.
class RadioUpdater final:private juce::Timer {
public:
    ~RadioUpdater() override;
    bool request(bool prepare,std::function<void(juce::var)>);
    bool busy() const noexcept{return child!=nullptr;}
    double progress=-1;int phase=0;
    juce::String stage() const;
    static bool installAndRestart(const juce::var&,juce::String& error);
private:
    std::unique_ptr<juce::ChildProcess> child;
    juce::File report,progressFile;
    std::function<void(juce::var)> callback;
    double deadline=0;
    void timerCallback() override;
    void clearFiles();
};
