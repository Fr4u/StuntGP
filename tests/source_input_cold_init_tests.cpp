#include "sgn/source_input_cold_init.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace sgn::retail;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct Fixture {
    SourceInputStorage input;
    SessionMenuInputState menu;
    FrontendLaunchState frontend;
    SourceInputColdWorld world(){return {input,menu,frontend,input.force_feedback};}
};
using Image=std::array<std::uint8_t,1904>;
// Independent receiving serialization: explicit field names, no production
// borrow_physical_bytes, borrow_menu_input_bank or object-layout casts.
Image capture(const Fixture& f){
    Image out{};std::size_t cursor=0;
    const auto byte=[&](std::uint8_t value){out.at(cursor++)=value;};
    const auto word=[&](std::uint32_t value){for(unsigned i=0;i<4;++i)byte(static_cast<std::uint8_t>(value>>(8u*i)));};
    for(std::size_t slot=0;slot<4;++slot){
        const auto& raw=f.input.acquisition.physical[slot];const auto& extra=f.input.physical[slot];
        const auto& edges=f.menu.edges[slot];const auto& other=raw.extra_edges;
        word(f.menu.raw_words[slot].word_0);word(f.menu.raw_words[slot].word_4);
        word(raw.word_8);word(raw.word_0c);word(extra.activity_kind);word(extra.profile_index);
        word(f.frontend.input_bindings[slot].vehicle);word(f.frontend.input_bindings[slot].kind);
        word(extra.feedback_mode);word(raw.word_24);word(raw.word_28);
        for(auto value:raw.words_from_2c)word(value);
        for(auto value:extra.bytes_168_187)byte(value);
        for(auto value:extra.bytes_188_1a7)byte(value);
        byte(other.current_1a8);byte(other.current_1a9);byte(edges.up);byte(edges.down);
        byte(edges.activate);byte(other.current_1ad);byte(edges.back);
        byte(other.previous_1af);byte(other.previous_1b0);byte(edges.up_previous);byte(edges.down_previous);
        byte(edges.activate_previous);byte(other.previous_1b4);byte(edges.back_previous);
        for(auto value:extra.bytes_1b6_1b7)byte(value);
        for(auto value:f.input.force_feedback.physical_words[slot])word(value);
        for(auto value:extra.bytes_1d8_1db)byte(value);
        require(cursor==(slot+1u)*476u,"receiving serializer record width");
    }
    require(cursor==out.size(),"receiving serializer total width");return out;
}
Image expected(){Image out{};for(std::size_t slot=0;slot<4;++slot){out[slot*476u+24u]=255;out[slot*476u+25u]=255;}return out;}
void poison(Fixture& f){
    for(std::size_t slot=0;slot<4;++slot){
        const auto value=0x90807060u+static_cast<std::uint32_t>(slot)*0x01010101u;
        const auto byte=static_cast<std::uint8_t>(0xb0u+slot);
        auto& raw=f.input.acquisition.physical[slot];auto& extra=f.input.physical[slot];
        f.menu.raw_words[slot]={value,value};raw.word_8=value;raw.word_0c=value;raw.word_24=value;raw.word_28=value;
        raw.words_from_2c.fill(value);raw.extra_edges={byte,byte,byte,byte,byte,byte};
        extra.activity_kind=value;extra.profile_index=value;extra.feedback_mode=value;
        extra.bytes_168_187.fill(byte);extra.bytes_188_1a7.fill(byte);
        extra.bytes_1b6_1b7.fill(byte);extra.bytes_1d8_1db.fill(byte);
        f.menu.edges[slot]={byte,byte,byte,byte,byte,byte,byte,byte};
        f.frontend.input_bindings[slot]={value,value};f.input.force_feedback.physical_words[slot].fill(value);
    }
}
void case_fresh_is_not_cold(){
    Fixture f;for(const auto& b:f.frontend.input_bindings)require(b.vehicle==0,"constructor invented cold binding");
    initialize_source_physical_input_45ec30(f.world());
    require(capture(f)==expected(),"fresh cold image differs from exact source");
    for(const auto& b:f.frontend.input_bindings)require(b.vehicle==0x0000ffffu,"binding must be 0000FFFF, not FFFFFFFF");
}
void case_all_four_full_records(){
    Fixture f;poison(f);const auto before=capture(f);
    require(std::none_of(before.begin(),before.end(),[](auto b){return b==0;}),"test did not seed all1904bytes");
    initialize_source_physical_input_45ec30(f.world());
    require(capture(f)==expected(),"nonzero edge/raw/opaque/FF bytes survived cold init");
}
void case_profiles_and_vehicle_profiles_retained(){
    Fixture f;poison(f);
    for(std::size_t i=0;i<f.input.profiles.size();++i)f.input.profiles[i].fill(0x10203040u+static_cast<std::uint32_t>(i));
    for(std::size_t i=0;i<f.input.vehicle_profiles.size();++i)f.input.vehicle_profiles[i].fill(static_cast<std::uint8_t>(50u+i));
    const auto profiles=f.input.profiles;const auto vehicles=f.input.vehicle_profiles;
    initialize_source_physical_input_45ec30(f.world());
    require(f.input.profiles==profiles && f.input.vehicle_profiles==vehicles,"profile or aliased menu selectors reset");
    require(capture(f)==expected(),"profile preservation bypassed physical reset");
}
void case_upstream_and_globals_retained(){
    Fixture f;poison(f);
    for(std::size_t slot=0;slot<10;++slot)f.input.acquisition.upstream[slot].words.fill(0x88776600u+static_cast<std::uint32_t>(slot));
    const auto upstream=f.input.acquisition.upstream;
    f.input.acquisition.source_device_pointer_576b6c=0x576810u;
    f.input.previous_activity=0x99aabbccu;f.menu.previous_pause=0x12345678u;
    f.frontend.setting_60d744=1;f.frontend.track_index=37;f.frontend.prepared=3;f.frontend.catalog_count=48;
    initialize_source_physical_input_45ec30(f.world());
    for(std::size_t slot=0;slot<10;++slot)require(f.input.acquisition.upstream[slot].words==upstream[slot].words,"upstream reset");
    require(f.input.acquisition.source_device_pointer_576b6c==0x576810u,"source device pointer reset");
    require(f.input.previous_activity==0x99aabbccu && f.menu.previous_pause==0x12345678u,"external activity or pause reset");
    require(f.frontend.setting_60d744==1 && f.frontend.track_index==37 && f.frontend.prepared==3 && f.frontend.catalog_count==48,"frontend non-binding state reset");
    require(capture(f)==expected(),"upstream retention bypassed physical reset");
}
void case_shared_ff_physical_only(){
    Fixture f;poison(f);auto& ff=f.input.force_feedback;
    ff.selected_data_4d5b24=0x5732bcu;ff.initialized_576b70=0x89abcdefu;
    ff.direction_576b78={17,29};ff.axes_477640={4,8};ff.axes_count_477648=2;
    ff.gain_47764c=4321;ff.deadband_477650=987;
    for(auto& row:ff.parameter_pointer_kind)row.fill(ForceFeedbackParameterPointer::transient_periodic);
    const auto provenance=ff.parameter_pointer_kind;
    initialize_source_physical_input_45ec30(f.world());
    for(const auto& row:ff.physical_words)for(auto value:row)require(value==0,"same FF physical owner was not cleared");
    require(ff.selected_data_4d5b24==0x5732bcu && ff.initialized_576b70==0x89abcdefu,"FF global reset");
    require(ff.direction_576b78==std::array<std::uint32_t,2>{17,29} && ff.axes_477640==std::array<std::uint32_t,2>{4,8},"FF direction/axes reset");
    require(ff.axes_count_477648==2 && ff.gain_47764c==4321 && ff.deadband_477650==987,"FF tuning reset");
    require(ff.parameter_pointer_kind==provenance,"FF relocation provenance reset");
}
void case_repeat_is_explicitly_destructive(){
    Fixture f;initialize_source_physical_input_45ec30(f.world());
    poison(f);require(capture(f)!=expected(),"receiving rebind seed absent");
    initialize_source_physical_input_45ec30(f.world());
    require(capture(f)==expected(),"hidden once guard skipped explicit second source invocation");
}
void case_another_world_is_untouched(){
    Fixture first,second;poison(first);poison(second);const auto other=capture(second);
    initialize_source_physical_input_45ec30(first.world());
    require(capture(first)==expected(),"selected world unchanged");
    require(capture(second)==other,"physical reset escaped borrowed world");
}
}
int main(){
    using Test=void(*)();
    const std::array<Test,7> cases{case_fresh_is_not_cold,case_all_four_full_records,
        case_profiles_and_vehicle_profiles_retained,case_upstream_and_globals_retained,
        case_shared_ff_physical_only,case_repeat_is_explicitly_destructive,case_another_world_is_untouched};
    try{for(std::size_t i=0;i<cases.size();++i){cases[i]();std::cout<<"PASS cold input case "<<i+1u<<'\n';}}
    catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
    std::cout<<"7 cold input receiving groups passed\n";return 0;
}
