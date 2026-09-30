// Complete actual NEW source bodies, exactly once. Only external OS/COM
// results are scripted; source input, pause, FF, Session and scheduler are real.
#include "fixture_com.hpp"
#include "capture_probe.hpp"
#include "sgn/menu_directinput_startup.hpp"
#include "sgn/menu_keyboard_acquisition.hpp"
HMODULE WINAPI ReceivingLoadLibraryExW(LPCWSTR name,HANDLE file,DWORD flags){
    fixture::active->log.validate(std::wstring(name)==L"dinput.dll"&&!file&&flags==LOAD_LIBRARY_SEARCH_SYSTEM32);
    (void)fixture::active->log.call("host.load");return ::LoadLibraryExW(name,file,flags);
}
FARPROC WINAPI ReceivingGetProcAddress(HMODULE module,LPCSTR name){
    fixture::active->log.validate(module&&std::string(name)=="DirectInputCreateA");
    (void)fixture::active->log.call("host.symbol");
    if(!::GetProcAddress(module,name))return nullptr;
    return std::bit_cast<FARPROC>(&fixture::create);
}
BOOL WINAPI ReceivingFreeLibrary(HMODULE module){
    fixture::active->log.validate(fixture::active->all_released());
    (void)fixture::active->log.call("host.unload");
    const auto result=::FreeLibrary(module);fixture::active->log.validate(result!=FALSE);return result;
}
BOOL WINAPI ReceivingGetKeyboardState(PBYTE destination){
    ++source_outer_receiving::keyboard_calls;
    std::memcpy(destination,source_outer_receiving::keyboard_packet.data(),256);return TRUE;
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
