#include "source_audio_checkpoint_receiving.hpp"
#include "sgn/race_session.hpp"
#include "sgn/session_window_input.hpp"
#include "sgn/source_boot_config.hpp"
#include <windows.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {
using namespace sgn::retail;
namespace fs=std::filesystem;
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
// Exact356-byte authored CFG already qualified in source-boot-config-native-receiving-v1.
// This is diagnostic input, not a recovered retail Game.cfg or a game default.
constexpr std::string_view authored_settings=
    "-GAMESETTINGS = \r\nGAMELOCATION = fixture\r\nCONTROLMETHOD = Pad_Analog\r\n"
    "DISPLAYRESWIDTH = 1920\r\nDISPLAYRESHEIGHT = 1080\r\nDISPLAYRESDEPTH = 24\r\n"
    "SOUNDQUALITY = 44100\r\nMASTERVOLUME = 75\r\nSFXVOLUME = 80\r\nMUSVOLUME = 60\r\n"
    "LEVELSCRIPT = TRACK1\r\nD3D_DEVICEINDEX = 9\r\nD3D_DRIVERINDEX = 2\r\nD3D_UVFIX = -1\r\n"
    "DISABLE_FRAMERATE_CORRECTION = 1\r\nDISPLAY_BOOT_GFX = 1\r\n-END\r\n";
static_assert(authored_settings.size()==356);
struct DiagnosticWindow {
    HINSTANCE instance=::GetModuleHandleW(nullptr);
    HWND window{};
    static constexpr const wchar_t* class_name=L"StuntGPSourceAudioCheckpointReceiving";
    static LRESULT CALLBACK procedure(HWND hwnd,UINT message,WPARAM wparam,LPARAM lparam){
        return ::DefWindowProcW(hwnd,message,wparam,lparam);
    }
    DiagnosticWindow(){
        WNDCLASSW wc{};wc.lpfnWndProc=procedure;wc.hInstance=instance;wc.lpszClassName=class_name;
        require(::RegisterClassW(&wc)!=0,"checkpoint window class registration failed");
        window=::CreateWindowExW(0,class_name,L"Stunt GP NEW: source audio initialization diagnostic",
            WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,720,180,nullptr,nullptr,instance,nullptr);
        if(!window){(void)::UnregisterClassW(class_name,instance);throw std::runtime_error("checkpoint window creation failed");}
        ::ShowWindow(window,SW_SHOWNORMAL);(void)::UpdateWindow(window);
    }
    ~DiagnosticWindow(){if(window)(void)::DestroyWindow(window);(void)::UnregisterClassW(class_name,instance);}
    DiagnosticWindow(const DiagnosticWindow&)=delete;
    DiagnosticWindow& operator=(const DiagnosticWindow&)=delete;
    InputWindowHandles handles()const{return {reinterpret_cast<std::uintptr_t>(instance),reinterpret_cast<std::uintptr_t>(window)};}
};
}
int wmain(int argc,wchar_t** argv){try{
    if(argc!=5||std::wstring_view(argv[1])!=L"--retail-root"||std::wstring_view(argv[3])!=L"--output"){
        std::cerr<<"usage: source_audio_checkpoint_native_receiving --retail-root <absolute NEW root> --output <fresh absolute directory>\n";return 2;
    }
    const fs::path retail(argv[2]),output(argv[4]);
    require(retail.is_absolute()&&fs::is_directory(retail)&&output.is_absolute()&&
        fs::is_directory(output.parent_path())&&!fs::exists(output),"explicit retail root and fresh output directory required");
    require(fs::create_directory(output),"cannot create fresh checkpoint output directory");
    {
        std::ofstream cfg(output/L"Game.cfg",std::ios::binary);
        cfg.write(authored_settings.data(),static_cast<std::streamsize>(authored_settings.size()));
        cfg.close();require(bool(cfg),"cannot write authored checkpoint CFG");
    }
    std::ofstream log(output/L"native-audio-chain.log",std::ios::binary);
    require(bool(log),"cannot create checkpoint log");log<<std::unitbuf;
    DiagnosticWindow window; //outlives every Session, alias, native input and sound owner
    receiving::AudioCheckpointObservation result;
    {
        auto platform=std::make_shared<RaceSessionPlatform>(SourceColdConstructionOnly{},retail);
        auto& world=platform->state();auto& bank=platform->source_allocations();
        require(!bank.initialized(),"cold bank unexpectedly initialized");bank.initialize_42eab0();
        // Same explicit authored pre-CFG path fixture as prior CFG receiving.
        // Prevents the admitted source WAD probe from accessing an ambient D: root.
        constexpr std::string_view fixture_root="fixture\\";
        std::fill(world.boot_config.base_path.begin(),world.boot_config.base_path.end(),char{});
        std::copy(fixture_root.begin(),fixture_root.end(),world.boot_config.base_path.begin());
        NativeSourceBootFilePlatform files(output);
        initialize_platform_source_boot_config_42d660(*platform,files);
        require(world.audio_level.input_b==80&&world.audio_initialization.requested_rate==44100&&
            world.audio_level.scale==75&&world.audio_level.output_b==153&&bank.live_count()==0,
            "actual CFG did not configure/release the same world and bank");
        RaceSession session(SourceColdAttachmentOnly{},platform);
        SessionWindowInput input(platform,session.shared_state(),window.handles());
        result=receiving::receive_source_audio_checkpoint(session,window.handles(),log);
    } //same thread: input/session/aliases -> platform -> primary -> device; HWND still alive
    require(::IsWindow(window.window)!=0,"native owner teardown destroyed supplied window");
    log<<"SOURCE_OWNERS_DESTROYED_BEFORE_WINDOW\n";log.close();require(bool(log),"checkpoint log close failed");
    std::ofstream status(output/L"RESULT.txt",std::ios::binary);
    const char* label=result.primary_format_completed?"SOURCE_AUDIO_CHECKPOINT_461CF0_COMPLETED":"SOURCE_AUDIO_CHECKPOINT_STOPPED";
    status<<label<<"\nAuthored CFG. Actual system DirectSound. Full462090, listener, samples and gameplay audio NOT qualified.\n";
    status.close();require(bool(status),"checkpoint result write failed");std::cout<<label<<'\n';
    return result.primary_format_completed?0:77;
}catch(const SourceBootConfigError& error){std::cerr<<"CFG source_site="<<error.source_site<<' '<<error.what()<<'\n';return 1;}
catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
catch(...){std::cerr<<"unexpected checkpoint exception\n";return 1;}}
