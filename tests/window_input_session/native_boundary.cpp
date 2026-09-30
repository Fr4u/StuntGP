// Compile the actual integrated implementation bodies exactly once in this
// receiving EXE. Only the factory result is scripted; HWND/process/thread,
// module load/unload and keyboard implementation use actual Windows APIs.
#include "fixture_com.hpp"
#include "allocation_probe.hpp"
#include "sgn/menu_directinput_startup.hpp"
#include "sgn/menu_keyboard_acquisition.hpp"

HMODULE WINAPI ReceivingLoadLibraryExW(LPCWSTR name,HANDLE file,DWORD flags){
    fixture::active->log.validate(std::wstring(name)==L"dinput.dll" && !file && flags==LOAD_LIBRARY_SEARCH_SYSTEM32);
    (void)fixture::active->log.call("host.load");
    return ::LoadLibraryExW(name,file,flags);
}
FARPROC WINAPI ReceivingGetProcAddress(HMODULE module,LPCSTR name){
    fixture::active->log.validate(module && std::string(name)=="DirectInputCreateA");
    (void)fixture::active->log.call("host.symbol");
    if(!::GetProcAddress(module,name))return nullptr;
    return std::bit_cast<FARPROC>(&fixture::create);
}
BOOL WINAPI ReceivingFreeLibrary(HMODULE module){
    fixture::active->log.validate(fixture::active->all_released());
    (void)fixture::active->log.call("host.unload");
    const auto result=::FreeLibrary(module);
    fixture::active->log.validate(result!=FALSE);
    allocation_probe::unloaded=true;
    return result;
}
BOOL WINAPI ReceivingGetKeyboardState(PBYTE destination){
    (void)fixture::active->log.call("host.keyboard");
    return ::GetKeyboardState(destination);
}

#define LoadLibraryExW ReceivingLoadLibraryExW
#define GetProcAddress ReceivingGetProcAddress
#define FreeLibrary ReceivingFreeLibrary
#include SGN_WINDOW_SESSION_STARTUP_SOURCE
#undef LoadLibraryExW
#undef GetProcAddress
#undef FreeLibrary

#define GetKeyboardState ReceivingGetKeyboardState
#include SGN_WINDOW_SESSION_KEYBOARD_SOURCE
#undef GetKeyboardState
