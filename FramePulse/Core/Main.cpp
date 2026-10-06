#include "Adaptive/Engine.hpp"
#include "AntiCheat/Policy.hpp"
#include "IPC/Control.hpp"
#include "Telemetry/PresentMon.hpp"
#include "Profiles/Store.hpp"
#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.ApplicationModel.AppService.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>
#include <shellapi.h>
#include <sddl.h>
#include <dxgi1_5.h>
#include <mutex>
#include <memory>
#include <fstream>

using namespace winrt;
using namespace Windows::ApplicationModel::AppService;
using namespace Windows::Foundation::Collections;
using namespace Windows::Data::Json;
namespace {
constexpr UINT exitMessage=WM_APP+1,reconnectMessage=WM_APP+2;
DWORD mainThread=0,pid=0;HWND gameWindow=nullptr;HANDLE game=nullptr,gameWait=nullptr;
HANDLE map=nullptr;fp::Control* control=nullptr;
std::mutex mutex;fp::Engine engine;fp::Policy policy;fp::PresentMon telemetry;
std::unique_ptr<fp::Store> profiles;
std::filesystem::path base,data;std::wstring exe;
std::string error,warning,chain;bool blocked=false,safe=true,fgSuspected=false,fgOffConfirmed=false;
std::ofstream debugLog;size_t logRows=0;
double refresh=0,lastTelemetry=0,displayMs=0;unsigned dropped=0;
double telemetryClockOffset=0;bool telemetryClockSet=false;
unsigned long long epoch=0;double lastPolicy=0;
AppServiceConnection connection{nullptr};HANDLE reconnectEvent=nullptr,reconnectWait=nullptr;
UINT_PTR timerId=0;
double now(){LARGE_INTEGER q,f;QueryPerformanceCounter(&q);QueryPerformanceFrequency(&f);return double(q.QuadPart)/f.QuadPart;}
double monitorRefresh(HWND window){
    HMONITOR mon=MonitorFromWindow(window,MONITOR_DEFAULTTONEAREST);
    MONITORINFOEXW info{};info.cbSize=sizeof(info);if(!GetMonitorInfoW(mon,&info))return 0;
    UINT32 pathsCount=0,modesCount=0;
    if(GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS,&pathsCount,&modesCount)==ERROR_SUCCESS){
        std::vector<DISPLAYCONFIG_PATH_INFO> paths(pathsCount);std::vector<DISPLAYCONFIG_MODE_INFO> modes(modesCount);
        if(QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS,&pathsCount,paths.data(),&modesCount,modes.data(),nullptr)==ERROR_SUCCESS){
            for(UINT32 i=0;i<pathsCount;++i){auto& p=paths[i];DISPLAYCONFIG_SOURCE_DEVICE_NAME source{};
                source.header.type=DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;source.header.size=sizeof(source);
                source.header.adapterId=p.sourceInfo.adapterId;source.header.id=p.sourceInfo.id;
                if(DisplayConfigGetDeviceInfo(&source.header)==ERROR_SUCCESS&&std::wcscmp(source.viewGdiDeviceName,info.szDevice)==0){
                    auto rate=p.targetInfo.refreshRate;if(rate.Denominator)return double(rate.Numerator)/rate.Denominator;
                }
            }
        }
    }
    DEVMODEW mode{};mode.dmSize=sizeof(mode);
    return EnumDisplaySettingsW(info.szDevice,ENUM_CURRENT_SETTINGS,&mode)?mode.dmDisplayFrequency:0;
}
void inspectModules(){
    warning.clear();fgSuspected=false;
    HANDLE snap=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32,pid);
    if(snap==INVALID_HANDLE_VALUE)return;
    MODULEENTRY32W m{sizeof(m)};
    if(Module32FirstW(snap,&m))do{auto name=fp::lower(m.szModule);
        if(name.find(L"dlssg")!=std::wstring::npos||name.find(L"sl.interposer")!=std::wstring::npos||name.find(L"ffx_fsr3")!=std::wstring::npos){
            fgSuspected=true;warning="FG components detected; pacing disabled pending an adapter";}
        if(name.find(L"rtsshooks")!=std::wstring::npos)warning="RTSS detected: check existing limiters";
    }while(Module32NextW(snap,&m));CloseHandle(snap);
}
void disableControl(){if(control)fp::write(&control->enabled,0);}
void endGame(){
    {std::lock_guard lock(mutex);++epoch;disableControl();}
    telemetry.stop(); // Never join a callback while holding the engine lock.
    if(gameWait){UnregisterWaitEx(gameWait,INVALID_HANDLE_VALUE);gameWait=nullptr;}
    if(game){CloseHandle(game);game=nullptr;}
    if(timerId){KillTimer(nullptr,timerId);timerId=0;}
    std::lock_guard lock(mutex);
    if(control){UnmapViewOfFile(control);control=nullptr;}if(map){CloseHandle(map);map=nullptr;}
    pid=0;exe.clear();chain.clear();engine.reset();lastTelemetry=0;displayMs=0;dropped=0;telemetryClockSet=false;
    error.clear();warning.clear();blocked=false;fgSuspected=false;safe=true;fgOffConfirmed=false;
}
void CALLBACK exited(PVOID context,BOOLEAN){PostThreadMessageW(mainThread,exitMessage,reinterpret_cast<WPARAM>(context),0);}
void CALLBACK reconnect(PVOID,BOOLEAN){PostThreadMessageW(mainThread,reconnectMessage,0,0);}
void selectGame(HWND window){
    DWORD candidate=0;GetWindowThreadProcessId(window,&candidate);if(!candidate||candidate==pid)return;
    auto path=fp::processPath(candidate);auto name=fp::lower(std::filesystem::path(path).filename().wstring());
    if(!policy.games.count(name)&&!policy.competitive.count(name))return;
    endGame();
    std::lock_guard lock(mutex);pid=candidate;exe=name;gameWindow=window;
    game=OpenProcess(SYNCHRONIZE|PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);
    if(!game){pid=0;error="Game inspection unavailable";return;}
    RegisterWaitForSingleObject(&gameWait,game,exited,reinterpret_cast<PVOID>(static_cast<uintptr_t>(pid)),INFINITE,WT_EXECUTEONLYONCE);
    refresh=monitorRefresh(window);blocked=policy.blocked(pid);inspectModules();
    try{engine.configure(profiles->load(exe));profiles->save(exe,engine.config());}
    catch(...){engine.configure({});warning="Profile could not be loaded or saved; defaults active";}
    // Default token DACL grants the launching user/admins, not Everyone.
    map=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(fp::Control),fp::mappingName(pid).c_str());
    if(map&&GetLastError()!=ERROR_ALREADY_EXISTS){
        control=static_cast<fp::Control*>(MapViewOfFile(map,FILE_MAP_ALL_ACCESS,0,0,sizeof(fp::Control)));
        if(control){ZeroMemory(control,sizeof(*control));fp::write(&control->version,fp::protocol);
            InterlockedExchange64(&control->window,reinterpret_cast<LONG64>(window));}
    }else {if(map)CloseHandle(map);map=nullptr;error="Control mapping already exists or failed";}
    unsigned long long generation=epoch;DWORD selected=pid;
    auto presentMon=base/L"PresentMon.exe";
    bool started=telemetry.start(presentMon,pid,[generation,selected](fp::FrameEvent f){
        double arrival=now();std::lock_guard lock(mutex);
        if(epoch!=generation||pid!=selected||f.pid!=pid)return;
        // Conservative first swapchain lock. Multi-swapchain selection is a known limitation.
        if(chain.empty())chain=f.swapchain;if(f.swapchain!=chain)return;
        lastTelemetry=arrival;
        if(!telemetryClockSet){telemetryClockOffset=std::abs(arrival-f.time)>60?arrival-f.time:0;telemetryClockSet=true;}
        engine.add({f.time+telemetryClockOffset,f.ms});if(f.displayedMs&&*f.displayedMs>0)displayMs=*f.displayedMs;
        if(f.dropped)++dropped;
    });
    if(!started)error="PresentMon missing or launch failed";
    timerId=SetTimer(nullptr,0,200,nullptr);lastPolicy=0;
}
void CALLBACK foreground(HWINEVENTHOOK,DWORD,HWND window,LONG,LONG,DWORD,DWORD){if(window)selectGame(window);}
JsonObject snapshot(){
    JsonObject o;auto t=now();auto s=engine.stats(t);
    if(!lastTelemetry||t-lastTelemetry>2)s={};
    auto number=[&](const wchar_t* n,double v){o.SetNamedValue(n,JsonValue::CreateNumberValue(v));};
    auto text=[&](const wchar_t* n,std::string v){o.SetNamedValue(n,JsonValue::CreateStringValue(to_hstring(v)));};
    number(L"pid",pid);number(L"fps",s.fps);number(L"frametime",s.median);number(L"low1",s.low1);
    number(L"p95",s.p95);number(L"p99",s.p99);number(L"deviation",s.deviation);number(L"target",engine.target());
    number(L"refresh",refresh);number(L"dropped",dropped);number(L"displayFps",displayMs>0&&t-lastTelemetry<2?1000/displayMs:0);
    text(L"metric","PRESENT FPS");text(L"fg",fgSuspected?"FG COMPONENTS DETECTED; ACTIVE UNKNOWN":fgOffConfirmed?"USER CONFIRMED FG OFF":"UNKNOWN");
    text(L"vrr","UNKNOWN (driver setting not measured)");text(L"game",to_string(exe));
    std::string status=pid?fp::label(engine.state()):"IDLE";
    if(pid&&lastTelemetry==0)status="WAITING FOR TELEMETRY";
    if(blocked)status="PACING BLOCKED — POLICY / INSPECTION";
    text(L"state",status);text(L"error",error);text(L"warning",warning);
    bool active=control&&fp::read(&control->hooked)&&fp::read(&control->presents)>0&&fp::read(&control->enabled)&&
        GetTickCount()-static_cast<DWORD>(fp::read(&control->lastPacedTick))<2000;
    text(L"pacing",active?"PACING ACTIVE":safe?"SAFE MODE — TELEMETRY ONLY":"PACING UNAVAILABLE");
    o.SetNamedValue(L"safe",JsonValue::CreateBooleanValue(safe));o.SetNamedValue(L"config",fp::configJson(engine.config()));
    o.SetNamedValue(L"fgOffConfirmed",JsonValue::CreateBooleanValue(fgOffConfirmed));
    JsonArray graph;
    // Bounded 5 Hz buckets retain the worst frame in each bin.
    double bin=0,peak=0;bool any=false;
    for(auto f:engine.history()){
        double b=std::floor(f.time*5)/5;
        if(any&&b!=bin){graph.Append(JsonValue::CreateNumberValue(peak));peak=0;}
        bin=b;peak=std::max(peak,f.ms);any=true;
    }
    if(any)graph.Append(JsonValue::CreateNumberValue(peak));o.SetNamedValue(L"graph",graph);return o;
}
fire_and_forget request(AppServiceConnection sender,AppServiceRequestReceivedEventArgs args){
    auto deferral=args.GetDeferral();ValueSet reply;
    try{
        auto message=args.Request().Message();std::lock_guard lock(mutex);
        auto op=message.HasKey(L"op")?unbox_value<hstring>(message.Lookup(L"op")):L"snapshot";
        if(op==L"configure"){
            if(!pid)throw std::runtime_error("Open a recognized game before editing its profile");
            auto json=JsonObject::Parse(unbox_value<hstring>(message.Lookup(L"config")));
            auto cfg=fp::parseConfig(json);profiles->save(exe,cfg);engine.configure(cfg);
        }else if(op==L"safe"){
            bool requested=unbox_value<bool>(message.Lookup(L"value"));
            if(!requested&&(blocked||fgSuspected||!fgOffConfirmed||(exe!=L"skyrimse.exe"&&exe!=L"framepulsetestgame.exe")))
                throw std::runtime_error("Requires approved DXGI adapter, policy clearance and confirmed FG off");
            safe=requested;if(safe)disableControl();
        }else if(op==L"fgOff"){
            fgOffConfirmed=unbox_value<bool>(message.Lookup(L"value"));
            if(!fgOffConfirmed){safe=true;disableControl();}
        }else if(op==L"attach"){
            if(safe||!fgOffConfirmed||fgSuspected||policy.blocked(pid)||(exe!=L"skyrimse.exe"&&exe!=L"framepulsetestgame.exe"))
                throw std::runtime_error("Select Advanced Pacing, confirm FG off, and use an approved game first");
            auto attach=base/L"FramePulseAttach.exe";
            std::wstring cmd=L"\""+attach.wstring()+L"\" "+std::to_wstring(pid)+L" --ack-advanced";
            STARTUPINFOW startup{sizeof(startup)};PROCESS_INFORMATION process{};
            if(!CreateProcessW(attach.c_str(),cmd.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,base.c_str(),&startup,&process))
                throw std::runtime_error("Attach helper failed to start");
            CloseHandle(process.hThread);CloseHandle(process.hProcess);
        }else if(op==L"openLogs"){
            ShellExecuteW(nullptr,L"open",data.c_str(),nullptr,nullptr,SW_SHOWNORMAL);
        }else if(op==L"debug"){
            debugLog.close();logRows=0;
            if(unbox_value<bool>(message.Lookup(L"value"))){
                debugLog.open(data/L"debug.csv",std::ios::trunc);
                if(!debugLog)throw std::runtime_error("Debug log could not be opened");
                debugLog<<"qpc_seconds,fps,median_ms,p95_ms,p99_ms,low1,target,state\n";
            }
        }else if(op==L"shutdown")PostThreadMessageW(mainThread,WM_QUIT,0,0);
        else if(op!=L"snapshot")throw std::runtime_error("Unsupported IPC operation");
        reply.Insert(L"json",box_value(snapshot().Stringify()));
    }catch(const std::exception& e){reply.Insert(L"error",box_value(to_hstring(e.what())));}
    catch(...){reply.Insert(L"error",box_value(L"Request rejected"));}
    try{co_await args.Request().SendResponseAsync(reply);}catch(...){}
    deferral.Complete();
}
void connect(){
    try{
        if(connection)connection.Close();
        connection=AppServiceConnection();connection.AppServiceName(L"FramePulse.Bridge");
        connection.PackageFamilyName(Windows::ApplicationModel::Package::Current().Id().FamilyName());
        connection.RequestReceived(request);
        auto result=connection.OpenAsync().get();
        if(result!=AppServiceConnectionStatus::Success){connection.Close();connection=nullptr;}
    }catch(...){connection=nullptr;}
}
void tick(){
    if(game&&WaitForSingleObject(game,0)==WAIT_OBJECT_0){endGame();return;}
    std::lock_guard lock(mutex);double t=now();engine.tick(t);
    if(!telemetry.running())error="PresentMon exited; check ETW permissions and CLI compatibility";
    if(t-lastPolicy>1){blocked=policy.blocked(pid);inspectModules();lastPolicy=t;refresh=monitorRefresh(gameWindow);}
    if(control){fp::write(&control->milliFps,static_cast<LONG>(engine.target()*1000));
        fp::write(&control->heartbeat,static_cast<LONG>(GetTickCount()));
        bool telemetryReady=lastTelemetry>0&&t-lastTelemetry<2;
        fp::write(&control->enabled,!safe&&!blocked&&!fgSuspected&&fgOffConfirmed&&
            (engine.config().mode==fp::Mode::Manual||telemetryReady)&&(exe==L"skyrimse.exe"||exe==L"framepulsetestgame.exe"));}
    if(debugLog&&logRows<18000){auto s=engine.stats(t);
        debugLog<<t<<','<<s.fps<<','<<s.median<<','<<s.p95<<','<<s.p99<<','<<s.low1<<','<<engine.target()<<','<<fp::label(engine.state())<<'\n';
        ++logRows;if(logRows==18000)debugLog.close();}
}
}
int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int){
    init_apartment(apartment_type::multi_threaded);mainThread=GetCurrentThreadId();
    MSG ensure;PeekMessageW(&ensure,nullptr,0,0,PM_NOREMOVE);
    HANDLE instance=CreateMutexW(nullptr,FALSE,L"Local\\FramePulse.Core.Instance");
    bool existing=GetLastError()==ERROR_ALREADY_EXISTS;
    reconnectEvent=CreateEventW(nullptr,FALSE,FALSE,L"Local\\FramePulse.Core.Reconnect");
    if(existing){SetEvent(reconnectEvent);CloseHandle(reconnectEvent);CloseHandle(instance);return 0;}
    wchar_t self[32768],local[32768];GetModuleFileNameW(nullptr,self,32768);base=std::filesystem::path(self).parent_path();
    DWORD n=GetEnvironmentVariableW(L"LOCALAPPDATA",local,32768);
    if(!n||n>=32768)return 1;data=std::filesystem::path(local)/L"FramePulse";
    profiles=std::make_unique<fp::Store>(data/L"Profiles");policy.load(data);
    RegisterWaitForSingleObject(&reconnectWait,reconnectEvent,reconnect,nullptr,INFINITE,WT_EXECUTEDEFAULT);
    connect();
    auto hook=SetWinEventHook(EVENT_SYSTEM_FOREGROUND,EVENT_SYSTEM_FOREGROUND,nullptr,foreground,0,0,WINEVENT_OUTOFCONTEXT|WINEVENT_SKIPOWNPROCESS);
    selectGame(GetForegroundWindow());
    if(!pid)EnumWindows([](HWND w,LPARAM)->BOOL{
        if(IsWindowVisible(w))selectGame(w);return pid?FALSE:TRUE;
    },0);
    MSG msg;
    while(GetMessageW(&msg,nullptr,0,0)>0){
        if(msg.message==WM_TIMER)tick();
        else if(msg.message==exitMessage&&msg.wParam==pid)endGame();
        else if(msg.message==reconnectMessage)connect();
        else {TranslateMessage(&msg);DispatchMessageW(&msg);}
    }
    if(hook)UnhookWinEvent(hook);endGame();if(connection)connection.Close();
    if(reconnectWait)UnregisterWaitEx(reconnectWait,INVALID_HANDLE_VALUE);
    CloseHandle(reconnectEvent);CloseHandle(instance);return 0;
}
