#pragma once
#include <charconv>
#include <cmath>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
namespace fp {
inline std::vector<std::string> csv(std::string_view line){
    std::vector<std::string> out;std::string value;bool quote=false;
    for(size_t i=0;i<line.size();++i){char c=line[i];
        if(c=='"'){if(quote&&i+1<line.size()&&line[i+1]=='"'){value+='"';++i;}else quote=!quote;}
        else if(c==','&&!quote){out.push_back(value);value.clear();}
        else if(c!='\r')value+=c;
    }out.push_back(value);return out;
}
inline std::optional<double> number(std::string_view s){
    double n=0;auto r=std::from_chars(s.data(),s.data()+s.size(),n);
    if(r.ec!=std::errc{}||r.ptr!=s.data()+s.size()||!std::isfinite(n))return {};
    return n;
}
struct FrameEvent {
    unsigned pid=0;std::string swapchain,frameType;
    double time=0,ms=0;std::optional<double> displayedMs;bool dropped=false;
};
class CsvDecoder {
    std::unordered_map<std::string,size_t> columns_;
    std::unordered_map<std::string,double> previous_;
public:
    bool header(std::string_view line){
        auto fields=csv(line);bool found=false;
        for(auto& f:fields)if(f=="ProcessID")found=true;
        if(!found)return false;
        columns_.clear();previous_.clear();
        for(size_t i=0;i<fields.size();++i)columns_[fields[i]]=i;
        return true;
    }
    std::optional<FrameEvent> decode(std::string_view line){
        auto f=csv(line);
        auto get=[&](const char* key)->std::string{
            auto it=columns_.find(key);return it!=columns_.end()&&it->second<f.size()?f[it->second]:"";};
        auto pid=number(get("ProcessID"));if(!pid||*pid<1||*pid>4294967295.0)return {};
        FrameEvent e;e.pid=static_cast<unsigned>(*pid);e.swapchain=get("SwapChainAddress");
        e.frameType=get("FrameType");
        auto t=number(get("CPUStartQPCTime"));if(t)e.time=*t/1000;
        else {t=number(get("TimeInSeconds"));if(!t)return {};e.time=*t;}
        auto ms=number(get("MsBetweenPresents"));
        std::string key=std::to_string(e.pid)+":"+e.swapchain;
        auto previous=previous_.find(key);
        if(ms)e.ms=*ms;else if(previous!=previous_.end())e.ms=(e.time-previous->second)*1000;
        // Bound malformed streams and PID/swapchain churn.
        if(previous_.size()>256)previous_.clear();
        previous_[key]=e.time;
        e.displayedMs=number(get("MsBetweenDisplayChange"));
        if(!e.displayedMs)e.displayedMs=number(get("DisplayedTime"));
        e.dropped=get("Dropped")=="1"||(columns_.count("DisplayedTime")&&get("DisplayedTime")=="NA");
        if(e.ms<=0||e.ms>10000)return {};
        return e;
    }
};
}
