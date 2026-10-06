#include "Adaptive/Engine.hpp"
#include "Telemetry/Csv.hpp"
#include "Pacing/Deadline.hpp"
#include <chrono>
#include <iostream>
#include <limits>
#include <random>
#include <cstdlib>
int checks=0;
void check(bool b,const char* name){++checks;if(!b){std::cerr<<"FAIL: "<<name<<'\n';std::exit(1);}}
struct Run {
    fp::Engine e; double time=0;
    void feed(double capacity,double seconds,bool capped=true){
        double end=time+seconds;
        while(time<end){double fps=capped?std::min(capacity,e.target()):capacity;
            double ms=1000/fps;time+=ms/1000;e.add({time,ms});e.tick(time);}
    }
};
int main(){
    Run a;a.feed(120,8);check(a.e.target()==120,"stable maximum unchanged");
    a.feed(60,1.0/60,false);a.feed(120,2);check(a.e.target()==120,"single spike ignored");
    a.feed(80,.5,false);check(a.e.target()==120,"short dip ignored");
    a.feed(80,3);check(a.e.target()>=77&&a.e.target()<=80,"sustained drop adapts");
    double low=a.e.target();a.feed(120,1);check(a.e.target()==low,"recovery waits");
    a.feed(120,5);check(a.e.target()>low&&a.e.target()<110,"recovery bounded step");
    a.feed(120,30);check(a.e.target()==120,"eventual recovery to ceiling");
    Run b;b.feed(80,20);check(b.e.target()<=86,"failed recovery probe rolls back");
    Run c;c.feed(20,4);check(c.e.target()==30,"minimum cap floor respected");
    c.feed(20,1);check(c.e.state()==fp::State::Limited,"below floor remains LIMITED");
    fp::Config manual;manual.mode=fp::Mode::Manual;manual.manual=117;
    fp::Engine m(manual);for(int i=0;i<200;++i){m.add({i*.02,20});m.tick(i*.02);}
    check(m.target()==117,"manual never adapts");m.tick(10);check(m.state()==fp::State::Idle,"stale data idle");
    m.add({11,std::numeric_limits<double>::quiet_NaN()});check(m.history().size()==200,"invalid sample ignored");
    fp::Config bad;bad.minimum=130;bool threw=false;try{m.configure(bad);}catch(...){threw=true;}
    check(threw,"invalid configuration rejected");
    std::deque<fp::Sample> samples;for(int i=0;i<100;++i)samples.push_back({i*.01,i==99?100.0:10.0});
    auto s=fp::statistics(samples,0);check(std::abs(s.low1-10)<1e-9,"1 percent low definition");
    check(s.median==10&&s.p95==10&&s.p99>10,"tail statistics");
    fp::Deadline d;double first=d.schedule(1,100);check(first==1,"first frame immediate");
    check(std::abs(d.schedule(1.005,100)-1.01)<1e-9,"absolute deadline cadence");
    check(d.schedule(3,100)==3,"stall resets cadence without burst");
    check(d.schedule(3.001,60)==3.001,"cap change resets deadline");
    check(d.schedule(4,0)==4,"disabled limiter immediate");
    fp::CsvDecoder csv;check(csv.header("Application,ProcessID,SwapChainAddress,CPUStartQPCTime,MsBetweenPresents,DisplayedTime,FrameType"),"modern CSV header");
    auto f=csv.decode("\"Game, Name.exe\",42,0xA,1000,8.3,8.3,Application");
    check(f&&f->pid==42&&f->time==1&&f->ms==8.3,"quoted CSV parse");
    f=csv.decode("Game.exe,42,0xA,1008.3,8.3,NA,Application");check(f&&f->dropped,"dropped frame detected");
    check(!csv.decode("Game.exe,NaN,0xA,1000,8.3,8.3,Application"),"invalid PID rejected");
    csv.header("Application,ProcessID,SwapChainAddress,CPUStartQPCTime,DisplayedTime");
    csv.decode("Game.exe,42,0xA,1000,8.3");f=csv.decode("Game.exe,42,0xA,1010,10");
    check(f&&std::abs(f->ms-10)<1e-6,"timestamp fallback");
    f=csv.decode("Game.exe,42,0xB,1010,10");check(!f,"swapchain clocks isolated");
    // FG output is not used as base performance and never divided by an assumed x2.
    Run base;base.feed(60,5);check(base.e.target()<65,"base 60 adapts independently of output 120");
    fp::Config vrr;vrr.maximum=162;fp::Engine vr(vrr);check(vr.target()==162,"explicit VRR ceiling respected");
    for(auto response:{fp::Response::Smooth,fp::Response::Balanced,fp::Response::Responsive}){
        fp::Config cfg;cfg.response=response;fp::Engine e(cfg);
        double t=0;std::mt19937 rng(42);std::uniform_real_distribution<double> fps(75,120);
        for(int i=0;i<4000;++i){double ms=1000/fps(rng);t+=ms/1000;e.add({t,ms});e.tick(t);}
        check(e.target()>=30&&e.target()<=120,"variable workload bounded");
    }
    auto begin=std::chrono::steady_clock::now();Run bench;bench.feed(120,600);
    double elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();
    std::cout<<"PASS "<<checks<<" checks\n600 simulated seconds (72000 frames): "<<elapsed<<" ms wall time\n";
}
