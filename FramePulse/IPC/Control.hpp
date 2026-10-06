#pragma once
#include <windows.h>
#include <string>
namespace fp {
constexpr LONG protocol=1;
struct Control {
    volatile LONG version;
    volatile LONG enabled;
    volatile LONG milliFps;
    volatile LONG heartbeat;
    volatile LONG hooked;
    volatile LONG presents;
    volatile LONG lastPacedTick;
    volatile LONG64 window;
};
inline std::wstring mappingName(DWORD pid){return L"Local\\FramePulse.Control."+std::to_wstring(pid);}
inline LONG read(volatile LONG* p){return InterlockedCompareExchange(p,0,0);}
inline void write(volatile LONG* p,LONG v){InterlockedExchange(p,v);}
}
