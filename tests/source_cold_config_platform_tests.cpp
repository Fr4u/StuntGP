// Authored receiving, UNRUN by author. The platform and allocation bank are
// real production owners. File contents below are explicit test fixtures,
// not a recovered retail Game.cfg or proof of full cold startup.
#include "sgn/race_session.hpp"
#include "sgn/source_boot_config.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <filesystem>
#include <iostream>
#include <map>
#include <string>
#include <thread>
#include <vector>

using namespace sgn;
using namespace sgn::retail;
namespace {
using Word=std::uint32_t;
unsigned checks{};
void check(bool value,const char* reason){if(!value)throw std::runtime_error(reason);++checks;}
template<class F> void boundary(F&& action,Word site){
    bool caught=false;
    try{action();}catch(const SourceBootConfigError& error){caught=true;check(error.source_site==site,"wrong source boundary");}
    check(caught,"expected source boundary was not reached");
}
const std::array<std::string,3> paths{"Game.cfg","D:\\Game\\Game.cfg","C:\\Game\\Game.cfg"};
const std::string authored_settings=
    "-GAMESETTINGS = \r\nGAMELOCATION = C:\\Data\r\nCONTROLMETHOD = pAd_anALog\r\n"
    "DISPLAYRESWIDTH = 1920\r\nDISPLAYRESHEIGHT = 1080\r\nDISPLAYRESDEPTH = 24\r\n"
    "SOUNDQUALITY = 44100\r\nMASTERVOLUME = 75\r\nSFXVOLUME = 80\r\nMUSVOLUME = 60\r\n"
    "LEVELSCRIPT = TRACK1\r\nD3D_DEVICEINDEX = 9\r\nD3D_DRIVERINDEX = 2\r\nD3D_UVFIX = -1\r\n"
    "DISABLE_FRAMERATE_CORRECTION = 1\r\nDISPLAY_BOOT_GFX = 1\r\n-END\r\n";
std::string changed(std::string text,std::string_view from,std::string_view to){
    const auto at=text.find(from);check(at!=std::string::npos,"receiving fixture replacement missing");
    text.replace(at,from.size(),to);return text;
}
// Borrowed bank only; this boundary does not allocate or own any source buffer.
struct FixtureFiles final:SourceBootFilePlatform {
    SourceAllocationBank& bank;
    std::map<std::string,std::string> files;
    std::vector<std::string> calls;
    std::uintptr_t last_destination{}; //diagnostic address, never dereferenced
    Word last_requested{},last_reserved{};
    explicit FixtureFiles(SourceAllocationBank& owner):bank(owner){}
    Word size_loose_44af60(std::string_view path) override{
        calls.push_back("S:"+std::string(path));const auto it=files.find(std::string(path));
        return it==files.end()?0u:static_cast<Word>(it->second.size());
    }
    Word read_loose_44afa0(std::string_view path,Word requested,std::span<std::uint8_t> destination,std::size_t& copied) override{
        calls.push_back("R:"+std::string(path)+":"+std::to_string(requested));copied=0;
        last_destination=reinterpret_cast<std::uintptr_t>(destination.data());unsigned matches=0;
        for(const auto& row:bank.source_records())if(row.aligned!=0&&row.aligned==last_destination){
            ++matches;check(row.category==0,"CFG read used persistent bank category");
            check(row.requested_bytes==requested+32u&&destination.size()==row.requested_bytes,"CFG read does not borrow original bank payload");
            check(row.reserved_bytes==row.requested_bytes+36u,"bank overhead changed");
            last_requested=row.requested_bytes;last_reserved=row.reserved_bytes;
        }
        check(matches==1,"CFG read used a separate buffer");
        check(std::all_of(destination.begin(),destination.end(),[](auto byte){return byte==0;}),"bank payload was not zero initialized");
        const auto it=files.find(std::string(path));if(it==files.end())return 0;
        const auto count=std::bit_cast<std::int32_t>(requested)>0?static_cast<std::size_t>(requested):std::size_t{0x200000};
        const auto available=std::min(count,it->second.size());copied=std::min(available,destination.size());
        std::copy_n(it->second.begin(),copied,destination.begin());
        if(available>destination.size())throw SourceBootConfigError(0x44afccu,"fixture reached actual read bound");
        return static_cast<Word>(copied);
    }
    SourceBootRawAllocation allocate_wad_raw(Word) override{
        throw std::runtime_error("this explicit loose-file fixture has no WAD payload");
    }
};
std::array<std::uint64_t,10> audio_snapshot(const RaceSessionState& s){
    const auto& a=s.audio_level;return {s.audio_initialization.requested_rate,a.scale,a.input_a,a.input_b,
        a.output_a,a.output_b,a.output_b_copy,s.options.audio_a,s.options.audio_b,s.audio_initialization.device.value};
}
void no_backends(RaceSessionPlatform& platform){
    const auto h=platform.host_readiness();
    check(!h.assets&&!h.audio_device&&!h.audio_stream&&!h.window_input,"CFG created an unrelated host backend");
}
std::shared_ptr<RaceSessionPlatform> fresh(){
    // Absolute explicit fixture root; no original config is read here.
    return std::make_shared<RaceSessionPlatform>(SourceColdConstructionOnly{},
        std::filesystem::temp_directory_path()/"sgn-authored-cfg-fixture-boundary",0.0f);
}
void complete_paths_and_lifetime(){
    for(std::size_t selected=0;selected<paths.size();++selected){
        auto owner=fresh();auto& s=owner->state();auto& bank=owner->source_allocations();
        check(!bank.initialized(),"platform initialized source bank prematurely");bank.initialize_42eab0();
        const auto anchor=bank.allocate_42e8d0(17,1);bank.bytes(anchor)[0]=0xa5;
        const auto* const cfg=&s.boot_config;const auto* const rng=&s.random;const auto* const time=&s.source_cold_time;
        const auto random=s.random.state;const auto tz=s.source_cold_time.cached_tz.get();
        const auto tz_initialized=s.source_cold_time.timezone_initialized;
        s.boot_config.game_location[250]='Z';s.boot_config.level_script[31]='Q';
        FixtureFiles files(bank);files.files[paths[selected]]=authored_settings;
        std::shared_ptr<RaceSessionState> retained;
        {
            // Two paths execute before Session; the third verifies delegation.
            if(selected!=2)initialize_platform_source_boot_config_42d660(*owner,files);
            RaceSession session(SourceColdAttachmentOnly{},owner);
            if(selected==2)initialize_session_source_boot_config_42d660(session,files);
            retained=session.shared_state();
            check(&session.state()==&s&&session.shared_platform().get()==owner.get(),"Session does not borrow actual platform state");
            check(&session.state().boot_config==cfg,"Session constructed another CFG owner");
        }
        check(retained.get()==&s&&!retained.owner_before(owner)&&!owner.owner_before(retained),"retained state lost platform owner alias");
        std::vector<std::string> expected;
        for(std::size_t i=0;i<=selected;++i)expected.push_back("S:"+paths[i]);
        expected.push_back("S:"+paths[selected]);
        if(selected==0)expected.push_back("S:D:\\Game\\\\wads\\p_single.wad");
        expected.push_back("R:"+paths[selected]+":"+std::to_string(authored_settings.size()));
        check(files.calls==expected,"fallback/reprobe/WAD/loose-read order changed");
        check(s.control_kind==3&&s.source_scheduler.fixed_mode_a==1,"CFG did not write actual control and scheduler owners");
        check(s.boot_config.display_width==1920&&s.boot_config.display_height==1080&&s.boot_config.display_depth==24,"display CFG values");
        check(std::string(s.boot_config.base_path.data())=="C:\\Data\\"&&std::string(s.boot_config.graphics_path.data())=="C:\\Data\\Graphics\\"&&
            std::string(s.boot_config.sound_path.data())=="C:\\Data\\Sound","source path writes changed");
        check(std::string(s.boot_config.level_script.data())=="TRACK1"&&s.boot_config.game_location[250]=='Z'&&s.boot_config.level_script[31]=='Q',"source string tails were reset");
        check(s.boot_config.device_index==9&&s.boot_config.driver_index==2&&s.boot_config.uv_fix==0xffffffffu&&s.boot_config.display_boot_graphics==1,"source optional words");
        const auto& audio=s.audio_level;
        check(s.audio_initialization.requested_rate==44100&&audio.scale==75&&audio.input_b==80&&audio.input_a==60,"460C20 scalar writes");
        check(audio.output_b==153&&audio.output_a==114&&audio.output_b_copy==153&&s.options.audio_b==80&&s.options.audio_a==60,"real 460BC0 scalar update");
        check(&s.random==rng&&s.random.state==random&& &s.source_cold_time==time&&s.source_cold_time.cached_tz.get()==tz&&
            s.source_cold_time.timezone_initialized==tz_initialized,"CFG changed RNG or time owner");
        check(!s.boot_config.files.archive&&s.boot_config.files.archive_size==0,"loose fixture acquired a raw WAD cache");
        check(!s.boot_config.boundary.is_null&&s.boot_config.boundary.offset==authored_settings.find("-END"),"589624 boundary publication");
        bool released=false;try{(void)bank.bytes(s.boot_config.boundary.allocation);}catch(const SourceAllocationError&){released=true;}
        check(released,"successful parser did not source-release CFG buffer");
        check(bank.live_count()==1&&bank.bytes(anchor)[0]==0xa5,"CFG released or replaced unrelated persistent allocation");
        const auto& words=bank.source_words();
        check(words.next_ordinal==2&&words.ordinary_bytes==0&&words.persistent_bytes==53&&words.peak_bytes==authored_settings.size()+121u,"CFG reset bank or changed accounting");
        bool duplicate=false;try{RaceSession second(SourceColdAttachmentOnly{},owner);}catch(const std::runtime_error&){duplicate=true;}
        check(duplicate,"CFG path bypassed single-Session admission");no_backends(*owner);
        auto* const bank_address=&bank;std::weak_ptr<RaceSessionPlatform> weak=owner;
        owner.reset();check(!weak.expired()&& &retained->boot_config==cfg&&bank_address->bytes(anchor)[0]==0xa5,"retained actual world lost CFG/bank lifetime");
        retained.reset();check(weak.expired(),"test leaked a platform owner");
    }
}
void missing_file_retains_32_bytes(){
    auto owner=fresh();auto& s=owner->state();auto& bank=owner->source_allocations();bank.initialize_42eab0();
    s.control_kind=77;s.source_scheduler.fixed_mode_a=88;s.boot_config.display_width=901;
    const auto before=audio_snapshot(s);FixtureFiles files(bank);
    boundary([&]{initialize_platform_source_boot_config_42d660(*owner,files);},0x42c5b2u);
    check(files.calls==std::vector<std::string>{"S:Game.cfg","S:D:\\Game\\Game.cfg","S:C:\\Game\\Game.cfg","S:C:\\Game\\Game.cfg","R:C:\\Game\\Game.cfg:0"},"missing-file source call order");
    check(files.last_requested==32&&files.last_reserved==68&&bank.live_count()==1&&bank.source_words().ordinary_bytes==68,"missing file lost original 32-byte payload");
    check(s.control_kind==77&&s.source_scheduler.fixed_mode_a==88&&s.boot_config.display_width==901&&s.boot_config.boundary.is_null&&audio_snapshot(s)==before,"missing file fabricated defaults or rolled source state forward");
    {RaceSession session(SourceColdAttachmentOnly{},owner);check(&session.state().boot_config==&s.boot_config,"failure changed CFG owner on Session attach");}
    bool found=false;for(const auto& row:bank.source_records())if(row.aligned==files.last_destination&&row.aligned!=0)found=row.requested_bytes==32;
    check(found&&bank.source_words().next_ordinal==1,"Session destruction cleared failed source allocation");no_backends(*owner);
}
void uninitialized_bank_is_rejected(){
    auto owner=fresh();auto& s=owner->state();auto& bank=owner->source_allocations();FixtureFiles files(bank);
    files.files[paths[1]]=authored_settings;const auto before=audio_snapshot(s);bool rejected=false;
    try{initialize_platform_source_boot_config_42d660(*owner,files);}catch(const SourceAllocationError&){rejected=true;}
    check(rejected&&!bank.initialized()&&bank.live_count()==0&&files.last_destination==0,"CFG initialized bank or read before bank admission");
    check(audio_snapshot(s)==before&&s.boot_config.boundary.is_null,"bank rejection changed audio or parsed CFG");no_backends(*owner);
}
void partial_display_write_survives(){
    auto owner=fresh();auto& s=owner->state();auto& bank=owner->source_allocations();bank.initialize_42eab0();
    s.boot_config.display_width=7;s.boot_config.display_height=8;s.boot_config.display_depth=9;
    const auto before=audio_snapshot(s);FixtureFiles files(bank);
    files.files[paths[1]]=changed(authored_settings,"DISPLAYRESDEPTH = 24\r\n","");
    boundary([&]{initialize_platform_source_boot_config_42d660(*owner,files);},0x42d4ceu);
    check(s.boot_config.display_height==1080&&s.boot_config.display_width==7&&s.boot_config.display_depth==9,"42D4CE lost preceding height store or invented unwritten display locals");
    check(s.control_kind==3&&std::string(s.boot_config.base_path.data())=="C:\\Data\\"&&audio_snapshot(s)==before,"parser failure rolled back prior writes or reached audio");
    check(bank.live_count()==1&&bank.bytes(s.boot_config.boundary.allocation).size()==files.files[paths[1]].size()+32u,"partial parse source buffer released early");no_backends(*owner);
}
void scalar_limits_without_backend(){
    for(const bool high:{false,true}){
        auto owner=fresh();auto& s=owner->state();auto& bank=owner->source_allocations();bank.initialize_42eab0();
        s.audio_level.scale=50;s.audio_level.output_b=20;s.audio_level.output_a=40;
        auto data=changed(authored_settings,"SOUNDQUALITY = 44100",high?"SOUNDQUALITY = 96000":"SOUNDQUALITY = 1");
        data=changed(data,"MASTERVOLUME = 75","MASTERVOLUME = 101");
        data=changed(data,"SFXVOLUME = 80","SFXVOLUME = 101");data=changed(data,"MUSVOLUME = 60","MUSVOLUME = 101");
        FixtureFiles files(bank);files.files[paths[1]]=data;initialize_platform_source_boot_config_42d660(*owner,files);
        check(s.audio_initialization.requested_rate==(high?44100u:5512u)&&s.audio_level.scale==50&&s.audio_level.input_b==20&&s.audio_level.input_a==40,"460C20 rate clamp or rejected volume did not retain actual prior scalar");
        check(s.audio_level.output_b==25&&s.audio_level.output_a==51&&s.audio_level.output_b_copy==25&&s.options.audio_b==20&&s.options.audio_a==40,"460BC0 retained scalar calculation");no_backends(*owner);
    }
}
void wrong_thread_has_no_file_or_state_effects(){
    auto owner=fresh();auto& s=owner->state();auto& bank=owner->source_allocations();bank.initialize_42eab0();
    FixtureFiles files(bank);files.files[paths[1]]=authored_settings;RaceSession session(SourceColdAttachmentOnly{},owner);
    const auto before=audio_snapshot(s);unsigned rejected=0;
    std::thread other([&]{
        try{initialize_platform_source_boot_config_42d660(*owner,files);}catch(const std::logic_error&){++rejected;}
        try{initialize_session_source_boot_config_42d660(session,files);}catch(const std::logic_error&){++rejected;}
    });other.join();
    check(rejected==2&&files.calls.empty()&&bank.live_count()==0&&bank.source_words().next_ordinal==0&&audio_snapshot(s)==before&&s.boot_config.boundary.is_null,"wrong-thread CFG binding performed source side effects");no_backends(*owner);
}
void old_boundary_is_not_reset_for_second_read(){
    auto owner=fresh();auto& s=owner->state();auto& bank=owner->source_allocations();bank.initialize_42eab0();
    FixtureFiles first(bank);first.files[paths[1]]=authored_settings;initialize_platform_source_boot_config_42d660(*owner,first);
    const auto old_boundary=s.boot_config.boundary.allocation;const auto before=audio_snapshot(s);
    FixtureFiles second(bank);second.files[paths[1]]=authored_settings;
    boundary([&]{initialize_platform_source_boot_config_42d660(*owner,second);},0x42c59du);
    check(s.boot_config.boundary.allocation==old_boundary&&bank.live_count()==1&&bank.source_words().next_ordinal==2&&
        bank.source_words().ordinary_bytes==authored_settings.size()+68u&&audio_snapshot(s)==before,
        "repeated CFG call reset 589624, hid foreign generation, or released failed allocation");no_backends(*owner);
}
}
int main(){
    try{
        complete_paths_and_lifetime();missing_file_retains_32_bytes();uninitialized_bank_is_rejected();
        partial_display_write_survives();scalar_limits_without_backend();wrong_thread_has_no_file_or_state_effects();old_boundary_is_not_reset_for_second_read();
        std::cout<<"actual platform CFG: 7 groups, "<<checks<<" checks; authored file fixtures only\n";return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
