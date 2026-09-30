// Authored receiving; compilation and native execution belong to Main.
// Platform, GameTextState and AssetStore are production owners. Snapshot
// structs are diagnostic values only. Synthetic WADs are labeled fixtures.
#include "sgn/race_session.hpp"
#include "sgn/source_cold_language.hpp"
#include "sgn/source_allocation_bank.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using namespace sgn;
using namespace sgn::retail;
namespace {
unsigned checks{};
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
template<class F> std::string failure(F&& call){
    try{call();}catch(const FormatError& e){return e.what();}
    throw std::runtime_error("expected explicit asset/loader failure");
}
struct TextSnapshot {
    std::array<char,GameTextState::arena_size> arena;
    std::array<std::optional<std::uint32_t>,105> handles;
    std::array<std::uint32_t,105> descriptors;
    std::uint32_t budget,language;
    bool operator==(const TextSnapshot&) const=default;
};
TextSnapshot snapshot(const GameTextState& s){return {s.arena,s.offsets,s.descriptor_order,s.remaining_bytes,s.language};}
void reset_prefix(const GameTextState& s){
    check(std::ranges::all_of(s.offsets,[](const auto& offset){return !offset;}),"445D8C must clear all105 handles");
    for(std::size_t i=0;i<104;++i)check(s.descriptor_order[i]==i,"445D94 must restore all source descriptors");
    check(s.descriptor_order[104]==0xffffffffu,"445D10 descriptor sentinel");
}
void retained_storage(const GameTextState& s,const TextSnapshot& before){
    check(s.arena==before.arena&&s.remaining_bytes==before.budget&&s.language==before.language,"failed asset borrow/read changed retained arena, budget or current language");
}
std::shared_ptr<RaceSessionPlatform> cold(const std::filesystem::path& root){
    return std::make_shared<RaceSessionPlatform>(SourceColdConstructionOnly{},root,0.0f);
}
void readiness(RaceSessionPlatform& p,bool assets){
    const auto h=p.host_readiness();
    check(h.assets==assets&&!h.audio_device&&!h.audio_stream&&!h.window_input,"language stage changed unrelated host readiness");
    check(!p.source_allocations().initialized()&&p.source_allocations().live_count()==0,"language stage initialized or populated source bank");
}
void pe_text_only(RaceSessionPlatform& p){
    const auto& s=p.state();const auto& t=s.game_text;
    check(s.menu.requested_language==1&&t.language==0&&t.remaining_bytes==0x3000,"pinned PE language words changed or language was auto-selected");
    check(std::ranges::all_of(t.arena,[](char c){return c==0;})&&
        std::ranges::all_of(t.offsets,[](const auto& o){return !o;})&&
        std::ranges::all_of(t.descriptor_order,[](auto d){return d==0;}),"cold world called445D10 or loaded text instead of PeImageOnly construction");
}
void poison(GameTextState& t){
    t.arena.fill('Q');t.offsets.fill(0xfffffff0u);t.descriptor_order.fill(0x12345678u);
    t.remaining_bytes=200;t.language=3;
}
void write_word(Bytes& bytes,std::size_t at,std::size_t value){
    check(value<=std::numeric_limits<std::uint32_t>::max(),"fixture word overflow");
    for(unsigned i=0;i<4;++i)bytes.at(at+i)=static_cast<std::uint8_t>(value>>(i*8u));
}
// NEW authored DIR fixture. Real Archive and AssetStore perform the read.
std::filesystem::path fixture(const std::filesystem::path& root,std::string_view label,std::optional<std::string_view> csv){
    const auto directory=root/std::string(label);check(std::filesystem::create_directory(directory),"fixture directory must be new");
    check(std::filesystem::create_directory(directory/"wads"),"fixture WAD directory must be new");
    std::vector<std::pair<std::string,std::string>> entries{{"Config\\lvscript.cfg","\n"}};
    if(csv)entries.emplace_back("Config\\GameText.csv",*csv);
    Bytes bytes(12);std::vector<std::size_t> offsets;
    for(const auto& [name,payload]:entries){(void)name;offsets.push_back(bytes.size());bytes.insert(bytes.end(),payload.begin(),payload.end());}
    const auto table=bytes.size();bytes.resize(table+4100);std::array<std::size_t,1024> heads{};
    for(std::size_t i=0;i<entries.size();++i){
        const auto& [name,payload]=entries[i];const auto node=bytes.size(),relative=node-table;const auto bucket=name_hash(name);
        bytes.resize(node+12+name.size()+1);write_word(bytes,node,heads[bucket]);write_word(bytes,node+4,offsets[i]);write_word(bytes,node+8,payload.size());
        std::copy(name.begin(),name.end(),bytes.begin()+static_cast<std::ptrdiff_t>(node+12));
        heads[bucket]=relative;write_word(bytes,table+4+static_cast<std::size_t>(bucket)*4,relative);
    }
    write_word(bytes,0,0x1a524944u);write_word(bytes,4,bytes.size());write_word(bytes,8,table);
    std::ofstream output(directory/"wads"/"authored-language.wad",std::ios::binary);
    check(bool(output.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()))),"fixture write failed");
    output.close();check(!output.fail(),"fixture close failed");return directory;
}
constexpr std::string_view csv="text_race_results,A1,A2,A3,A4,A5,A6,A7\ntext_pause_continue,B1,B2,B3,B4,B5,B6,B7\n";

void equal_unprepared_and_thread(const std::filesystem::path& root){
    auto p=cold(root/"intentionally-unprepared");pe_text_only(*p);auto& s=p->state();
    const auto random=s.random.state;
    for(const auto value:std::array<std::uint32_t,4>{0u,0xffffffffu,8u,7u}){
        poison(s.game_text);s.game_text.language=value;set_session_source_language_445f70(*p,value);
        const auto before=snapshot(s.game_text);refresh_session_source_language_445f80(*p);
        check(snapshot(s.game_text)==before&&s.menu.requested_language==value,"equal language performed asset validation or source writes");readiness(*p,false);
        unsigned rejected=0;bool unexpected=false;
        std::thread other([&]{
            try{refresh_session_source_language_445f80(*p);}catch(const std::logic_error&){++rejected;}catch(...){unexpected=true;}
            try{set_session_source_language_445f70(*p,0x11223344u);}catch(const std::logic_error&){++rejected;}catch(...){unexpected=true;}
        });other.join();
        check(!unexpected&&rejected==2&&snapshot(s.game_text)==before&&s.menu.requested_language==value,"equal foreign-thread path bypassed owner check or wrote state");
    }
    check(s.random.state==random,"language equality consumed RNG");
}
void unequal_unprepared_prefix(const std::filesystem::path& root){
    auto p=cold(root/"intentionally-unprepared-unequal");auto& s=p->state();const auto random=s.random.state;
    for(const auto requested:std::array<std::uint32_t,2>{1u,0xffffffffu}){
        poison(s.game_text);set_session_source_language_445f70(*p,requested);const auto before=snapshot(s.game_text);
        const auto why=failure([&]{refresh_session_source_language_445f80(*p);});
        check(why.find("assets have not been prepared")!=std::string::npos,"unprepared host borrow replaced by a fallback or language preflight");
        reset_prefix(s.game_text);retained_storage(s.game_text,before);
        check(s.menu.requested_language==requested&&s.random.state==random,"failed delayed borrow changed request/RNG");readiness(*p,false);
    }
    poison(s.game_text);set_session_source_language_445f70(*p,2);const auto before=snapshot(s.game_text);bool rejected=false;
    std::thread other([&]{try{refresh_session_source_language_445f80(*p);}catch(const std::logic_error&){rejected=true;}});other.join();
    check(rejected&&snapshot(s.game_text)==before&&s.menu.requested_language==2,"foreign unequal call reached reset prefix");
}
void successful_switches_lifetime(const std::filesystem::path& root){
    auto p=cold(fixture(root,"switches",csv));pe_text_only(*p);auto& s=p->state();auto* const text=&s.game_text;
    const auto initial=snapshot(*text);const auto random=s.random.state;p->prepare_host_assets();
    auto* const assets=&p->assets();check(snapshot(*text)==initial,"explicit host AssetStore preparation loaded language");
    const auto payload=assets->resource("config/gametext.csv");std::shared_ptr<RaceSessionState> retained;
    for(std::uint32_t language=1;language<=7;++language){
        set_session_source_language_445f70(*p,language);refresh_session_source_language_445f80(*p);
        check(s.menu.requested_language==language&&text->language==language&&text->remaining_bytes==0x3000-language*6u,"same arena/request/current owners on successful switch");
        check(text->required(GameTextId::race_results)==std::string("A")+std::to_string(language)&&
            text->required(GameTextId::pause_continue)==std::string("B")+std::to_string(language),"requested authored column not loaded");
        check(text->text(1)->data()==text->arena.data()+*text->offsets[1],"text view does not use actual world arena");
        const auto loaded=snapshot(*text);refresh_session_source_language_445f80(*p);check(snapshot(*text)==loaded,"equal loaded request reloaded text");
        if(language==2){
            {RaceSession session(SourceColdAttachmentOnly{},p);retained=session.shared_state();
             check(&session.state().game_text==text&&session.shared_platform().get()==p.get(),"Session cloned language world");}
            check(snapshot(*text)==loaded,"Session attachment/destruction reset language state");
        }
    }
    check(&p->assets()==assets&&assets->resource("Config\\GameText.csv").data()==payload.data()&&s.random.state==random,"language replaced AssetStore or consumed RNG");
    readiness(*p,true);std::weak_ptr<RaceSessionPlatform> weak=p;p.reset();
    check(!weak.expired()&& &retained->game_text==text&&text->required(GameTextId::race_results)=="A7"&&
        assets->resource("config\\gametext.csv").data()==payload.data(),"retained Session world lost text/AssetStore lifetime");
    retained.reset();check(weak.expired(),"receiving leaked platform ownership");
}
void missing_empty_prefix_and_attachment(const std::filesystem::path& root){
    for(const bool empty:{false,true}){
        auto p=cold(fixture(root,empty?"empty":"missing",empty?std::optional<std::string_view>{""}:std::nullopt));
        auto& s=p->state();poison(s.game_text);set_session_source_language_445f70(*p,2);p->prepare_host_assets();
        const auto before=snapshot(s.game_text);const auto* const assets=&p->assets();
        for(unsigned attempt=0;attempt<2;++attempt){
            const auto why=failure([&]{refresh_session_source_language_445f80(*p);});
            check(why.find(empty?"445B95":"missing original resource")!=std::string::npos,"missing/empty failure became a success or another admission");
            reset_prefix(s.game_text);retained_storage(s.game_text,before);check(s.menu.requested_language==2,"failure rewrote request");
        }
        const auto failed=snapshot(s.game_text);
        {RaceSession session(SourceColdAttachmentOnly{},p);check(&session.state().game_text==&s.game_text,"failure Session attachment copied text owner");}
        check(snapshot(s.game_text)==failed&& &p->assets()==assets,"Session destroyed failed source prefix or replaced assets");
        set_session_source_language_445f70(*p,s.game_text.language);refresh_session_source_language_445f80(*p);
        check(snapshot(s.game_text)==failed,"equal request retried missing resource after failure");readiness(*p,true);
    }
}
void partial_failure_and_retry(const std::filesystem::path& root){
    auto p=cold(fixture(root,"partial","text_race_results,EN,PL\ntext_pause_continue,old,\"unfinished"));
    auto& s=p->state();poison(s.game_text);s.game_text.language=1;s.game_text.remaining_bytes=100;
    set_session_source_language_445f70(*p,2);p->prepare_host_assets();
    for(std::uint32_t attempt=1;attempt<=2;++attempt){
        const auto before=snapshot(s.game_text);const auto why=failure([&]{refresh_session_source_language_445f80(*p);});
        check(why.find("unterminated game text value")!=std::string::npos,"wrong partial-parser failure");
        const auto at=100-attempt*3u;
        check(s.game_text.remaining_bytes==at&&s.game_text.language==1&&s.menu.requested_language==2&&s.game_text.offsets[1]==at,"partial failure lost first source allocation or published language");
        auto expected=before.arena;expected[at]='P';expected[at+1]='L';expected[at+2]=0;
        check(s.game_text.arena==expected&&s.game_text.required(GameTextId::race_results)=="PL","partial parse changed unreached arena bytes");
        for(std::size_t i=0;i<105;++i)if(i!=1)check(!s.game_text.offsets[i],"failed loader preserved stale handle");
        for(std::size_t i=0;i<103;++i)check(s.game_text.descriptor_order[i]==i+1,"first descriptor removal lost");
        check(s.game_text.descriptor_order[103]==103&&s.game_text.descriptor_order[104]==0xffffffffu,"descriptor tail/sentinel changed");
    }
    readiness(*p,true);
}
void exhausted_arena_no_equal_retry(const std::filesystem::path& root){
    auto p=cold(fixture(root,"exhausted",csv));auto& s=p->state();s.game_text.remaining_bytes=3;
    set_session_source_language_445f70(*p,2);p->prepare_host_assets();refresh_session_source_language_445f80(*p);
    check(s.game_text.language==2&&s.game_text.remaining_bytes==0xfffffffdu&&s.game_text.offsets[1]==0u&&
        s.game_text.required(GameTextId::race_results)=="A2"&&!s.game_text.offsets[8],"negative arena budget was replaced or successful final language store omitted");
    const auto before=snapshot(s.game_text);refresh_session_source_language_445f80(*p);
    check(snapshot(s.game_text)==before,"equal language replenished exhausted arena or retried null handles");readiness(*p,true);
}
void actual_retail_assets(const std::filesystem::path& root){
    // Exact bytes below independently read from pinned NEW config.wad member
    // config\gametext.csv (SHA-256 a04d4083...972eb9b); not translated fixtures.
    const std::array<std::uint32_t,2> languages{2,7};
    const std::array<std::string_view,2> results{"RACERESULTAAT","RESULTATS DE LA COURSE"};
    const std::array<std::string_view,2> continuation{"* HERVATTEN","* CONTINUER"};
    for(std::size_t i=0;i<languages.size();++i){
        auto p=cold(root);pe_text_only(*p);auto& s=p->state();const auto before=snapshot(s.game_text);const auto random=s.random.state;
        auto* const text=&s.game_text;set_session_source_language_445f70(*p,languages[i]);p->prepare_host_assets();
        auto* const assets=&p->assets();check(snapshot(*text)==before,"retail AssetStore preparation loaded source language");
        const auto data=assets->resource("Config\\GameText.csv");check(data.size()==13932,"retail receiving input member size drift");
        const auto* archive=assets->archive("config.wad");check(archive!=nullptr,"real config.wad archive absent");bool same_payload=false;
        for(const auto& entry:archive->entries())if(canonical(entry.name)=="config\\gametext.csv")
            same_payload=archive->data(entry).data()==data.data()&&archive->data(entry).size()==data.size();
        check(same_payload,"AssetStore text payload does not borrow actual config.wad archive");
        refresh_session_source_language_445f80(*p);check(text->language==languages[i]&&s.menu.requested_language==languages[i],"actual retail language request/current mismatch");
        check(text->required(GameTextId::race_results)==results[i]&&text->required(GameTextId::pause_continue)==continuation[i],"actual retail CSV column bytes mismatch");
        check(text->remaining_bytes<GameTextState::arena_size&&text->text(1)->data()==text->arena.data()+*text->offsets[1],"actual retail text arena storage/debit mismatch");
        const auto loaded=snapshot(*text);std::shared_ptr<RaceSessionState> retained;
        {RaceSession session(SourceColdAttachmentOnly{},p);retained=session.shared_state();check(&session.state().game_text==text,"actual retail Session duplicated text");}
        check(snapshot(*text)==loaded&&s.random.state==random,"actual retail Session attachment changed text/RNG");
        refresh_session_source_language_445f80(*p);check(snapshot(*text)==loaded,"equal actual retail language reloaded");readiness(*p,true);
        std::weak_ptr<RaceSessionPlatform> weak=p;p.reset();
        check(!weak.expired()&& &retained->game_text==text&&text->required(GameTextId::race_results)==results[i]&&
            assets->resource("Config\\GameText.csv").data()==data.data(),"actual retail retained platform lifetime failed");
        retained.reset();check(weak.expired(),"actual retail receiving leaked platform owner");
    }
}
}
int main(int argc,char** argv){
    try{
        if(argc!=3)throw std::runtime_error("expected --fixtures writable-root or --retail actual-retail-root");
        const std::string_view mode=argv[1];const auto root=std::filesystem::absolute(argv[2]);
        if(mode=="--retail")actual_retail_assets(root);
        else if(mode=="--fixtures"){
            std::filesystem::create_directories(root);
            const auto run=root/std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
            check(std::filesystem::create_directory(run),"receiving run directory must be new");
            equal_unprepared_and_thread(run);unequal_unprepared_prefix(run);successful_switches_lifetime(run);
            missing_empty_prefix_and_attachment(run);partial_failure_and_retry(run);exhausted_arena_no_equal_retry(run);
        }else throw std::runtime_error("unknown receiving mode");
        std::cout<<"actual persistent-platform language "<<mode<<": "<<checks<<" checks PASS\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
