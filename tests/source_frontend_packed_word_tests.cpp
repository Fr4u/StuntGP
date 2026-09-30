#include "sgn/source_frontend_packed_word.hpp"
#include "sgn/retail_frontend_state.hpp"
#include "sgn/retail_frontend_initialization.hpp"
#include "../src/source_frontend_packed_word_store.hpp"
#include "source_frontend_packed_word_vectors.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <iostream>
#include <limits>
#include <memory>
#include <tuple>
#include <type_traits>
#include <vector>
using namespace sgn::retail;
namespace {
std::uint64_t checks{};
void require(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
template<class F>void rejected(F&& operation){bool caught=false;try{operation();}catch(const sgn::FormatError&){caught=true;}require(caught,"cross-owner/unaligned source view accepted");}
using Region=std::pair<std::uint32_t,std::size_t>;
constexpr std::array<Region,29> guard_regions{{
    {0x5da2e8,120},{0x5da360,16},{0x5da370,4},{0x5df194,640},
    {0x5df560,persistence_size},{FrontendSourceStorage::suffix_base,FrontendSourceStorage::cold_bytes-persistence_size},
    {0x625938,persistence_size},{0x5fd148,256},{0x47966c,4},{0x5ff328,128},{0x5fd258,0x20d0},
    {0x5ff3e8,208},{0x61b9c8,0x104},{0x61bdcc,0x104},{0x5ff8c0,4},{0x5ff8c4,0x404},
    {0x5ffcc8,0x6000},{0x605cc8,0xb70},{0x606838,36},{0x606860,8000},{0x47b1a0,4},{0x6087a0,4},{0x6087a4,4},
    {0x608d88,0x1224*4},{0x61bbe0,0x1ec},{0x61bed0,0x7c},
    {0x5ff3e8,92},{0x5ff4ac,4},{0x5ff4b4,4}
}};
std::vector<std::uint8_t> guards(FrontendSourceStorage& f){
    std::vector<std::uint8_t> out;
    for(const auto& [address,size]:guard_regions){const auto bytes=f.source_bytes(address,size);out.insert(out.end(),bytes.begin(),bytes.end());}
    return out;
}
void seed_guards(FrontendSourceStorage& f){
    unsigned group=0;
    for(const auto& [address,size]:guard_regions){auto bytes=f.source_bytes(address,size);for(std::size_t i=0;i<bytes.size();++i)bytes[i]=static_cast<std::uint8_t>(0x31u+group*17u+i*3u);++group;}
}
void vectors_and_sole_store(){
    auto first=std::make_unique<FrontendSourceStorage>(),second=std::make_unique<FrontendSourceStorage>();
    auto& f=*first;seed_guards(f);seed_guards(*second);second->packed_word_5ff4b8=0x9abc1234u;
    const auto before=guards(f),other_before=guards(*second);
    for(const auto& [input,expected]:packed_word_vectors){
        f.packed_word_5ff4b8=~expected;
        set_frontend_packed_word_43dd10(f,input);
        require(f.packed_word_5ff4b8==expected,"prepared1027 word expectation mismatch");
        require(f.source_word(0x5ff4b8)==expected,"source word did not see leaf store");
        const auto bytes=f.source_bytes(0x5ff4b8,4);
        const auto expected_bytes=std::bit_cast<std::array<std::uint8_t,4>>(expected);
        require(std::equal(bytes.begin(),bytes.end(),expected_bytes.begin()),"source bytes did not see little-endian leaf store");
        require(guards(f)==before,"leaf changed another source owner");
        require(second->packed_word_5ff4b8==0x9abc1234u&&guards(*second)==other_before,"leaf changed another frontend instance");
        unsigned stores=0;std::array<std::uint32_t,3> event{};
        detail::frontend_packed_word_store_43dd10(input,[&](auto pc,auto address,auto value){++stores;event={pc,address,value};});
        require(stores==1&&event==std::array<std::uint32_t,3>{0x43dd31u,0x5ff4b8u,expected},"shared source-store path added/skipped/wrote another destination");
    }
    unsigned zero_stores=0;
    for(unsigned i=0;i<2;++i)detail::frontend_packed_word_store_43dd10(0,[&](auto pc,auto address,auto value){require(pc==0x43dd31&&address==0x5ff4b8&&value==0,"cold caller logical store");++zero_stores;});
    require(zero_stores==2,"repeated zero input conditionally skipped a logical source store");
}
void aliasing_and_bounds(){
    static_assert(std::endian::native==std::endian::little);
    static_assert(sizeof(FrontendSourceStorage::packed_word_5ff4b8)==4);
    static_assert(std::tuple_size_v<decltype(FrontendSourceStorage::navigation_words)> == 52);
    static_assert(!std::is_copy_constructible_v<FrontendSourceStorage> && !std::is_move_constructible_v<FrontendSourceStorage>);
    auto object=std::make_unique<FrontendSourceStorage>();auto& f=*object;
    const auto bytes=f.source_bytes(0x5ff4b8,4);auto& word=f.source_word(0x5ff4b8);
    require(&word==&f.packed_word_5ff4b8&&bytes.data()==reinterpret_cast<std::uint8_t*>(&word),"new views copied rather than aliased sole word");
    word=0x12345678u;require(bytes[0]==0x78&&bytes[1]==0x56&&bytes[2]==0x34&&bytes[3]==0x12,"word write not visible in bytes");
    for(unsigned i=0;i<4;++i){const auto part=f.source_bytes(0x5ff4b8+i,1);require(part.data()==bytes.data()+i,"sub-byte view moved");part[0]=static_cast<std::uint8_t>(0x81+i);}
    require(word==0x84838281u,"byte writes not visible in typed word");
    require(f.source_bytes(0x5ff4b8,0).empty()&&f.source_bytes(0x5ff4bc,0).empty(),"zero-length end view must have no payload");
    const auto preserved=word;
    for(auto [address,size]:std::array<Region,7>{{{0x5ff4b7,2},{0x5ff4b4,8},{0x5ff3e8,212},{0x5ff4b8,5},{0x5ff4bb,2},{0x5ff4bc,1},{0x5ff4b8,std::numeric_limits<std::size_t>::max()}}})
        rejected([&]{(void)f.source_bytes(address,size);});
    for(auto address:{0x5ff4b9u,0x5ff4bau,0x5ff4bbu,0x5ff4bcu})rejected([&]{(void)f.source_word(address);});
    require(word==preserved,"invalid view request mutated target");
    require(&f.source_word(0x5ff4b4)==&f.navigation_words[51]&&&word!=&f.navigation_words[51],"word aliased navigation suffix");
}
void existing_clears_preserve_target(){
    auto object=std::make_unique<FrontendSourceStorage>();auto& f=*object;
    for(const auto target:{0u,0xffffffffu,0x12345678u,0xdeadbeefu}){
        f.navigation_words.fill(0x13572468u);f.packed_word_5ff4b8=target;
        initialize_frontend_navigation_43dda0(f);
        require(f.packed_word_5ff4b8==target,"43DDA0 incorrectly cleared53rd word");
        auto wanted=std::array<std::uint32_t,52>{};wanted[0]=1;wanted[1]=0x10000;wanted[23]=1;
        require(f.navigation_words==wanted,"existing52-word initialization changed");
        f.navigation_words.fill(0xabcdef12u);const auto before=f.navigation_words;
        clear_frontend_navigation_tail_43dee0(f);
        for(unsigned i=0;i<52;++i)require(f.navigation_words[i]==(i>=34&&i<=45?0u:before[i]),"43DEE0 range changed");
        require(f.packed_word_5ff4b8==target,"43DEE0 changed packed word");
        clear_frontend_cold_storage(f);require(f.packed_word_5ff4b8==target,"cold suffix clear changed separate word");
    }
}
}
int main()try{
    vectors_and_sole_store();aliasing_and_bounds();existing_clears_preserve_target();
    std::cout<<"{\"passed\":true,\"prepared_vectors\":1027,\"checks\":"<<checks<<",\"navigation_words\":52,\"parent_connected\":false,\"original_execution\":false}\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
