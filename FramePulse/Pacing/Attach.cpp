#include "AntiCheat/Policy.hpp"
#include "IPC/Control.hpp"
#include <iostream>
#include <filesystem>
#include <cwchar>
int wmain(int argc,wchar_t** argv){
    if(argc!=3||std::wcscmp(argv[2],L"--ack-advanced")!=0){
        std::wcerr<<L"Usage: FramePulseAttach PID --ack-advanced\nExplicitly loads experimental DXGI pacing into an approved single-player game.\n";return 2;}
    wchar_t* end=nullptr;unsigned long n=std::wcstoul(argv[1],&end,10);
    if(!n||*end)return 2;DWORD pid=static_cast<DWORD>(n);
    auto exe=fp::lower(std::filesystem::path(fp::processPath(pid)).filename().wstring());
    // Only the first test candidate is enabled, never all single-player games by default.
    if(exe!=L"skyrimse.exe"&&exe!=L"framepulsetestgame.exe"){
        std::wcerr<<L"No approved DXGI adapter for this executable.\n";return 3;}
    wchar_t local[32768];DWORD count=GetEnvironmentVariableW(L"LOCALAPPDATA",local,32768);
    fp::Policy policy;if(count&&count<32768)policy.load(std::filesystem::path(local)/L"FramePulse");
    if(policy.blocked(pid)){std::wcerr<<L"COMPETITIVE / ANTI-CHEAT DETECTED or inspection denied.\n";return 4;}
    HANDLE mapping=OpenFileMappingW(FILE_MAP_READ,FALSE,fp::mappingName(pid).c_str());
    if(!mapping){std::wcerr<<L"Start FramePulse Core and select the running game first.\n";return 5;}CloseHandle(mapping);
    HANDLE process=OpenProcess(PROCESS_CREATE_THREAD|PROCESS_QUERY_LIMITED_INFORMATION|PROCESS_VM_OPERATION|PROCESS_VM_WRITE|PROCESS_VM_READ,FALSE,pid);
    if(!process)return 6;
    USHORT machine=0,native=0;
    if(!IsWow64Process2(process,&machine,&native)||machine!=IMAGE_FILE_MACHINE_UNKNOWN||native!=IMAGE_FILE_MACHINE_AMD64){CloseHandle(process);return 7;}
    wchar_t actualPath[32768];DWORD actualSize=32768;
    if(!QueryFullProcessImageNameW(process,0,actualPath,&actualSize)||fp::lower(std::filesystem::path(actualPath).filename().wstring())!=exe||policy.blocked(pid)){
        CloseHandle(process);return 7;
    }
    wchar_t self[32768];GetModuleFileNameW(nullptr,self,32768);
    auto dll=std::filesystem::path(self).parent_path()/L"FramePulsePacing.dll";
    if(!std::filesystem::is_regular_file(dll)){CloseHandle(process);return 8;}
    auto text=dll.wstring();size_t bytes=(text.size()+1)*sizeof(wchar_t);
    void* remote=VirtualAllocEx(process,nullptr,bytes,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    if(!remote||!WriteProcessMemory(process,remote,text.c_str(),bytes,nullptr)){
        if(remote)VirtualFreeEx(process,remote,0,MEM_RELEASE);CloseHandle(process);return 9;}
    auto localLoad=GetProcAddress(GetModuleHandleW(L"kernel32.dll"),"LoadLibraryW");
    HMODULE owner=nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(localLoad),&owner);
    wchar_t ownerPath[32768];GetModuleFileNameW(owner,ownerPath,32768);
    auto ownerName=fp::lower(std::filesystem::path(ownerPath).filename().wstring());
    uintptr_t remoteBase=0;
    HANDLE modules=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE,pid);
    if(modules!=INVALID_HANDLE_VALUE){MODULEENTRY32W module{sizeof(module)};
        if(Module32FirstW(modules,&module))do{
            if(fp::lower(module.szModule)==ownerName){remoteBase=reinterpret_cast<uintptr_t>(module.modBaseAddr);break;}
        }while(Module32NextW(modules,&module));CloseHandle(modules);}
    if(!owner||!localLoad||!remoteBase){VirtualFreeEx(process,remote,0,MEM_RELEASE);CloseHandle(process);return 10;}
    auto load=reinterpret_cast<LPTHREAD_START_ROUTINE>(remoteBase+reinterpret_cast<uintptr_t>(localLoad)-reinterpret_cast<uintptr_t>(owner));
    HANDLE thread=CreateRemoteThread(process,nullptr,0,load,remote,0,nullptr);
    if(!thread){VirtualFreeEx(process,remote,0,MEM_RELEASE);CloseHandle(process);return 10;}
    DWORD waited=WaitForSingleObject(thread,10000),result=0;
    if(waited==WAIT_OBJECT_0){GetExitCodeThread(thread,&result);VirtualFreeEx(process,remote,0,MEM_RELEASE);}
    // On timeout remote memory must remain allocated until loader thread ends.
    CloseHandle(thread);CloseHandle(process);
    if(waited!=WAIT_OBJECT_0||!result){std::wcerr<<L"Load failed or timed out. No retry performed.\n";return 11;}
    std::wcout<<L"DLL loaded. Wait for Core to report PACING ACTIVE; this is not a compatibility certification.\n";
    return 0;
}
