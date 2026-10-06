#pragma once
#include <algorithm>
#include <cmath>
#include <deque>
#include <stdexcept>
#include <string>
#include <vector>

namespace fp {
enum class Mode { Manual, Adaptive };
enum class Response { Smooth, Balanced, Responsive };
enum class State { Idle, Stable, Transient, SustainedDrop, Adapting, Recovering, Limited };
inline const char* label(State s) {
    switch(s) {
    case State::Idle:return "IDLE"; case State::Stable:return "STABLE";
    case State::Transient:return "TRANSIENT"; case State::SustainedDrop:return "SUSTAINED DROP";
    case State::Adapting:return "ADAPTING"; case State::Recovering:return "RECOVERING";
    default:return "LIMITED";
    }
}
struct Config {
    Mode mode=Mode::Adaptive; Response response=Response::Balanced;
    double minimum=30, maximum=120, manual=117;
    void validate() const {
        if(!std::isfinite(minimum)||!std::isfinite(maximum)||!std::isfinite(manual)||
           minimum<10||maximum>1000||minimum>maximum||manual<10||manual>1000)
            throw std::invalid_argument("FPS range must be finite, 10..1000, min <= max");
    }
};
struct Sample { double time, ms; };
struct Stats {
    double fps=0, median=0, mean=0, p95=0, p99=0, low1=0, deviation=0;
    size_t count=0;
};
inline double percentile(const std::vector<double>& sorted, double p) {
    if(sorted.empty())return 0;
    double index=p*(sorted.size()-1); size_t lo=static_cast<size_t>(index);
    size_t hi=std::min(lo+1,sorted.size()-1);
    return sorted[lo]+(sorted[hi]-sorted[lo])*(index-lo);
}
inline Stats statistics(const std::deque<Sample>& samples, double since) {
    std::vector<double> values;
    for(auto s:samples)if(s.time>=since)values.push_back(s.ms);
    Stats r; r.count=values.size(); if(values.empty())return r;
    double sum=0;for(double v:values)sum+=v;
    r.mean=sum/values.size(); r.fps=1000/r.mean;
    for(double v:values)r.deviation+=(v-r.mean)*(v-r.mean);
    r.deviation=std::sqrt(r.deviation/values.size());
    std::sort(values.begin(),values.end());
    r.median=percentile(values,.5);r.p95=percentile(values,.95);r.p99=percentile(values,.99);
    // 1% low = reciprocal of the mean of the slowest ceil(1% * N) frame times.
    size_t n=std::max<size_t>(1,static_cast<size_t>(std::ceil(values.size()*.01)));
    double tail=0;for(size_t i=values.size()-n;i<values.size();++i)tail+=values[i];
    r.low1=1000/(tail/n);return r;
}
class Engine {
    Config cfg_; std::deque<Sample> history_;
    double target_=120, evaluated_=-1, dropSince_=-1, stableSince_=-1;
    double probeSince_=-1, previousTarget_=0, cooldown_=0;
    State state_=State::Idle;
public:
    explicit Engine(Config c={}):cfg_(c) {c.validate();target_=c.mode==Mode::Manual?c.manual:c.maximum;}
    const Config& config()const{return cfg_;}
    void configure(Config c){c.validate();cfg_=c;reset();}
    void reset(){history_.clear();evaluated_=dropSince_=stableSince_=probeSince_=-1;
        cooldown_=0;target_=cfg_.mode==Mode::Manual?cfg_.manual:cfg_.maximum;state_=State::Idle;}
    void add(Sample s){
        if(!std::isfinite(s.time)||!std::isfinite(s.ms)||s.ms<=0||s.ms>10000)return;
        if(!history_.empty()&&s.time<history_.back().time)return;
        history_.push_back(s);
        while(!history_.empty()&&(s.time-history_.front().time>10||history_.size()>10000))history_.pop_front();
    }
    Stats stats(double now,double window=10)const{return statistics(history_,now-window);}
    const std::deque<Sample>& history()const{return history_;}
    double target()const{return target_;} State state()const{return state_;}
    void tick(double now){
        if(history_.empty()||now-history_.back().time>2){state_=State::Idle;return;}
        if(evaluated_>=0&&now-evaluated_<.19)return;
        evaluated_=now;
        if(cfg_.mode==Mode::Manual){target_=cfg_.manual;state_=State::Stable;return;}
        auto s=stats(now,1);if(s.count<10){state_=State::Transient;return;}
        double downDelay=cfg_.response==Response::Smooth?1.5:cfg_.response==Response::Responsive?.8:1.0;
        double upDelay=cfg_.response==Response::Smooth?5:cfg_.response==Response::Responsive?3:4;
        double sustainable=1000/s.median;
        if(target_==cfg_.minimum&&sustainable<cfg_.minimum*.94){
            state_=State::Limited;stableSince_=dropSince_=-1;return;
        }
        if(probeSince_>=0){
            state_=State::Recovering;
            if(now-probeSince_<1.2)return;
            if(sustainable<target_*.97||s.p95>1000/target_*1.20){
                target_=previousTarget_;cooldown_=now+upDelay*2;
            }
            probeSince_=-1;stableSince_=-1;dropSince_=-1;return;
        }
        if(sustainable<target_*.94){
            stableSince_=-1;
            if(dropSince_<0)dropSince_=now;
            state_=State::SustainedDrop;
            if(now-dropSince_>=downDelay){
                double proposed=std::floor(sustainable*.98);
                target_=std::clamp(proposed,cfg_.minimum,cfg_.maximum);
                state_=proposed<cfg_.minimum?State::Limited:State::Adapting;
                dropSince_=-1;cooldown_=now+upDelay;
            }
            return;
        }
        dropSince_=-1;
        if(s.p99>s.median*1.7||s.p95>1000/target_*1.20){
            state_=State::Transient;stableSince_=-1;return;
        }
        state_=State::Stable;
        if(stableSince_<0)stableSince_=now;
        // Capped FPS do not reveal spare capacity. Recovery uses bounded probes,
        // never an invented ceiling inferred from the current cap.
        if(target_<cfg_.maximum&&now>=cooldown_&&now-stableSince_>=upDelay){
            previousTarget_=target_;target_=std::min(cfg_.maximum,target_+std::min(10.0,target_*.1));
            probeSince_=now;state_=State::Recovering;
        }
    }
};
}
