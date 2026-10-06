#pragma once
#include <windows.h>
#include <tlhelp32.h>
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <set>
#include <string>
#include <cwctype>
#include <vector>
namespace fp {
inline std::wstring lower(std::wstring s){std::transform(s.begin(),s.end(),s.begin(),towlower);return s;}
inline std::wstring processPath(DWORD pid){
    HANDLE h=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);if(!h)return {};
    wchar_t p[32768];DWORD n=32768;bool ok=QueryFullProcessImageNameW(h,0,p,&n);CloseHandle(h);
    return ok?std::wstring(p,n):L"";
}
struct Policy {
    std::set<std::wstring> games{L"skyrimse.exe",L"rdr2.exe",L"cyberpunk2077.exe",L"witcher3.exe",L"hogwartslegacy.exe",L"framepulsetestgame.exe"};
    std::vector<std::wstring> denied{L"vgc",L"vgk",L"vanguard",L"easyanticheat",L"battleye",L"beservice",L"faceit",L"ricochet"};
    std::set<std::wstring> competitive{L"valorant-win64-shipping.exe",L"cs2.exe",L"fortniteclient-win64-shipping.exe",L"r5apex.exe",L"cod.exe",L"overwatch.exe",L"rainbowsix.exe"};
    void load(const std::filesystem::path& dir){
        auto lines=[&](const wchar_t* name,auto fn){std::wifstream f(dir/name);std::wstring s;
            while(std::getline(f,s)){if(!s.empty()&&s.back()==L'\r')s.pop_back();
                if(!s.empty()&&s.front()!=L'#')fn(lower(s));}};
        lines(L"games.txt",[&](auto s){games.insert(s);});
        lines(L"anticheat.txt",[&](auto s){denied.push_back(s);});
        lines(L"competitive.txt",[&](auto s){competitive.insert(s);});
    }
    bool match(const std::wstring& name)const{
        auto n=lower(name);if(competitive.count(n))return true;
        for(auto& token:denied)if(n.find(token)!=std::wstring::npos)return true;
        return false;
    }
    // A positive hit anywhere disables pacing. Negative scans are NOT proof of safety.
    bool blocked(DWORD pid)const{
        if(match(std::filesystem::path(processPath(pid)).filename().wstring()))return true;
        HANDLE snap=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
        if(snap==INVALID_HANDLE_VALUE)return true;
        PROCESSENTRY32W pe{sizeof(pe)};bool hit=false;
        if(!Process32FirstW(snap,&pe)){CloseHandle(snap);return true;}
        do{if(match(pe.szExeFile)){hit=true;break;}}while(Process32NextW(snap,&pe));CloseHandle(snap);
        if(hit)return true;
        HANDLE modules=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32,pid);
        if(modules==INVALID_HANDLE_VALUE)return true;
        MODULEENTRY32W me{sizeof(me)};
        if(!Module32FirstW(modules,&me)){CloseHandle(modules);return true;}
        do{if(match(me.szModule)){hit=true;break;}}while(Module32NextW(modules,&me));CloseHandle(modules);
        SC_HANDLE scm=OpenSCManagerW(nullptr,nullptr,SC_MANAGER_ENUMERATE_SERVICE);
        if(!scm)return true;
        DWORD needed=0,count=0,resume=0;
        EnumServicesStatusExW(scm,SC_ENUM_PROCESS_INFO,SERVICE_WIN32|SERVICE_DRIVER,SERVICE_ACTIVE,nullptr,0,&needed,&count,&resume,nullptr);
        std::vector<BYTE> buffer(needed);resume=0;
        if(needed && !EnumServicesStatusExW(scm,SC_ENUM_PROCESS_INFO,SERVICE_WIN32|SERVICE_DRIVER,SERVICE_ACTIVE,
            buffer.data(),needed,&needed,&count,&resume,nullptr)){CloseServiceHandle(scm);return true;}
        auto entries=reinterpret_cast<ENUM_SERVICE_STATUS_PROCESSW*>(buffer.data());
        for(DWORD i=0;i<count;++i)if(match(entries[i].lpServiceName))hit=true;
        CloseServiceHandle(scm);return hit;
    }
};
}
