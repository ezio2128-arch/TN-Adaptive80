#pragma once
#include "Telemetry/Csv.hpp"
#include <windows.h>
#include <functional>
#include <thread>
#include <filesystem>
#include <string>
namespace fp {
class PresentMon {
    HANDLE process_=nullptr,pipe_=nullptr;std::thread reader_;
public:
    ~PresentMon(){stop();}
    PresentMon()=default;PresentMon(const PresentMon&)=delete;
    bool running()const{return process_&&WaitForSingleObject(process_,0)==WAIT_TIMEOUT;}
    void stop(){
        if(process_){TerminateProcess(process_,0);WaitForSingleObject(process_,2000);}
        if(reader_.joinable())reader_.join();
        if(pipe_)CloseHandle(pipe_);if(process_)CloseHandle(process_);
        pipe_=process_=nullptr;
    }
    bool start(const std::filesystem::path& executable,DWORD pid,std::function<void(FrameEvent)> callback){
        stop();if(!std::filesystem::is_regular_file(executable))return false;
        SECURITY_ATTRIBUTES sa{sizeof(sa),nullptr,TRUE};HANDLE write=nullptr;
        if(!CreatePipe(&pipe_,&write,&sa,0))return false;
        SetHandleInformation(pipe_,HANDLE_FLAG_INHERIT,0);
        HANDLE input=CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&sa,OPEN_EXISTING,0,nullptr);
        std::wstring cmd=L"\""+executable.wstring()+L"\" --process_id "+std::to_wstring(pid)+
            L" --output_stdout --no_console_stats --qpc_time_ms --no_track_input --terminate_on_proc_exit --stop_existing_session --session_name FramePulse."+std::to_wstring(pid);
        STARTUPINFOW si{sizeof(si)};si.dwFlags=STARTF_USESTDHANDLES;si.hStdOutput=si.hStdError=write;si.hStdInput=input;
        PROCESS_INFORMATION pi{};
        BOOL ok=CreateProcessW(executable.c_str(),cmd.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW,nullptr,executable.parent_path().c_str(),&si,&pi);
        CloseHandle(write);if(input!=INVALID_HANDLE_VALUE)CloseHandle(input);
        if(!ok){CloseHandle(pipe_);pipe_=nullptr;return false;}
        process_=pi.hProcess;CloseHandle(pi.hThread);
        reader_=std::thread([this,callback]{
            CsvDecoder decoder;std::string pending;char buffer[8192];DWORD got=0;
            while(ReadFile(pipe_,buffer,sizeof(buffer),&got,nullptr)&&got){
                pending.append(buffer,got);size_t pos;
                while((pos=pending.find('\n'))!=std::string::npos){
                    std::string line=pending.substr(0,pos);pending.erase(0,pos+1);
                    if(!decoder.header(line)){auto event=decoder.decode(line);if(event)callback(*event);}
                }
                if(pending.size()>65536)pending.clear();
            }
        });return true;
    }
};
}
