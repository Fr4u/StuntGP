#include "sgn/source_cold_language.hpp"
#include "sgn/retail_game_text.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <numeric>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
namespace r=sgn::retail;
void expect(bool value,const char* why){if(!value)throw std::runtime_error(why);}
template<class F> std::string rejects(F&& operation){
    try{operation();}catch(const sgn::FormatError& error){return error.what();}
    throw std::runtime_error("expected a named bounded loader failure");
}
struct TextImage {
    std::array<char,r::GameTextState::arena_size> arena;
    std::array<std::optional<std::uint32_t>,105> offsets;
    std::array<std::uint32_t,105> order;
    std::uint32_t remaining,language;
    bool operator==(const TextImage&) const=default;
};
TextImage snapshot(const r::GameTextState& s){return {s.arena,s.offsets,s.descriptor_order,s.remaining_bytes,s.language};}
void expect_reset(const r::GameTextState& s){
    expect(std::ranges::all_of(s.offsets,[](const auto& value){return !value;}),"all105 handles must be cleared");
    for(std::size_t i=0;i<104;++i)expect(s.descriptor_order[i]==i,"descriptor source order reset");
    expect(s.descriptor_order.back()==std::numeric_limits<std::uint32_t>::max(),"descriptor sentinel retained");
}
void expect_retained_storage(const r::GameTextState& s,const TextImage& before){
    expect(s.arena==before.arena&&s.remaining_bytes==before.remaining&&s.language==before.language,
           "failed acquisition/admission changed retained arena/budget/current language");
}
void u32(sgn::Bytes& bytes,std::size_t at,std::size_t value){
    expect(value<=std::numeric_limits<std::uint32_t>::max(),"fixture word overflow");
    for(unsigned i=0;i<4;++i)bytes.at(at+i)=static_cast<std::uint8_t>(value>>(i*8));
}
//Authored DIR transport fixture, not captured retail content. It is consumed
//by the real Archive and AssetStore; no stub replaces resource() or the loader.
std::filesystem::path fixture(const std::filesystem::path& root,std::string_view label,
                              std::optional<std::string_view> csv){
    const auto directory=root/std::string(label);
    expect(std::filesystem::create_directory(directory),"fixture directory must be new");
    std::filesystem::create_directory(directory/"wads");
    std::vector<std::pair<std::string,std::string>> entries{{"Config\\lvscript.cfg","\n"}};
    if(csv)entries.emplace_back("Config\\GameText.csv",*csv);
    sgn::Bytes bytes(12);std::vector<std::size_t> offsets;
    for(const auto& [name,payload]:entries){
        (void)name;offsets.push_back(bytes.size());bytes.insert(bytes.end(),payload.begin(),payload.end());
    }
    const auto table=bytes.size();bytes.resize(table+4100);
    std::array<std::size_t,1024> heads{};
    for(std::size_t i=0;i<entries.size();++i){
        const auto& [name,payload]=entries[i];
        const auto node=bytes.size(),relative=node-table;
        const auto bucket=sgn::name_hash(name);bytes.resize(node+12+name.size()+1);
        u32(bytes,node,heads[bucket]);u32(bytes,node+4,offsets[i]);u32(bytes,node+8,payload.size());
        std::copy(name.begin(),name.end(),bytes.begin()+static_cast<std::ptrdiff_t>(node+12));
        heads[bucket]=relative;u32(bytes,table+4+static_cast<std::size_t>(bucket)*4,relative);
    }
    u32(bytes,0,0x1a524944u);u32(bytes,4,bytes.size());u32(bytes,8,table);
    std::ofstream file(directory/"wads"/"language-test.wad",std::ios::binary);
    if(!file.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size())))
        throw std::runtime_error("failed to write authored DIR fixture");
    file.close();expect(!file.fail(),"failed to close authored DIR fixture");return directory;
}
constexpr std::string_view complete_csv=
    "text_race_results,A1,A2,A3,A4,A5,A6,A7\n"
    "text_pause_continue,B1,B2,B3,B4,B5,B6,B7\n";

void equal_language_skips_resource_and_validation(const std::filesystem::path& root){
    sgn::AssetStore missing(fixture(root,"equal-missing",std::nullopt));
    for(const auto value:std::array<std::uint32_t,4>{0u,1u,7u,0xffffffffu}){
        r::GameTextState text;text.arena.fill('Q');text.offsets[20]=0xfffffff0u;
        text.descriptor_order.fill(0x12345678u);text.remaining_bytes=0xffffffffu;text.language=value;
        auto requested=value;const auto before=snapshot(text);
        r::refresh_source_language_445f80(missing,requested,text);
        expect(snapshot(text)==before&&requested==value,"equality must bypass resource, validation and writes");
    }
}
void verbatim_request_and_successful_switches(const std::filesystem::path& root){
    sgn::AssetStore assets(fixture(root,"switches",complete_csv));r::GameTextState text;
    expect(text.language==0&&text.remaining_bytes==0x3000,"existing text PE language/budget");
    std::uint32_t requested=1;const auto initial=snapshot(text);
    for(const auto value:std::array<std::uint32_t,4>{0u,8u,0x80000000u,0xffffffffu}){
        r::set_source_language_445f70(requested,value);
        expect(requested==value&&snapshot(text)==initial,"445F70 must store all32bits without loading or clamping");
    }
    for(std::uint32_t language=1;language<=7;++language){
        r::set_source_language_445f70(requested,language);
        r::refresh_source_language_445f80(assets,requested,text);
        expect(text.language==language&&requested==language,"successful requested/current language owners");
        expect(text.required(r::GameTextId::race_results)==std::string("A")+std::to_string(language),"requested CSV column");
        expect(text.required(r::GameTextId::pause_continue)==std::string("B")+std::to_string(language),"second source text ID");
        expect(text.remaining_bytes==0x3000-language*6,"language switch must retain and debit arena budget");
        const auto id=static_cast<std::uint32_t>(r::GameTextId::race_results);
        expect(text.text(id)->data()==text.arena.data()+*text.offsets[id],"text must reference same actual arena");
        const auto loaded=snapshot(text);r::refresh_source_language_445f80(assets,requested,text);
        expect(snapshot(text)==loaded,"same language must not reload or consume budget");
    }
}
void missing_and_empty_resource_keep_prefix_writes(const std::filesystem::path& root){
    sgn::AssetStore valid(fixture(root,"failure-seed",complete_csv));
    sgn::AssetStore missing(fixture(root,"missing",std::nullopt));
    sgn::AssetStore empty(fixture(root,"empty",std::string_view{}));
    for(const auto* assets:std::array<const sgn::AssetStore*,2>{&missing,&empty}){
        r::GameTextState text;r::load_game_text(valid,3,text);const auto before=snapshot(text);
        std::uint32_t requested=2;
        for(unsigned retry=0;retry<2;++retry){
            const auto error=rejects([&]{r::refresh_source_language_445f80(*assets,requested,text);});
            expect_reset(text);expect_retained_storage(text,before);
            expect(requested==2,"failed load must not rewrite requested owner");
            expect(error.find(assets==&missing?"missing original resource":"445B95")!=std::string::npos,
                   "resource failure must not be hidden as a successful language change");
        }
    }
}
void partial_parse_failure_and_retry_retain_consumed_bytes(const std::filesystem::path& root){
    constexpr std::string_view broken="text_race_results,EN,PL\ntext_pause_continue,old,\"unfinished";
    sgn::AssetStore assets(fixture(root,"partial",broken));r::GameTextState text;
    text.arena.fill('Z');text.language=1;text.remaining_bytes=100;text.offsets[9]=0;
    std::uint32_t requested=2;
    for(std::uint32_t retry=1;retry<=2;++retry){
        const auto before=snapshot(text);
        const auto error=rejects([&]{r::refresh_source_language_445f80(assets,requested,text);});
        expect(error.find("unterminated game text value")!=std::string::npos,"expected failure after the first real text row");
        const auto offset=100-retry*3;
        expect(text.remaining_bytes==offset&&text.language==1&&requested==2,"partial failure must retain debit and old current language");
        expect(text.offsets[1]==offset&&text.required(r::GameTextId::race_results)=="PL","first row must survive failure");
        for(std::size_t i=0;i<text.offsets.size();++i)if(i!=1)expect(!text.offsets[i],"old handles must not survive the reset");
        auto expected_arena=before.arena;expected_arena[offset]='P';expected_arena[offset+1]='L';expected_arena[offset+2]=0;
        expect(text.arena==expected_arena,"partial failure changed bytes beyond reached arena stores");
        for(std::size_t i=0;i<103;++i)expect(text.descriptor_order[i]==i+1,"first descriptor removal retained on failure");
        expect(text.descriptor_order[103]==103&&text.descriptor_order[104]==0xffffffffu,"descriptor tail and sentinel retained");
    }
}
void arena_exhaustion_publishes_language_without_retry(const std::filesystem::path& root){
    sgn::AssetStore assets(fixture(root,"exhaustion",complete_csv));r::GameTextState text;
    text.remaining_bytes=3;std::uint32_t requested=2;
    r::refresh_source_language_445f80(assets,requested,text);
    expect(text.language==2&&text.remaining_bytes==0xfffffffdu,"source negative budget still publishes loaded language");
    expect(text.offsets[1]==0u&&text.required(r::GameTextId::race_results)=="A2"&&!text.offsets[8],"arena exhaustion returns null for later text");
    const auto before=snapshot(text);r::refresh_source_language_445f80(assets,requested,text);
    expect(snapshot(text)==before,"equal language must not retry exhausted text handles");
    r::set_source_language_445f70(requested,3);r::refresh_source_language_445f80(assets,requested,text);
    expect(text.language==3&&text.remaining_bytes==0xfffffff7u&&!text.offsets[1]&&!text.offsets[8],"next language preserves negative budget");
}
void asset_admission_and_direct_buffer_contracts(const std::filesystem::path& root){
    sgn::AssetStore assets(fixture(root,"admission",complete_csv));
    sgn::AssetStore missing(fixture(root,"admission-missing",std::nullopt));
    for(const auto invalid:std::array<std::uint32_t,3>{0u,8u,0xffffffffu}){
        r::GameTextState text;r::load_game_text(assets,2,text);const auto before=snapshot(text);
        (void)rejects([&]{r::load_game_text(complete_csv,invalid,text);});
        expect(snapshot(text)==before,"explicit-buffer API preflight contract changed");
        auto requested=invalid;(void)rejects([&]{r::refresh_source_language_445f80(assets,requested,text);});
        expect_reset(text);expect_retained_storage(text,before);
        const auto error=rejects([&]{r::refresh_source_language_445f80(missing,requested,text);});
        expect(error.find("missing original resource")!=std::string::npos,"asset lookup must precede language admission");
    }
    const std::string nul_input("text_race_results,A\0B\n",22);
    sgn::AssetStore nul_assets(fixture(root,"nul-admission",std::string_view(nul_input)));
    r::GameTextState text;r::load_game_text(assets,2,text);const auto before=snapshot(text);std::uint32_t requested=1;
    (void)rejects([&]{r::refresh_source_language_445f80(nul_assets,requested,text);});
    expect_reset(text);expect_retained_storage(text,before);
}
void nonempty_no_rows_publishes_current_language(const std::filesystem::path& root){
    sgn::AssetStore assets(fixture(root,"no-rows",std::string_view("\n")));
    r::GameTextState text;text.remaining_bytes=91;text.arena.fill('R');text.offsets[2]=5;
    const auto arena=text.arena;std::uint32_t requested=7;
    r::refresh_source_language_445f80(assets,requested,text);
    expect_reset(text);expect(text.language==7&&text.remaining_bytes==91&&text.arena==arena,"nonempty zero-token source should publish current language");
}
}
int main(int argc,char** argv)try{
    if(argc!=2)throw std::runtime_error("expected explicit writable authored-fixture root");
    const auto root=std::filesystem::absolute(argv[1])/std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    std::filesystem::create_directories(root.parent_path());
    expect(std::filesystem::create_directory(root),"receiving fixture run directory already exists");
    equal_language_skips_resource_and_validation(root);
    verbatim_request_and_successful_switches(root);
    missing_and_empty_resource_keep_prefix_writes(root);
    partial_parse_failure_and_retry_retain_consumed_bytes(root);
    arena_exhaustion_publishes_language_without_retry(root);
    asset_admission_and_direct_buffer_contracts(root);
    nonempty_no_rows_publishes_current_language(root);
    std::cout<<"source cold language:7 authored AssetStore receiving groups passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
