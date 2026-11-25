#include "VinylTexture.h"
#include <algorithm>
#include <cmath>

float VinylTexture::uniform() noexcept {
    randomState^=randomState>>12;randomState^=randomState<<25;randomState^=randomState>>27;
    return ((randomState*2685821657736338717ULL)>>40)*(1.f/16777216.f);
}
double VinylTexture::interval(float rate) noexcept {return -std::log(std::max(1e-7f,1-uniform()))*sr/rate;}
void VinylTexture::prepare(double rate) noexcept {
    sr=rate;grains={};nextGrain=0;randomState=0x9e3779b97f4a7c15ULL;
    fineClock=interval(90);popClock=interval(2);driftClock=sr*.3;
    drift=driftTarget=1;surfaceLow=surfaceHigh=0;
    driftPole=(float)std::exp(-1/(sr*.22));lowPole=(float)std::exp(-6.28318530718*650/sr);highPole=(float)std::exp(-6.28318530718*5000/sr);
}
void VinylTexture::trigger(bool pop) noexcept {
    auto& g=grains[nextGrain];nextGrain=(nextGrain+1)%grains.size();
    float r=uniform(),weight=r*r*r;
    float amplitude=(pop?.025f+.20f*weight:.002f+.055f*weight)*(uniform()<.5f?-1.f:1.f);
    double fast=pop?.000045+.00013*uniform():.000025+.000055*uniform();
    double slow=pop?.0008+.0038*uniform():.00010+.00065*uniform();
    g.fast=g.slow=amplitude;g.fastPole=(float)std::exp(-1/(sr*fast));g.slowPole=(float)std::exp(-1/(sr*slow));
    // Mostly common groove noise; modest lateral differences follow Width later.
    g.pan=(uniform()*2-1)*.35f;
}
std::array<float,2> VinylTexture::tick(float amount) noexcept {
    if(amount<=0)return {};
    if(--driftClock<=0){driftTarget=.65f+uniform()*.8f;driftClock=sr*(.15+.65*uniform());}
    drift=driftPole*drift+(1-driftPole)*driftTarget;
    float fineRate=(45+190*amount)*drift,popRate=(.45f+4.5f*amount*amount)*drift;
    if(--fineClock<=0){trigger(false);fineClock+=interval(fineRate);}
    if(--popClock<=0){trigger(true);popClock+=interval(popRate);}
    float l=0,r=0;
    for(auto& g:grains){g.fast*=g.fastPole;g.slow*=g.slowPole;float pulse=g.fast-g.slow;l+=pulse*(1-g.pan);r+=pulse*(1+g.pan);}
    // A very low, band-limited surface floor underneath the discrete grains.
    float white=uniform()*2-1;surfaceLow=lowPole*surfaceLow+(1-lowPole)*white;surfaceHigh=highPole*surfaceHigh+(1-highPole)*white;
    float surface=(surfaceHigh-surfaceLow)*.0025f*drift;
    float level=amount*std::sqrt(amount);
    return {(l+surface)*level,(r+surface)*level};
}
