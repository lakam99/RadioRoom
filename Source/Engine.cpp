#include "Engine.h"
#include <cmath>

void Filter::design(double rate,double hz,double q,double db,int kind) noexcept {
    hz=std::clamp(hz,15.,rate*.44);double w=juce::MathConstants<double>::twoPi*hz/rate,c=std::cos(w),s=std::sin(w),alpha=s/(2*q),a0=1;
    if(kind==0){b0=(1-c)*.5;b1=1-c;b2=b0;a0=1+alpha;a1=-2*c;a2=1-alpha;}
    else if(kind==1){b0=(1+c)*.5;b1=-(1+c);b2=b0;a0=1+alpha;a1=-2*c;a2=1-alpha;}
    else {double A=std::pow(10.,db/40);b0=1+alpha*A;b1=-2*c;b2=1-alpha*A;a0=1+alpha/A;a1=-2*c;a2=1-alpha/A;}
    b0/=a0;b1/=a0;b2/=a0;a1/=a0;a2/=a0;
}
void RadioEngine::prepare(double rate,int) {
    sr=std::clamp(rate,8000.,384000.);wet.setSize(2,quantum);oversampling.initProcessing(quantum);oversampling.reset();
    latencySamples=(int)std::lround(oversampling.getLatencyInSamples());
    for(auto& d:dry)d.assign((size_t)latencySamples+1,0);dryWrite=0;
    early.assign((size_t)(sr*.09)+1,0);earlyWrite=0;
    constexpr double seconds[]{.0297,.0371,.0419,.0473,.0539,.0611,.0677,.0739};
    for(int i=0;i<8;++i){auto& l=lines[(size_t)i];l.data.assign((size_t)(seconds[i]*sr)|1,0);l.pos=0;l.damp=0;}
    for(auto& ch:filters)for(auto& f:ch)f.clear();for(auto& f:dc){f.clear();f.design(sr,18,.707,0,1);}
    for(int i=0;i<ControlCount;++i){smooth[(size_t)i].reset(sr,.06);smooth[(size_t)i].setCurrentAndTargetValue(defaults[i]);current[(size_t)i]=defaults[i];}
    wetEnable.reset(sr,.015);wetEnable.setCurrentAndTargetValue(1);
    detector=detectorLow=0;compressGain=1;coefficientCounter=0;
    attack=(float)std::exp(-1/(sr*.016));release=(float)std::exp(-1/(sr*.22));gainPole=(float)std::exp(-1/(sr*.004));detectorCoefficient=(float)(1-std::exp(-2*juce::MathConstants<double>::pi*100/sr));coefficients();
}
void RadioEngine::targets(const std::array<float,ControlCount>& values,bool bypass,bool immediate) noexcept {
    for(int i=0;i<ControlCount;++i){float v=std::isfinite(values[(size_t)i])?std::clamp(values[(size_t)i],minimums[i],maximums[i]):defaults[i];
        if(immediate)smooth[(size_t)i].setCurrentAndTargetValue(v);else smooth[(size_t)i].setTargetValue(v);}
    if(immediate)wetEnable.setCurrentAndTargetValue(bypass?0.f:1.f);else wetEnable.setTargetValue(bypass?0.f:1.f);
}
void RadioEngine::coefficients() noexcept {
    float era=(current[Era]-1920)/39,band=current[Bandwidth],speaker=current[Speaker];
    float hp=std::exp(std::log(65.f)+(1-band)*(std::log(380.f-150*era)-std::log(65.f)));
    float lp=std::exp(std::log(1800.f+1800*era)+band*(std::log(9500.f+5500*era)-std::log(1800.f+1800*era)));
    lp*=1-.30f*current[Distance]*current[Room];
    for(auto& f:filters){f[0].design(sr,hp,.707,0,1);f[1].design(sr,lp,.707,0,0);f[2].design(sr,lp,.707,0,0);
        f[3].design(sr,240,.65,current[Body],2);f[4].design(sr,2300,.7,current[Presence],2);
        f[5].design(sr,740+era*200,.9,speaker*(4.0f-1.5f*era),2);f[6].design(sr,3200,1.1,-speaker*3.f,2);}
    dampCoefficient=(float)(1-std::exp(-juce::MathConstants<double>::twoPi*(2900-1800*current[Distance])/sr));
    for(size_t i=0;i<8;++i)feedback[i]=(float)std::pow(10.,-3.*lines[i].data.size()/(sr*current[Decay]));
}
void RadioEngine::room(float& l,float& r) noexcept {
    float mono=(l+r)*.5f;early[(size_t)earlyWrite]=mono;
    auto tap=[&](double secs){int at=earlyWrite-(int)(secs*sr);if(at<0)at+=(int)early.size();return early[(size_t)at];};
    float earlyL=tap(.0097)*.42f+tap(.0223)*.31f-tap(.0471)*.18f;
    float earlyR=tap(.0149)*.42f+tap(.0311)*.31f-tap(.0533)*.18f;
    if(++earlyWrite==(int)early.size())earlyWrite=0;
    std::array<float,8> v{};float sum=0;
    for(size_t i=0;i<8;++i){auto& line=lines[i];line.damp+=dampCoefficient*(line.data[(size_t)line.pos]-line.damp);v[i]=line.damp;sum+=v[i];}
    // Orthogonal Householder scattering: one acoustic field shared by both channels.
    for(size_t i=0;i<8;++i){auto& line=lines[i];line.data[(size_t)line.pos]=mono*(i%2?.21f:-.21f)+feedback[i]*(v[i]-.25f*sum);if(++line.pos==(int)line.data.size())line.pos=0;}
    float lateL=(v[0]+v[2]-v[4]+v[6])*.38f,lateR=(v[1]+v[3]-v[5]+v[7])*.38f;
    float amount=current[Room],distance=current[Distance];
    float direct=1-amount*(.12f+.20f*distance),reflections=amount*(.45f+.65f*distance);
    l=l*direct+(earlyL+lateL*.65f)*reflections;r=r*direct+(earlyR+lateR*.65f)*reflections;
}
void RadioEngine::process(juce::AudioBuffer<float>& buffer) noexcept {
    inputPeak=outputPeak=reduction=0;int channels=buffer.getNumChannels();if(channels<1)return;
    for(int offset=0;offset<buffer.getNumSamples();offset+=quantum){int n=std::min(quantum,buffer.getNumSamples()-offset);
        for(int j=0;j<n;++j){
            for(int k=0;k<ControlCount;++k)current[(size_t)k]=smooth[(size_t)k].getNextValue();
            if(coefficientCounter++%32==0)coefficients();
            float l=buffer.getSample(0,offset+j),r=channels>1?buffer.getSample(1,offset+j):l;
            if(!std::isfinite(l))l=0;if(!std::isfinite(r))r=0;
            inputPeak=std::max(inputPeak,std::max(std::abs(l),std::abs(r)));
            int read=(dryWrite+1)%(latencySamples+1);dryL[(size_t)j]=dry[0][(size_t)read];dryR[(size_t)j]=dry[1][(size_t)read];
            dry[0][(size_t)dryWrite]=l;dry[1][(size_t)dryWrite]=r;dryWrite=read;
            float in=juce::Decibels::decibelsToGain(current[Input]);l*=in;r*=in;room(l,r);
            // A shared detector, with reduced low-bass sensitivity, avoids stereo wander.
            float level=std::max(std::abs(l),std::abs(r));detectorLow+=detectorCoefficient*(level-detectorLow);
            float detectorInput=std::max(0.f,level-.45f*detectorLow);float a=detectorInput>detector?attack:release;detector=a*detector+(1-a)*detectorInput;
            float glue=current[Glue],threshold=-10-16*glue,ratio=1+3*glue,over=juce::Decibels::gainToDecibels(detector,-100.f)-threshold;
            float knee=6,curve=over<=-knee*.5f?0:over>=knee*.5f?over:(over+knee*.5f)*(over+knee*.5f)/(2*knee);
            float gr=curve*(1-1/ratio);float desired=juce::Decibels::decibelsToGain(-gr);compressGain=gainPole*compressGain+(1-gainPole)*desired;reduction=std::max(reduction,-juce::Decibels::gainToDecibels(compressGain,-100.f));
            float makeup=juce::Decibels::decibelsToGain(glue*1.6f);l*=compressGain*makeup;r*=compressGain*makeup;
            float mid=(l+r)*.5f,side=(l-r)*.5f*current[Width];l=mid+side;r=mid-side;
            for(int ch=0;ch<2;++ch){float x=ch==0?l:r;for(int index:{0,3,4})x=filters[(size_t)ch][(size_t)index].tick(x);wet.setSample(ch,j,x);}
            tubeAmount[(size_t)j]=current[Tubes];drive[(size_t)j]=1+7*current[Tubes];blend[(size_t)j]=current[Mix];gain[(size_t)j]=juce::Decibels::decibelsToGain(current[Output]);active[(size_t)j]=wetEnable.getNextValue();
        }
        auto block=juce::dsp::AudioBlock<float>(wet).getSubBlock(0,(size_t)n);auto up=oversampling.processSamplesUp(block);
        for(size_t ch=0;ch<2;++ch)for(size_t j=0;j<up.getNumSamples();++j){size_t at=std::min((size_t)n-1,j/4);float x=up.getSample((int)ch,(int)j),d=drive[at];
            // Odd, centred soft saturation; no DC offset and no artificial hiss.
            float y=std::tanh(x*d)/std::sqrt(d);up.setSample((int)ch,(int)j,x+tubeAmount[at]*(y-x));}
        oversampling.processSamplesDown(block);
        for(int j=0;j<n;++j){float dl=dryL[(size_t)j],dr=dryR[(size_t)j],mix=blend[(size_t)j]*active[(size_t)j];
            float wl=wet.getSample(0,j),wr=wet.getSample(1,j);for(int index:{1,2,5,6}){wl=filters[0][(size_t)index].tick(wl);wr=filters[1][(size_t)index].tick(wr);}
            float l=dl+mix*(dc[0].tick(wl)*gain[(size_t)j]-dl);
            float r=dr+mix*(dc[1].tick(wr)*gain[(size_t)j]-dr);
            buffer.setSample(0,offset+j,channels==1?(l+r)*.5f:l);if(channels>1)buffer.setSample(1,offset+j,r);
            outputPeak=std::max(outputPeak,std::max(std::abs(l),std::abs(r)));
        }
    }
}
