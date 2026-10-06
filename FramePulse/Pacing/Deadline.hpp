#pragma once
#include <algorithm>
#include <cmath>
namespace fp {
// Absolute cadence; reset after misses instead of emitting catch-up bursts.
class Deadline {
    double next_=0,lastCap_=0;
public:
    double schedule(double now,double cap){
        if(!std::isfinite(cap)||cap<10||cap>1000){next_=lastCap_=0;return now;}
        double period=1/cap;
        if(next_==0||cap!=lastCap_||now>next_+period*2)next_=now;
        double due=std::max(now,next_);next_=due+period;lastCap_=cap;return due;
    }
};
}
