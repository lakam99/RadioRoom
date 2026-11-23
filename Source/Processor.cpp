#include "Processor.h"
#include "Editor.h"

juce::AudioProcessorValueTreeState::ParameterLayout RadioProcessor::layout(){
    juce::AudioProcessorValueTreeState::ParameterLayout result;
    for(int i=0;i<ControlCount;++i){juce::NormalisableRange<float> range(minimums[i],maximums[i],i==Era?1.f:.001f);if(i==Decay)range.setSkewForCentre(.55f);
        result.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ids[i],1},names[i],range,defaults[i]));}
    result.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"bypass",1},"Bypass",false));return result;
}
RadioProcessor::RadioProcessor():AudioProcessor(BusesProperties().withInput("Mix",juce::AudioChannelSet::stereo(),true).withOutput("Radio",juce::AudioChannelSet::stereo(),true)),state(*this,nullptr,"RadioRoom",layout()){
    for(int i=0;i<ControlCount;++i)params[(size_t)i]=state.getRawParameterValue(ids[i]);bypassParam=state.getRawParameterValue("bypass");
}
void RadioProcessor::prepareToPlay(double sr,int size){engine.prepare(sr,size);std::array<float,ControlCount> v{};for(int i=0;i<ControlCount;++i)v[(size_t)i]=params[(size_t)i]->load();engine.targets(v,bypassParam->load()>.5f,true);setLatencySamples(engine.latency());}
void RadioProcessor::process(juce::AudioBuffer<float>& b,bool hostBypass){
    juce::ScopedNoDenormals guard;std::array<float,ControlCount> v{};for(int i=0;i<ControlCount;++i)v[(size_t)i]=params[(size_t)i]->load(std::memory_order_relaxed);
    engine.targets(v,hostBypass||bypassParam->load()>.5f);engine.process(b);
    inputMeter.store(engine.inputPeak);outputMeter.store(engine.outputPeak);reductionMeter.store(engine.reduction);
}
void RadioProcessor::processBlock(juce::AudioBuffer<float>& b,juce::MidiBuffer& m){process(b,false);m.clear();}
void RadioProcessor::processBlockBypassed(juce::AudioBuffer<float>& b,juce::MidiBuffer& m){process(b,true);m.clear();}
juce::AudioProcessorEditor* RadioProcessor::createEditor(){return new RadioEditor(*this);}
void RadioProcessor::getStateInformation(juce::MemoryBlock& b){auto copy=state.copyState();copy.setProperty("skin",skin.load(),nullptr);copyXmlToBinary(*copy.createXml(),b);}
void RadioProcessor::setStateInformation(const void* d,int size){if(auto xml=getXmlFromBinary(d,size)){auto tree=juce::ValueTree::fromXml(*xml);if(tree.hasType("RadioRoom")){skin.store(juce::jlimit(0,2,(int)tree.getProperty("skin",0)));state.replaceState(tree);}}}
void RadioProcessor::setControl(int i,float v){auto* q=state.getParameter(ids[i]);q->beginChangeGesture();q->setValueNotifyingHost(q->convertTo0to1(v));q->endChangeGesture();}
std::array<float,ControlCount> RadioProcessor::presetValues(int n){
    std::array<float,ControlCount> v{};std::copy(std::begin(defaults),std::end(defaults),v.begin());
    if(n==0){v[Era]=1928;v[Bandwidth]=.16f;v[Tubes]=.39f;v[Speaker]=.65f;v[Body]=-.5f;v[Presence]=.5f;v[Glue]=.42f;v[Room]=.3f;v[Decay]=.42f;v[Distance]=.4f;v[Width]=0;v[Output]=-3;}
    if(n==2){v[Era]=1959;v[Bandwidth]=1;v[Tubes]=.10f;v[Speaker]=.07f;v[Body]=.5f;v[Presence]=0;v[Glue]=.24f;v[Room]=.35f;v[Decay]=.55f;v[Distance]=.32f;v[Width]=.75f;v[Output]=-1;}
    return v;
}
void RadioProcessor::applyPreset(int n){auto v=presetValues(n);for(int i=0;i<ControlCount;++i)setControl(i,v[(size_t)i]);}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new RadioProcessor();}
