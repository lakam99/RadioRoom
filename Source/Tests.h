#pragma once
#include "Processor.h"
juce::var runTests();
juce::var hostedTests(const juce::File&);
void processFile(const juce::File&,const juce::File&,int,float crackle=0);
