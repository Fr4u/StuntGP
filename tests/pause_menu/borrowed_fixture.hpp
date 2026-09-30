#pragma once
#include "sgn/retail_pause_menu.hpp"
#include <bit>

namespace pause_menu_test {
namespace r=sgn::retail;

// Only absent menu/input storage. Existing GameText/fonts, option/audio banks,
// HUD/vehicle/stream state, FrontendInputBinding and drawing allocations belong
// to the concrete fixture world supplied by the receiver. No Session edits.
struct MenuOwners {
    r::PausePhysicalInputIdentity physical;
    std::uint32_t release{},selection{},highlight{},ready{},language{1};
    std::array<r::PauseMenuRow,8> rows{r::initial_pause_menu_rows()};
    std::array<r::PauseMenuInputEdges,4> edges{};
    r::PauseMenuStateView view(const r::FrontendLaunchState& frontend){
        return {release,selection,highlight,ready,language,rows,frontend.input_bindings,edges};
    }
};

// Explicit codec of the absent source storage, with no copy-back of expected
// state. Source word callbacks remain source identities, not callable pointers.
using MenuWords=std::array<std::uint32_t,54>;
inline MenuWords capture(const MenuOwners& owner){
    MenuWords words{};
    words[0]=owner.physical.token;words[1]=owner.release;words[2]=owner.selection;
    words[3]=owner.highlight;words[4]=owner.ready;words[5]=owner.language;
    for(unsigned i=0;i<8;++i){const auto& row=owner.rows[i];const unsigned at=6+i*6;
        words[at]=row.enabled;words[at+1]=row.text_id;words[at+2]=std::bit_cast<std::uint32_t>(row.x);
        words[at+3]=std::bit_cast<std::uint32_t>(row.y);words[at+4]=row.callback;words[at+5]=row.extra_text_id;}
    return words;
}
inline std::array<std::uint8_t,32> capture_edges(const MenuOwners& owner){
    std::array<std::uint8_t,32> bytes{};
    for(unsigned i=0;i<4;++i){const auto& e=owner.edges[i];const std::array<std::uint8_t,8> row{
        e.up,e.up_previous,e.down,e.down_previous,e.activate,e.activate_previous,e.back,e.back_previous};
        for(unsigned j=0;j<8;++j)bytes[i*8+j]=row[j];}
    return bytes;
}
struct Patch {std::uint32_t word,value;};
inline void apply(MenuOwners& owner,Patch patch){
    if(patch.word>=54)throw sgn::FormatError("pause fixture menu patch index");
    const auto value=patch.value;
    if(patch.word<6){std::array<std::uint32_t*,6> fields{&owner.physical.token,&owner.release,&owner.selection,&owner.highlight,&owner.ready,&owner.language};*fields[patch.word]=value;return;}
    auto& row=owner.rows[(patch.word-6)/6];
    switch((patch.word-6)%6){
    case 0:row.enabled=value;break;case 1:row.text_id=value;break;
    case 2:row.x=std::bit_cast<std::int32_t>(value);break;case 3:row.y=std::bit_cast<std::int32_t>(value);break;
    case 4:row.callback=value;break;case 5:row.extra_text_id=value;break;
    }
}
inline void apply_edge(MenuOwners& owner,unsigned physical_slot,unsigned edge,std::uint8_t value){
    if(physical_slot>=4||edge>=8)throw sgn::FormatError("pause fixture physical edge index");
    auto& e=owner.edges[physical_slot];const std::array<std::uint8_t*,8> fields{
        &e.up,&e.up_previous,&e.down,&e.down_previous,&e.activate,&e.activate_previous,&e.back,&e.back_previous};
    *fields[edge]=value;
}
}
