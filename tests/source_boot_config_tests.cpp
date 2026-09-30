// WRITTEN ONLY. No compiler/C++/native/original execution by Session.
#include "sgn/source_boot_config.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <string>
#include <vector>

using namespace sgn::retail;
namespace {
using Word=std::uint32_t;
using Bytes=std::vector<std::uint8_t>;
void require(bool ok,const char* why) { if(!ok) throw std::runtime_error(why); }
template<class F> void fault(F&& action,Word site) {
    bool caught=false;
    try { action(); } catch(const SourceBootConfigError& error) { caught=true; require(error.source_site==site,"wrong boot source boundary"); }
    require(caught,"expected explicit source boundary");
}
struct Audio final:SourceBootAudio {
    std::vector<std::array<Word,4>> calls;
    void configure_460c20(Word a,Word b,Word c,Word d) override { calls.push_back({a,b,c,d}); }
};
struct Files final:SourceBootFilePlatform {
    std::map<std::string,Bytes> contents;
    std::map<std::string,std::vector<Word>> size_replies;
    std::vector<std::string> calls;
    bool malloc_fails{};
    Word size_loose_44af60(std::string_view path) override {
        calls.push_back("S:"+std::string(path));
        auto& replies=size_replies[std::string(path)];
        if(!replies.empty()) { const auto value=replies.front(); replies.erase(replies.begin()); return value; }
        const auto it=contents.find(std::string(path));
        return it==contents.end()?0u:static_cast<Word>(it->second.size());
    }
    Word read_loose_44afa0(std::string_view path,Word requested,std::span<std::uint8_t> out,std::size_t& copied) override {
        calls.push_back("R:"+std::string(path)+":"+std::to_string(requested)); copied=0;
        const auto it=contents.find(std::string(path)); if(it==contents.end()) return 0;
        const std::size_t count=std::bit_cast<std::int32_t>(requested)>0?requested:0x200000u;
        const auto available=std::min(count,it->second.size()); copied=std::min(available,out.size());
        std::copy_n(it->second.begin(),copied,out.begin());
        if(available>out.size()) throw SourceBootConfigError(0x44afccu,"scripted actual overflow boundary");
        return static_cast<Word>(copied);
    }
    SourceBootRawAllocation allocate_wad_raw(Word size) override {
        calls.push_back("M:"+std::to_string(size));
        if(malloc_fails) return {};
        auto pointer=static_cast<std::uint8_t*>(std::malloc(size));
        require(pointer!=nullptr,"fixture allocation failure");
        std::memset(pointer,0xcd,size); //test heap poison, NOT source initializedness
        return SourceBootRawAllocation(pointer);
    }
};
struct World {
    SourceBootConfigState config;
    Word controls{77},correction{88};
    Audio audio;
    SourceBootConfigWorld view() { return {config,controls,correction,audio}; }
};
const std::string settings=
    "-GAMESETTINGS = \r\nGAMELOCATION = C:\\Data\r\nCONTROLMETHOD = pAd_anALog\r\n"
    "DISPLAYRESWIDTH = 1920\r\nDISPLAYRESHEIGHT = 1080\r\nDISPLAYRESDEPTH = 24\r\n"
    "SOUNDQUALITY = 44100\r\nMASTERVOLUME = 75\r\nSFXVOLUME = 80\r\nMUSVOLUME = 60\r\n"
    "LEVELSCRIPT = TRACK1\r\nD3D_DEVICEINDEX = 9\r\nD3D_DRIVERINDEX = 2\r\nD3D_UVFIX = -1\r\n"
    "DISABLE_FRAMERATE_CORRECTION = 1\r\nDISPLAY_BOOT_GFX = 1\r\n-END\r\n";
Bytes bytes(std::string_view value) { return {value.begin(),value.end()}; }
std::string remove_line(std::string input,std::string_view prefix) {
    const auto at=input.find(prefix); require(at!=std::string::npos,"fixture line missing");
    const auto end=input.find('\n',at); input.erase(at,end-at+1); return input;
}
SourceAllocationRef buffer(SourceAllocationBank& bank,std::string_view input) {
    const auto ref=bank.allocate_42e8d0(static_cast<Word>(input.size()+32u),0);
    require(static_cast<bool>(ref),"fixture bank full"); auto payload=bank.bytes(ref);
    std::copy(input.begin(),input.end(),payload.begin()); return ref;
}
void pe_and_complete_parser() {
    World world; SourceAllocationBank bank; bank.initialize_42eab0();
    require(std::string(world.config.base_path.data())=="D:\\Game\\"&&world.config.boundary.is_null,"PE defaults");
    world.config.game_location[250]='Z'; world.config.level_script[31]='Q';
    const auto ref=buffer(bank,settings); apply_source_boot_settings_42d2e0(world.view(),bank,ref);
    require(world.controls==3&&world.correction==1,"actual control/scheduler references");
    require(world.config.display_width==1920&&world.config.display_height==1080&&world.config.display_depth==24,"display words");
    require(std::string(world.config.base_path.data())=="C:\\Data\\"&&std::string(world.config.graphics_path.data())=="C:\\Data\\Graphics\\"&&
        std::string(world.config.sound_path.data())=="C:\\Data\\Sound","literal source path formats");
    require(world.config.game_location[250]=='Z'&&world.config.level_script[31]=='Q',"string tails were reset");
    require(world.config.device_index==9&&world.config.driver_index==2&&world.config.uv_fix==0xffffffffu&&world.config.display_boot_graphics==1,"D3D/default word writers");
    require(world.audio.calls==std::vector<std::array<Word,4>>{{75,80,60,44100}},"460C20 arguments/order");
    require(!world.config.boundary.is_null&&world.config.boundary.allocation==ref&&world.config.boundary.offset==settings.find("-END"),"persistent589624 identity/offset");
}
void fallback_paths_and_reprobe() {
    const std::array<std::string,3> paths{"Game.cfg","D:\\Game\\Game.cfg","C:\\Game\\Game.cfg"};
    for(std::size_t selected=0;selected<paths.size();++selected) {
        World world; SourceAllocationBank bank; bank.initialize_42eab0(); Files files;
        files.contents[paths[selected]]=bytes(settings);
        initialize_source_boot_config_42d660(world.view(),bank,files);
        std::vector<std::string> expected;
        for(std::size_t i=0;i<=selected;++i) expected.push_back("S:"+paths[i]);
        expected.push_back("S:"+paths[selected]);
        if(selected==0) expected.push_back("S:D:\\Game\\\\wads\\p_single.wad"); //empty prefix selects first longest source row
        expected.push_back("R:"+paths[selected]+":"+std::to_string(settings.size()));
        require(files.calls==expected,"cold path probe/reprobe/WAD/READ order");
        require(bank.live_count()==0&&bank.source_words().ordinary_bytes==0,"normal42E980 release");
        require(bank.source_words().peak_bytes==settings.size()+68u,"same bank requested+32+36 accounting");
    }
}
void all_missing_is_not_success() {
    World world; SourceAllocationBank bank; bank.initialize_42eab0(); Files files;
    fault([&]{initialize_source_boot_config_42d660(world.view(),bank,files);},0x42c5b2u);
    require(files.calls==std::vector<std::string>{"S:Game.cfg","S:D:\\Game\\Game.cfg","S:C:\\Game\\Game.cfg","S:C:\\Game\\Game.cfg","R:C:\\Game\\Game.cfg:0"},"missing file control flow");
    require(bank.live_count()==1&&bank.source_words().ordinary_bytes==68,"missing32-byte buffer preserved before unreachable source free");
    require(world.controls==77&&world.correction==88&&world.config.boundary.is_null&&world.audio.calls.empty(),"missing cfg substituted defaults");
}
void reached_unwritten_display_values() {
    { World world; SourceAllocationBank bank; bank.initialize_42eab0();
      const auto ref=buffer(bank,remove_line(settings,"DISPLAYRESWIDTH"));
      fault([&]{apply_source_boot_settings_42d2e0(world.view(),bank,ref);},0x42d4a5u);
      require(world.controls==3&&world.config.display_width==0,"writes before missing width"); }
    { World world; SourceAllocationBank bank; bank.initialize_42eab0();
      world.config.display_width=7;world.config.display_height=8;world.config.display_depth=9;
      const auto ref=buffer(bank,remove_line(settings,"DISPLAYRESDEPTH"));
      fault([&]{apply_source_boot_settings_42d2e0(world.view(),bank,ref);},0x42d4ceu);
      require(world.config.display_height==1080&&world.config.display_width==7&&world.config.display_depth==9,"height store must precede unwritten depth read"); }
    { World world; SourceAllocationBank bank; bank.initialize_42eab0();
      auto script=remove_line(settings,"DISPLAYRESHEIGHT");script.replace(script.find("1920"),4,"319");
      const auto ref=buffer(bank,script); apply_source_boot_settings_42d2e0(world.view(),bank,ref);
      require(world.config.display_width==0&&world.audio.calls.size()==1,"invalid width must skip unwritten height read"); }
}
void failed_search_retains_cursor() {
    World world; SourceAllocationBank bank; bank.initialize_42eab0();
    auto script=remove_line(settings,"SOUNDQUALITY"); script=remove_line(script,"SFXVOLUME");script=remove_line(script,"LEVELSCRIPT");
    const auto ref=buffer(bank,script);apply_source_boot_settings_42d2e0(world.view(),bank,ref);
    require(world.audio.calls==std::vector<std::array<Word,4>>{{75,75,60,24}},"ignored failed searches must reuse last local cursor");
    require(std::string(world.config.level_script.data())=="60","missing LEVELSCRIPT must read retained MUSVOLUME cursor");
}
void literal_search_boundaries_and_defaults() {
    World world; SourceAllocationBank bank; bank.initialize_42eab0();
    auto script=settings; script.insert(script.find("DISPLAYRESWIDTH"),"/* DISPLAYRESWIDTH = 640\r\n");
    for(const auto key:{"D3D_DEVICEINDEX","D3D_DRIVERINDEX","D3D_UVFIX","DISABLE_FRAMERATE_CORRECTION","DISPLAY_BOOT_GFX"}) script=remove_line(script,key);
    script+="D3D_DEVICEINDEX = 99\r\n";
    const auto ref=buffer(bank,script);apply_source_boot_settings_42d2e0(world.view(),bank,ref);
    require(world.config.display_width==640,"source strstr is not comment-aware");
    require(world.config.device_index==0&&world.config.driver_index==1&&world.config.uv_fix==0&&world.correction==0&&world.config.display_boot_graphics==0,"source defaults/END cutoff");
}
void retained_pointer_generation() {
    World world; SourceAllocationBank bank; bank.initialize_42eab0();
    const auto first=buffer(bank,settings);apply_source_boot_settings_42d2e0(world.view(),bank,first);
    (void)bank.release_42e980(first); const auto second=buffer(bank,settings);
    require(!(first==second),"allocator generation contract");
    fault([&]{apply_source_boot_settings_42d2e0(world.view(),bank,second);},0x42c59du);
    require(world.config.boundary.allocation==first,"foreign pointer comparison silently reset589624");
}
void put(Bytes& data,std::size_t at,Word value) { for(unsigned i=0;i<4;++i) data.at(at+i)=static_cast<std::uint8_t>(value>>(i*8u)); }
Bytes archive(std::string_view name,std::string_view payload) {
    const Word table=static_cast<Word>(12u+payload.size()); const Word node=4100;
    Bytes data(static_cast<std::size_t>(table)+node+12u+name.size()+1u);
    put(data,0,0x1a524944);put(data,4,static_cast<Word>(data.size()));put(data,8,table);
    std::copy(payload.begin(),payload.end(),data.begin()+12);
    Word hash=0;for(const unsigned char c:name) hash=((((hash&511u)<<1u)|(hash>>9u))+c)&1023u;
    put(data,table+4u+hash*4u,node);put(data,table+node+4u,12);put(data,table+node+8u,static_cast<Word>(payload.size()));
    std::copy(name.begin(),name.end(),data.begin()+table+node+12u);
    return data;
}
void wad_lookup_and_short_copy() {
    World world; Files files;
    files.contents["D:\\Game\\\\wads\\misc.wad"]=archive("graphics24\\misc\\thing","PAYLOAD");
    const std::string path="D:\\Game\\graphics24\\misc\\thing";
    require(source_boot_file_size_44b3b0(world.config,files,path)==7,"WAD actual DIR lookup size");
    const auto allocation=world.config.files.archive.get(); const auto old_calls=files.calls.size();
    std::array<std::uint8_t,8> destination{};destination.fill(0xee);
    require(source_boot_file_read_44b260(world.config,files,path,3,destination)==7,"archive read must return full member size");
    require(destination[0]=='P'&&destination[1]=='A'&&destination[2]=='Y'&&destination[3]==0xee,"archive min-count/preserved tail");
    require(files.calls.size()==old_calls&&world.config.files.archive.get()==allocation,"retained archive cache reloaded");
    require(std::string(world.config.files.searched_name.data())=="graphics24\\misc\\thing","actual61B758 lookup scratch");
}
void wad_miss_size_and_read_differ() {
    World world; Files files; const std::string path="D:\\Game\\graphics24\\misc\\absent";
    files.contents["D:\\Game\\\\wads\\misc.wad"]=archive("graphics24\\misc\\other","A"); files.contents[path]=bytes("LOOSE");
    require(source_boot_file_size_44b3b0(world.config,files,path)==0,"cached archive size miss incorrectly loose-fell back");
    std::array<std::uint8_t,8> output{};
    require(source_boot_file_read_44b260(world.config,files,path,5,output)==5&&output[0]=='L',"read miss must loose-fallback");
}
void wad_failure_retained_state() {
    const std::string path="D:\\Game\\graphics24\\misc\\thing",wad="D:\\Game\\\\wads\\misc.wad";
    { World world;Files files;files.contents[wad]=archive("graphics24\\misc\\thing","A");files.malloc_fails=true;
      fault([&]{(void)source_boot_file_size_44b3b0(world.config,files,path);},0x44b245u);
      require(!world.config.files.archive&&world.config.files.archive_size==files.contents[wad].size()&&std::string(world.config.files.archive_name.data())=="misc.wad","malloc-null name/pointer/size publications"); }
    { World world;Files files;files.contents[wad]=Bytes(12);put(files.contents[wad],0,0x1a524944);put(files.contents[wad],8,12);
      files.size_replies[wad]={8192};
      fault([&]{(void)source_boot_file_size_44b3b0(world.config,files,path);},0x44b527u);
      require(world.config.files.archive&&world.config.files.archive_size==8192&&world.config.files.readable_prefix==12,"partial WAD initialized-prefix tracking"); }
}
void bank_admission_and_size_overflow() {
    { World world;SourceAllocationBank bank;Files files;files.contents["D:\\Game\\Game.cfg"]=bytes(settings);
      bool caught=false;try{initialize_source_boot_config_42d660(world.view(),bank,files);}catch(const SourceAllocationError&){caught=true;}
      require(caught&&!bank.initialized(),"CFG initialized bank implicitly"); }
    { World world;SourceAllocationBank bank;bank.initialize_42eab0();Files files;files.size_replies["Game.cfg"]={0xffffffffu,0xffffffffu};
      fault([&]{initialize_source_boot_config_42d660(world.view(),bank,files);},0x42d6cau);
      require(bank.live_count()==0&&world.audio.calls.empty(),"overflow reached allocation/parser"); }
}
void size_read_race_keeps_real_allocation() {
    World world;SourceAllocationBank bank;bank.initialize_42eab0();Files files;
    files.size_replies["C:\\Game\\Game.cfg"]={0,0};files.contents["C:\\Game\\Game.cfg"]=Bytes(64,'A');
    fault([&]{initialize_source_boot_config_42d660(world.view(),bank,files);},0x44afccu);
    require(bank.live_count()==1&&bank.source_words().ordinary_bytes==68&&world.config.boundary.is_null,"size/read race synthesized release/parser success");
}
}
int main() {
    try {
        pe_and_complete_parser();fallback_paths_and_reprobe();all_missing_is_not_success();
        reached_unwritten_display_values();failed_search_retains_cursor();literal_search_boundaries_and_defaults();
        retained_pointer_generation();wad_lookup_and_short_copy();wad_miss_size_and_read_differ();
        wad_failure_retained_state();bank_admission_and_size_overflow();size_read_race_keeps_real_allocation();
        std::cout<<"12 source boot-config receiving groups passed\n";return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
