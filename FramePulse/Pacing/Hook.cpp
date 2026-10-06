#include "IPC/Control.hpp"
#include "Pacing/Deadline.hpp"
#include <d3d11.h>
#include <dxgi1_2.h>
#include <MinHook.h>
#include <mutex>

namespace {
using Present=HRESULT (STDMETHODCALLTYPE*)(IDXGISwapChain*,UINT,UINT);
using Present1=HRESULT (STDMETHODCALLTYPE*)(IDXGISwapChain1*,UINT,UINT,const DXGI_PRESENT_PARAMETERS*);
Present original=nullptr;Present1 original1=nullptr;
fp::Control* control=nullptr;HANDLE mapping=nullptr;
std::mutex cadence;fp::Deadline deadline;thread_local bool nested=false;
LARGE_INTEGER frequency;
double now(){LARGE_INTEGER q;QueryPerformanceCounter(&q);return double(q.QuadPart)/frequency.QuadPart;}
void pace(IDXGISwapChain* sc,UINT flags){
    if(flags&(DXGI_PRESENT_TEST|DXGI_PRESENT_DO_NOT_WAIT))return;
    if(!control||fp::read(&control->version)!=fp::protocol)return;
    if(!fp::read(&control->enabled))return;
    auto age=GetTickCount()-static_cast<DWORD>(fp::read(&control->heartbeat));
    if(age>2000)return; // Crash / disconnect fails open: no sleeping in the game.
    DXGI_SWAP_CHAIN_DESC desc{};
    if(FAILED(sc->GetDesc(&desc)))return;
    auto target=static_cast<uintptr_t>(InterlockedCompareExchange64(&control->window,0,0));
    if(target&&reinterpret_cast<uintptr_t>(desc.OutputWindow)!=target)return;
    double cap=fp::read(&control->milliFps)/1000.0;
    if(cap<10||cap>1000)return;
    InterlockedIncrement(&control->presents);fp::write(&control->lastPacedTick,static_cast<LONG>(GetTickCount()));
    std::lock_guard lock(cadence);
    double due=deadline.schedule(now(),cap);
    thread_local HANDLE timer=CreateWaitableTimerExW(nullptr,nullptr,CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,TIMER_MODIFY_STATE|SYNCHRONIZE);
    double remaining=due-now();
    // 50 us final spin budget; no system-wide timeBeginPeriod request.
    if(remaining>.00010&&timer){
        LARGE_INTEGER delay;delay.QuadPart=-static_cast<LONGLONG>((remaining-.00005)*10000000);
        if(SetWaitableTimerEx(timer,&delay,0,nullptr,nullptr,nullptr,0))
            WaitForSingleObject(timer,static_cast<DWORD>(remaining*1000)+5);
    }
    else if(remaining>.001&&!timer)Sleep(static_cast<DWORD>(remaining*1000));
    while(now()<due){YieldProcessor();}
}
HRESULT STDMETHODCALLTYPE hooked(IDXGISwapChain* sc,UINT sync,UINT flags){
    if(nested)return original(sc,sync,flags);
    nested=true;pace(sc,flags);HRESULT hr=original(sc,sync,flags);nested=false;return hr;
}
HRESULT STDMETHODCALLTYPE hooked1(IDXGISwapChain1* sc,UINT sync,UINT flags,const DXGI_PRESENT_PARAMETERS* parameters){
    if(nested)return original1(sc,sync,flags,parameters);
    nested=true;pace(sc,flags);HRESULT hr=original1(sc,sync,flags,parameters);nested=false;return hr;
}
DWORD WINAPI initialize(void*){
    mapping=OpenFileMappingW(FILE_MAP_READ|FILE_MAP_WRITE,FALSE,fp::mappingName(GetCurrentProcessId()).c_str());
    if(!mapping)return 1;
    control=static_cast<fp::Control*>(MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(fp::Control)));
    if(!control||fp::read(&control->version)!=fp::protocol)return 2;
    QueryPerformanceFrequency(&frequency);
    WNDCLASSW cls{};cls.lpfnWndProc=DefWindowProcW;cls.lpszClassName=L"FramePulse.Probe";cls.hInstance=GetModuleHandleW(nullptr);
    RegisterClassW(&cls);
    HWND window=CreateWindowW(cls.lpszClassName,L"",WS_OVERLAPPED,0,0,32,32,nullptr,nullptr,cls.hInstance,nullptr);
    if(!window)return 3;
    DXGI_SWAP_CHAIN_DESC desc{};desc.BufferCount=2;desc.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;desc.OutputWindow=window;desc.SampleDesc.Count=1;
    desc.Windowed=TRUE;desc.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
    IDXGISwapChain* sc=nullptr;ID3D11Device* device=nullptr;ID3D11DeviceContext* context=nullptr;
    HRESULT hr=D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&desc,&sc,&device,nullptr,&context);
    if(SUCCEEDED(hr)&&MH_Initialize()==MH_OK){
        void** table=*reinterpret_cast<void***>(sc);
        bool ok=MH_CreateHook(table[8],reinterpret_cast<void*>(&hooked),reinterpret_cast<void**>(&original))==MH_OK;
        IDXGISwapChain1* sc1=nullptr;
        if(SUCCEEDED(sc->QueryInterface(__uuidof(IDXGISwapChain1),reinterpret_cast<void**>(&sc1)))){
            void** v1=*reinterpret_cast<void***>(sc1);
            MH_CreateHook(v1[22],reinterpret_cast<void*>(&hooked1),reinterpret_cast<void**>(&original1));sc1->Release();
        }
        if(ok&&MH_EnableHook(MH_ALL_HOOKS)==MH_OK)fp::write(&control->hooked,1);
    }
    if(context)context->Release();if(device)device->Release();if(sc)sc->Release();DestroyWindow(window);
    return fp::read(&control->hooked)?0:4;
}
}
BOOL APIENTRY DllMain(HMODULE module,DWORD reason,LPVOID){
    if(reason==DLL_PROCESS_ATTACH){DisableThreadLibraryCalls(module);
        HANDLE h=CreateThread(nullptr,0,initialize,nullptr,0,nullptr);if(h)CloseHandle(h);}
    // Deliberately never unload a live hook. It stays dormant after disable until game exit.
    return TRUE;
}
