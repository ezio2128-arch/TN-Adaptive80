#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <string>
using Microsoft::WRL::ComPtr;
double capacity=200;
LRESULT CALLBACK windowProc(HWND w,UINT message,WPARAM key,LPARAM p){
    if(message==WM_DESTROY){PostQuitMessage(0);return 0;}
    if(message==WM_KEYDOWN){if(key=='1')capacity=120;if(key=='2')capacity=80;if(key=='3')capacity=200;if(key==VK_ESCAPE)DestroyWindow(w);}
    return DefWindowProcW(w,message,key,p);
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int show){
    WNDCLASSW c{};c.hInstance=instance;c.lpszClassName=L"FramePulse.TestGame";c.lpfnWndProc=windowProc;RegisterClassW(&c);
    HWND window=CreateWindowW(c.lpszClassName,L"FramePulseTestGame — 1:120 / 2:80 / 3:200 FPS capacity",WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,CW_USEDEFAULT,960,600,nullptr,nullptr,instance,nullptr);
    DXGI_SWAP_CHAIN_DESC desc{};desc.BufferCount=2;desc.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;desc.OutputWindow=window;desc.SampleDesc.Count=1;
    desc.Windowed=TRUE;desc.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
    ComPtr<IDXGISwapChain> swap;ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
    if(FAILED(D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,
        &desc,&swap,&device,nullptr,&context)))return 1;
    ComPtr<ID3D11Texture2D> buffer;swap->GetBuffer(0,IID_PPV_ARGS(&buffer));
    ComPtr<ID3D11RenderTargetView> view;if(FAILED(device->CreateRenderTargetView(buffer.Get(),nullptr,&view)))return 2;
    ShowWindow(window,show);MSG message{};LARGE_INTEGER freq;QueryPerformanceFrequency(&freq);
    while(message.message!=WM_QUIT){
        while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}
        if(message.message==WM_QUIT)break;
        LARGE_INTEGER begin,q;QueryPerformanceCounter(&begin);
        // Synthetic workload, intentionally not a benchmark of real GPU work.
        Sleep(static_cast<DWORD>(1000/capacity));
        do{QueryPerformanceCounter(&q);}while(double(q.QuadPart-begin.QuadPart)/freq.QuadPart<1/capacity);
        float color[]={.03f,.10f,.12f,1};context->ClearRenderTargetView(view.Get(),color);
        swap->Present(0,0);
    }
    return 0;
}
