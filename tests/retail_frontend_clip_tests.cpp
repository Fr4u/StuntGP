#include "sgn/retail_frontend_clip.hpp"
#include "sgn/retail_frontend_state.hpp"
#include "sgn/retail_frontend_descriptors.hpp"
#include "sgn/retail_frontend_initialization.hpp"
#include "sgn/retail_world_material_loading.hpp"
#include <algorithm>
#include <array>
#include <cfenv>
#include <iostream>
#include <memory>
#include <stdexcept>

using namespace sgn::retail;
namespace {
unsigned checks=0;
void require(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
constexpr std::array<std::pair<std::uint32_t,std::uint64_t>,28> cases{{
 {0,0},{0x80000000,0},{1,0},{0x80000001,0},{0x007fffff,0},{0x00800000,0},
 {0x3f7fffff,0},{0xbf7fffff,0},{0x3f800000,1},{0xbf800000,0xffffffffffffffffull},
 {0x3fffffff,1},{0xbfffffff,0xffffffffffffffffull},{0x40200000,2},{0xc0200000,0xfffffffffffffffeull},
 {0x4effffff,0x7fffff80ull},{0x4f000000,0x80000000ull},{0x4f7fffff,0xffffff00ull},
 {0x4f800000,0x100000000ull},{0x4f800001,0x100000200ull},{0xcf800001,0xfffffffefffffe00ull},
 {0x5effffff,0x7fffff8000000000ull},{0xdeffffff,0x8000008000000000ull},
 {0x5f000000,0x8000000000000000ull},{0xdf000000,0x8000000000000000ull},
 {0x7f800000,0x8000000000000000ull},{0xff800000,0x8000000000000000ull},
 {0x7fc00001,0x8000000000000000ull},{0xff800001,0x8000000000000000ull}
}};
struct ClipCase{std::array<std::uint32_t,4> input,expected;};
constexpr ClipCase clips[]{
 {{0,0,0x44200000,0x43f00000},{0,0,640,480}},
 {{0xbf000000,0xbe800000,0x44200001,0x43f00001},{0,0,640,480}},
 {{0x3fe00000,0x40200000,0x441fffff,0x43efffff},{1,2,639,479}},
 {{0x44200000,0x43f00000,0x3f800000,0x40000000},{640,480,1,2}},
 {{0x7f800000,0xff800000,0x7f800000,0xff800000},{0,0,640,0}},
 {{0x7fc00001,0xffc00001,0x7f800001,0xff800001},{0,0,0,0}},
 {{0x4f800001,0xcf800001,0xcf800001,0x4f800001},{512,0,0xfffffe00,480}},
 {{0x80000000,1,0xcf000000,0xc0200000},{0,0,0x80000000,0xfffffffe}}
};
void leaf_and_clip(){
    auto f=std::make_unique<FrontendSourceStorage>();
    f->menu_words_5da2e8.fill(0x6ae1832bu);
    f->menu_music_word()=0x413fd378;std::fill(f->page_navigation().begin(),f->page_navigation().end(),std::uint8_t{0xb3});f->menu_counter_5da370=0x8322afed;
    f->menu_words_5df194.fill(0x317845ab);f->lookup_a.fill(37);f->lookup_b.fill(301);f->lookup_c.fill(38);
    const auto original_mode=std::fegetround();require(original_mode!=-1,"read host rounding mode");
    for(const auto mode:{FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO}){
        require(std::fesetround(mode)==0,"set host rounding mode");
        const auto flags=std::fetestexcept(FE_ALL_EXCEPT);
        for(auto [input,expected]:cases)require(frontend_ftol_f32_462860(input)==expected,"literal signed64 result bits");
        for(const auto& c:clips){f->frontend_clip.fill(0xdeadbeef);set_frontend_clip_438190(*f,c.input);require(f->frontend_clip==c.expected,"asymmetric clip result");}
        require(std::fegetround()==mode&&std::fetestexcept(FE_ALL_EXCEPT)==flags,"new integer code leaves host FP environment unchanged");
    }
    require(std::fesetround(original_mode)==0,"restore host rounding mode");
    require(f->menu_music_word()==0x413fd378&&f->menu_counter_5da370==0x8322afed,"clip retains neighbor/music owners");
    require(std::all_of(f->menu_words_5da2e8.begin(),f->menu_words_5da2e8.end(),[](auto x){return x==0x6ae1832bu;}),"clip retains436EB0 owner");
    require(std::all_of(f->page_navigation().begin(),f->page_navigation().end(),[](auto x){return x==0xb3;}),"clip retains navigation");
    require(std::all_of(f->menu_words_5df194.begin(),f->menu_words_5df194.end(),[](auto x){return x==0x317845ab;}),"clip retains draw lists");
    require(f->lookup_a.front()==37&&f->lookup_b.back()==301&&f->lookup_c.back()==38,"clip retains lookups");
    const auto bytes=f->source_bytes(0x5da360,16);
    require(bytes.data()==reinterpret_cast<std::uint8_t*>(f->frontend_clip.data()),"clip sole byte owner");
    for(unsigned i=0;i<4;++i){auto& w=f->source_word(0x5da360+i*4);require(&w==&f->frontend_clip[i],"clip typed alias");w=0xa4b5c6d7u+i;require(persistence_word(bytes,i*4)==w,"clip byte visibility");}
    require(f->source_bytes(0x5da363,10).data()==bytes.data()+3,"unaligned byte subview");
    require(f->source_bytes(0x5da35f,1).data()==reinterpret_cast<std::uint8_t*>(f->menu_words_5da2e8.data())+119,"last byte now has436EB0 owner");
    for(auto [address,size]:std::array<std::pair<unsigned,unsigned>,4>{{{0x5da35c,8},{0x5da35f,2},{0x5da36c,8},{0x5da360,17}}}){
        bool rejected=false;try{(void)f->source_bytes(address,size);}catch(const sgn::FormatError&){rejected=true;}require(rejected,"clip cross-owner boundary rejection");
    }
    bool rejected=false;try{(void)f->source_word(0x5da361);}catch(const sgn::FormatError&){rejected=true;}require(rejected,"unaligned typed word rejection");
}
void composition(){
    auto f=std::make_unique<FrontendSourceStorage>();
    f->frontend_clip.fill(0xe5a3d819);f->menu_counter_5da370=79;f->menu_words_5df194.fill(32);
    f->menu_music_word()=0xa7217be3;std::fill(f->page_navigation().begin(),f->page_navigation().end(),std::uint8_t{0xce});
    const auto prelude=source_world_material_prelude(2);
    for(unsigned pass=0;pass<2;++pass){
        f->lookup_a.fill(0);f->lookup_b.fill(1);f->lookup_c.fill(2);
        initialize_frontend_materials_441190(*f,prelude);
        require(std::all_of(f->lookup_a.begin(),f->lookup_a.end(),[](auto v){return v==0xffffffffu;}),"full441190 lookup A");
        require(std::all_of(f->lookup_b.begin(),f->lookup_b.end(),[](auto v){return v==0xffffffffu;}),"full441190 lookup B");
        require(std::all_of(f->lookup_c.begin(),f->lookup_c.end(),[](auto v){return v==0x10000u;}),"full441190 lookup C");
        require(f->frontend_clip[0]==0xe5a3d819&&f->menu_counter_5da370==79&&f->menu_words_5df194.back()==32,"441190 retains drawing-only fields");
    }
    const auto descriptors=f->frontend_descriptors;
    initialize_frontend_drawing_437e60(*f);
    require(f->frontend_clip==std::array<std::uint32_t,4>{0,0,640,480},"actual437E78 arguments");
    require(f->menu_counter_5da370==0&&std::all_of(f->menu_words_5df194.begin(),f->menu_words_5df194.end(),[](auto v){return v==0;}),"actual437E80 call");
    require(f->frontend_descriptors==descriptors,"same descriptor owner and values");
    require(f->menu_music_word()==0xa7217be3&&std::all_of(f->page_navigation().begin(),f->page_navigation().end(),[](auto v){return v==0xce;}),"later navigation/music initialization excluded");
    require(frontend_initialization_after_drawing_callsite==0x43d105,"drawing returns before436EB0");
}
}
int main()try{
    leaf_and_clip();composition();
    std::cout<<"{\"passed\":true,\"checks\":"<<checks<<",\"literal_ftol_vectors\":28,\"literal_clip_vectors\":8,\"host_rounding_modes\":4,\"new_original_execution\":false}\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
