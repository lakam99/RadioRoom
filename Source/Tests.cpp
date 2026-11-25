#include "Tests.h"
#include "Editor.h"
#include <cmath>
#if JUCE_WINDOWS
#define NOMINMAX
#include <windows.h>
#endif

namespace {
struct Transport final:juce::AudioPlayHead {
    bool running=true;
    juce::Optional<PositionInfo> getPosition() const override {PositionInfo info;info.setIsPlaying(running);return info;}
};
void set(juce::AudioProcessor& p,const char* name,float value){for(auto* q:p.getParameters())if(q->getName(128)==name){auto* r=dynamic_cast<juce::RangedAudioParameter*>(q);q->setValueNotifyingHost(r?r->convertTo0to1(value):value);return;}throw std::runtime_error("Missing parameter");}
void signal(juce::AudioBuffer<float>& b,int64_t start,double sr){for(int i=0;i<b.getNumSamples();++i){float t=(float)((start+i)/sr);b.setSample(0,i,.22f*std::sin(t*juce::MathConstants<float>::twoPi*331)+.08f*std::sin(t*juce::MathConstants<float>::twoPi*7001));if(b.getNumChannels()>1)b.setSample(1,i,.17f*std::sin(t*juce::MathConstants<float>::twoPi*559));}}
std::vector<float> texture(double sr,float amount,int block,int transport,float mix=1,bool bypass=false,float width=1){
    RadioEngine engine;engine.prepare(sr,block);std::array<float,ControlCount> controls{};std::copy(std::begin(defaults),std::end(defaults),controls.begin());controls[Crackle]=amount;controls[Mix]=mix;controls[Width]=width;controls[Room]=0;engine.targets(controls,bypass,true);
    int count=(int)(sr*4);std::vector<float> result((size_t)count*2);juce::AudioBuffer<float> b(2,block);juce::ScopedNoDenormals guard;
    for(int at=0;at<count;at+=block){int n=std::min(block,count-at);b.setSize(2,n,false,false,true);b.clear();engine.process(b,transport);for(int j=0;j<n;++j){result[(size_t)(at+j)*2]=b.getSample(0,j);result[(size_t)(at+j)*2+1]=b.getSample(1,j);}}return result;
}
double energy(const std::vector<float>& data){double sum=0;for(float v:data)sum+=v*v;return sum/data.size();}
bool crackleTests(juce::DynamicObject& report){
    bool off=true,gated=true,scales=true,streaming=true,mono=true,recall=true,order=true;float peak=0,maxError=0;
    for(double sr:{44100.,48000.,96000.}){
        auto low=texture(sr,.3f,127,1),high=texture(sr,.8f,127,1),differentBlock=texture(sr,.8f,17,1),centre=texture(sr,.8f,127,1,1,false,0);
        off&=energy(texture(sr,0,127,1))==0;gated&=energy(texture(sr,.8f,127,0))==0&&energy(texture(sr,.8f,127,-1))==0;
        off&=energy(texture(sr,.8f,127,1,0))==0&&energy(texture(sr,.8f,127,1,1,true))==0;
        scales&=energy(low)>1e-9&&energy(high)>energy(low)*3;
        for(size_t j=0;j<high.size();++j){peak=std::max(peak,std::abs(high[j]));maxError=std::max(maxError,std::abs(high[j]-differentBlock[j]));streaming&=std::isfinite(high[j]);}
        for(size_t j=0;j<centre.size();j+=2)mono&=std::abs(centre[j]-centre[j+1])<1e-7;
    }
    streaming&=maxError<2e-6&&peak<.5f;
    RadioProcessor p;auto parameters=p.getParameters();order&=parameters.size()==16;
    for(int i=0;i<16&&i<parameters.size();++i){auto* param=dynamic_cast<juce::AudioProcessorParameterWithID*>(parameters[i]);order&=param&&param->paramID==(i<Crackle?ids[i]:i==Crackle?"bypass":"crackle");}
    p.setControl(Crackle,.531f);juce::MemoryBlock state;p.getStateInformation(state);RadioProcessor restored;restored.setStateInformation(state.getData(),(int)state.getSize());recall&=std::abs(restored.state.getRawParameterValue("crackle")->load()-.531f)<.001;
    auto old=p.state.copyState();auto child=old.getChildWithProperty("id","crackle");old.removeChild(child,nullptr);juce::MemoryBlock legacy;juce::AudioProcessor::copyXmlToBinary(*old.createXml(),legacy);restored.setStateInformation(legacy.getData(),(int)legacy.getSize());recall&=restored.state.getRawParameterValue("crackle")->load()==0;
    // A playing host can sustain a vinyl bed through digital silence; stopping fades it away.
    RadioEngine tail;tail.prepare(48000,128);std::array<float,ControlCount> v{};std::copy(std::begin(defaults),std::end(defaults),v.begin());v[Crackle]=1;tail.targets(v,false,true);juce::AudioBuffer<float> b(2,128);double last=0;
    for(int k=0;k<1500;++k){b.clear();tail.process(b,k<200?1:0);if(k>1400)last+=b.getMagnitude(0,128);}gated&=last<1e-5;
    report.setProperty("crackle_off_dry_and_bypass_are_silent",off);report.setProperty("crackle_transport_and_idle_gating",gated);report.setProperty("crackle_amount_scales_energy",scales);report.setProperty("crackle_streaming_block_invariance",streaming);report.setProperty("crackle_max_block_difference",maxError);report.setProperty("crackle_peak_80_percent",peak);report.setProperty("crackle_mono_compatibility",mono);report.setProperty("crackle_state_and_legacy_zero_default",recall);report.setProperty("legacy_parameter_order_preserved",order);
    return off&&gated&&scales&&streaming&&mono&&recall&&order;
}
}
juce::var runTests(){
    auto* report=new juce::DynamicObject;bool finite=true,silent=true,dry=true,bypass=true,mono=true,recall=true,linked=true,roomTail=true;double totalMs=0,audioMs=0,worstMs=0;float maxPeak=0,dryError=0;
    for(double sr:{44100.,48000.,96000.}){
        RadioProcessor p;p.prepareToPlay(sr,127);juce::AudioBuffer<float> b(2,127);juce::MidiBuffer m;
        for(int k=0;k<20;++k){b.clear();p.processBlock(b,m);silent&=b.getMagnitude(0,127)==0;}
        int64_t at=0;
        for(int k=0;k<1600;++k){int n=std::array<int,5>{1,17,127,512,1025}[(size_t)(k%5)];b.setSize(2,n);signal(b,at,sr);
            if(k%53==0)for(int i=0;i<ControlCount;++i)p.setControl(i,minimums[i]+(maximums[i]-minimums[i])*((k/53+i)%7)/6.f);
            auto begin=juce::Time::getMillisecondCounterHiRes();p.processBlock(b,m);double ms=juce::Time::getMillisecondCounterHiRes()-begin;totalMs+=ms;audioMs+=n/sr*1000;worstMs=std::max(worstMs,ms);
            for(int c=0;c<2;++c)for(int i=0;i<n;++i){float v=b.getSample(c,i);finite&=std::isfinite(v);maxPeak=std::max(maxPeak,std::abs(v));}at+=n;
        }
        RadioProcessor d;d.setControl(Mix,0);d.prepareToPlay(sr,127);int latency=d.getLatencySamples();std::vector<float> input(5000);for(int i=0;i<5000;++i)input[(size_t)i]=.13f*std::sin(i*.51f);
        int offset=0;while(offset<5000){int n=std::min(127,5000-offset);b.setSize(2,n);for(int c=0;c<2;++c)for(int j=0;j<n;++j)b.setSample(c,j,input[(size_t)(offset+j)]);d.processBlock(b,m);
            for(int j=0;j<n;++j){float expected=offset+j>=latency?input[(size_t)(offset+j-latency)]:0;dryError=std::max(dryError,std::abs(expected-b.getSample(0,j)));}offset+=n;}dry&=dryError<1e-7f;
        RadioProcessor bp;set(bp,"Bypass",1);bp.prepareToPlay(sr,127);offset=0;while(offset<5000){int n=std::min(127,5000-offset);b.setSize(2,n);for(int c=0;c<2;++c)for(int j=0;j<n;++j)b.setSample(c,j,input[(size_t)(offset+j)]);bp.processBlock(b,m);for(int j=0;j<n;++j){float expected=offset+j>=latency?input[(size_t)(offset+j-latency)]:0;bypass&=std::abs(expected-b.getSample(0,j))<1e-7f;}offset+=n;}
        RadioProcessor mp;auto layout=mp.getBusesLayout();layout.inputBuses.set(0,juce::AudioChannelSet::mono());layout.outputBuses.set(0,juce::AudioChannelSet::mono());mono&=mp.setBusesLayout(layout);mp.prepareToPlay(sr,64);b.setSize(1,64);signal(b,0,sr);mp.processBlock(b,m);mono&=std::isfinite(b.getMagnitude(0,64));
        p.setControl(Room,.417f);p.setControl(Decay,.823f);p.skin.store(2);juce::MemoryBlock saved;p.getStateInformation(saved);RadioProcessor restored;restored.setStateInformation(saved.getData(),(int)saved.getSize());recall&=std::abs(restored.state.getRawParameterValue("room")->load()-.417f)<.001&&std::abs(restored.state.getRawParameterValue("decay")->load()-.823f)<.001&&restored.skin.load()==2;
        RadioProcessor equal;equal.setControl(Room,0);equal.prepareToPlay(sr,128);b.setSize(2,128);for(int k=0;k<30;++k){signal(b,k*128,sr);b.copyFrom(1,0,b,0,0,128);equal.processBlock(b,m);if(k>25)for(int j=0;j<128;++j)linked&=std::abs(b.getSample(0,j)-b.getSample(1,j))<1e-6f;}
        RadioProcessor room;room.setControl(Room,.7f);room.setControl(Decay,.5f);room.prepareToPlay(sr,128);b.setSize(2,128);double tail=0,final=0;
        for(int k=0;k<(int)(sr*3/128);++k){b.clear();if(k==0){b.setSample(0,0,.5f);b.setSample(1,0,.5f);}room.processBlock(b,m);if(k>sr*.1/128&&k<sr*.5/128)tail+=b.getRMSLevel(0,0,128);if(k>sr*2.8/128)final+=b.getRMSLevel(0,0,128);}roomTail&=tail>1e-5&&final<1e-5;
    }
    RadioProcessor ui;std::unique_ptr<juce::AudioProcessorEditor> ed(ui.createEditor());int sliders=0;bool layout=true;for(auto* c:ed->getChildren())if(auto* s=dynamic_cast<juce::Slider*>(c)){++sliders;layout&=ed->getLocalBounds().contains(s->getBounds());s->setValue(s->getMaximum(),juce::sendNotificationSync);}
    layout&=sliders==ControlCount;for(int i=0;i<ControlCount;++i)layout&=std::abs(ui.state.getRawParameterValue(ids[i])->load()-maximums[i])<.01f;ed.reset();
    bool crackle=crackleTests(*report);
    bool passed=finite&&silent&&dry&&bypass&&mono&&recall&&linked&&roomTail&&layout&&crackle;
    report->setProperty("passed",passed);report->setProperty("rates","44100,48000,96000");report->setProperty("block_sizes","1,17,127,512,1025");report->setProperty("finite_under_extreme_automation",finite);report->setProperty("silence_is_silent",silent);report->setProperty("latency_aligned_dry",dry);report->setProperty("dry_max_error",dryError);report->setProperty("latency_aligned_bypass",bypass);report->setProperty("mono_layout",mono);report->setProperty("parameter_and_skin_recall",recall);report->setProperty("linked_compression_channel_symmetry",linked);report->setProperty("room_tail_present_and_decays",roomTail);report->setProperty("all_ui_controls_bound",layout);report->setProperty("stress_peak_extreme_gains",maxPeak);report->setProperty("average_one_core_percent",100*totalMs/audioMs);report->setProperty("worst_callback_ms",worstMs);return juce::var(report);
}
juce::var hostedTests(const juce::File& file){
    juce::VST3PluginFormat format;juce::OwnedArray<juce::PluginDescription> descriptions;format.findAllTypesForFile(descriptions,file.getFullPathName());if(descriptions.isEmpty())throw std::runtime_error("VST3 not discovered");
    juce::String error;bool recall=true,finite=true,transportWorks=true;double maxRemove=0;int latency=0;
    for(int cycle=0;cycle<3;++cycle){std::vector<std::unique_ptr<juce::AudioPluginInstance>> instances;
        for(int i=0;i<8;++i){auto p=format.createInstanceFromDescription(*descriptions[0],48000,127,error);if(!p)throw std::runtime_error(error.toStdString());p->prepareToPlay(48000,127);latency=p->getLatencySamples();
            juce::AudioBuffer<float> b(2,127);juce::MidiBuffer m;for(int k=0;k<15;++k){signal(b,k*127,48000);p->processBlock(b,m);finite&=std::isfinite(b.getMagnitude(0,127));}
            if(cycle==0&&i==0){Transport clock;p->setPlayHead(&clock);set(*p,"Crackle",.8f);double bed=0,stopped=0;
                for(int k=0;k<250;++k){b.clear();p->processBlock(b,m);if(k>100)bed+=b.getRMSLevel(0,0,127);}
                clock.running=false;for(int k=0;k<1300;++k){b.clear();p->processBlock(b,m);if(k>1250)stopped+=b.getMagnitude(0,127);}transportWorks&=bed>1e-4&&stopped<1e-5;p->setPlayHead(nullptr);}
            set(*p,"Room",.413f);set(*p,"Crackle",.427f);juce::MemoryBlock state;p->getStateInformation(state);set(*p,"Room",.1f);set(*p,"Crackle",0);p->setStateInformation(state.getData(),(int)state.getSize());p->processBlock(b,m);bool crackleFound=false;for(auto* q:p->getParameters()){if(q->getName(128)=="Room")recall&=std::abs(q->getValue()-.413f)<.002f;if(q->getName(128)=="Crackle"){crackleFound=true;recall&=std::abs(q->getValue()-.427f)<.002f;}}recall&=crackleFound;
            if(i<3){std::unique_ptr<juce::AudioProcessorEditor> ed(p->createEditorIfNeeded());juce::DocumentWindow w("RadioRoom lifecycle",juce::Colours::black,0);w.setUsingNativeTitleBar(true);w.setContentNonOwned(ed.get(),true);w.setTopLeftPosition(-4000,-4000);w.setVisible(true);juce::MessageManager::getInstance()->runDispatchLoopUntil(70);w.setVisible(false);w.clearContentComponent();ed.reset();juce::MessageManager::getInstance()->runDispatchLoopUntil(30);}
            instances.push_back(std::move(p));}
        auto begin=juce::Time::getMillisecondCounterHiRes();for(auto& p:instances){p->releaseResources();p.reset();}instances.clear();juce::MessageManager::getInstance()->runDispatchLoopUntil(60);maxRemove=std::max(maxRemove,juce::Time::getMillisecondCounterHiRes()-begin);
    }
    bool unloaded=true;
#if JUCE_WINDOWS
    auto binary=file.getChildFile("Contents/x86_64-win/RadioRoom.vst3");unloaded=GetModuleHandleW(binary.getFullPathName().toWideCharPointer())==nullptr;
#endif
    auto* r=new juce::DynamicObject;r->setProperty("passed",recall&&finite&&unloaded&&maxRemove<1500&&transportWorks);r->setProperty("actual_vst3_loaded",true);r->setProperty("crackle_follows_actual_host_transport",transportWorks);r->setProperty("state_recall",recall);r->setProperty("finite_audio",finite);r->setProperty("instances",24);r->setProperty("native_editor_open_close_cycles",9);r->setProperty("all_instances_removed_and_module_unloaded",unloaded);r->setProperty("max_batch_removal_ms",maxRemove);r->setProperty("reported_latency_samples",latency);return juce::var(r);
}
void processFile(const juce::File& input,const juce::File& output,int preset,float crackle){
    juce::AudioFormatManager fm;fm.registerBasicFormats();std::unique_ptr<juce::AudioFormatReader> reader(fm.createReaderFor(input));if(!reader)throw std::runtime_error("Cannot read input file");
    if(input==output)throw std::runtime_error("Input and output must differ");output.getParentDirectory().createDirectory();auto fs=output.createOutputStream();if(!fs||!fs->setPosition(0)||fs->truncate().failed())throw std::runtime_error("Cannot write output");std::unique_ptr<juce::OutputStream> stream(std::move(fs));juce::WavAudioFormat wav;auto writer=wav.createWriterFor(stream,juce::AudioFormatWriterOptions{}.withSampleRate(reader->sampleRate).withNumChannels(2).withBitsPerSample(24));if(!writer)throw std::runtime_error("Cannot create WAV writer");
    RadioProcessor p;p.applyPreset(preset);p.setControl(Crackle,juce::jlimit(0.f,1.f,crackle));p.prepareToPlay(reader->sampleRate,512);juce::AudioBuffer<float> b(2,512);juce::MidiBuffer midi;
    // Flush latency and room tail, writing the compensated result with its decay.
    int latency=p.getLatencySamples();int64_t total=reader->lengthInSamples+(int64_t)(reader->sampleRate*2.5)+latency;
    for(int64_t at=0;at<total;at+=512){int n=(int)std::min<int64_t>(512,total-at);b.setSize(2,n,false,false,true);b.clear();if(at<reader->lengthInSamples)reader->read(&b,0,(int)std::min<int64_t>(n,reader->lengthInSamples-at),at,true,true);p.processBlock(b,midi);int skip=(int)std::clamp<int64_t>(latency-at,0,n);if(n>skip&&!writer->writeFromAudioSampleBuffer(b,skip,n-skip))throw std::runtime_error("WAV write failed");}p.releaseResources();
}
