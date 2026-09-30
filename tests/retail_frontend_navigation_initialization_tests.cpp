#include "sgn/retail_frontend_initialization.hpp"
#include "sgn/retail_frontend_state.hpp"
#include "sgn/retail_campaign_score_navigation.hpp"
#include "../src/retail_frontend_navigation_stores.hpp"
#include <algorithm>
#include <array>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <vector>
using namespace sgn::retail;
namespace {
unsigned checks=0;
void require(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
using NavigationPayload=std::span<std::uint8_t,92>;
static_assert(std::is_same_v<decltype(std::declval<FrontendSourceStorage&>().page_navigation()),NavigationPayload>);
static_assert(std::is_same_v<decltype(std::declval<const FrontendSourceStorage&>().page_navigation()),std::span<const std::uint8_t,92>>);
static_assert(std::is_same_v<decltype(std::declval<FrontendSourceStorage&>().menu_music_word()),std::uint32_t&>);
static_assert(std::is_same_v<decltype(std::declval<const FrontendSourceStorage&>().menu_music_word()),const std::uint32_t&>);
static_assert(!std::is_copy_constructible_v<FrontendSourceStorage> && !std::is_move_constructible_v<FrontendSourceStorage>);
static_assert(!std::is_copy_assignable_v<FrontendSourceStorage> && !std::is_move_assignable_v<FrontendSourceStorage>);
static_assert(!std::is_copy_assignable_v<CampaignScoreNavigationViews>); //reference remains a borrow
CampaignScoreNavigationViews navigation_view(FrontendSourceStorage& f) {
    return {{std::as_writable_bytes(std::span{f.page_pool}),std::as_writable_bytes(f.source_bytes(0x5fcf08,4)).first<4>()},
        std::as_writable_bytes(f.page_navigation()),7,f.menu_music_word()};
}
void ownership_and_copies() {
    auto owner_a=std::make_unique<FrontendSourceStorage>(),owner_b=std::make_unique<FrontendSourceStorage>();
    auto& a=*owner_a;auto& b=*owner_b;const auto& readonly=a;
    for(unsigned pass=0;pass<32;++pass){
        for(unsigned i=0;i<52;++i){a.navigation_words[i]=0x932146abu^(i*13007u+pass);b.navigation_words[i]=~a.navigation_words[i];}
        const auto full=a.source_bytes(0x5ff3e8,208);const auto prefix=a.page_navigation();
        auto& music=a.menu_music_word();const auto old_b=b.page_navigation();auto& music_b=b.menu_music_word();
        require(full.data()==reinterpret_cast<std::uint8_t*>(a.navigation_words.data()),"sole208-byte owner");
        require(prefix.data()==full.data()&&prefix.size()==92,"legacy92-byte prefix alias");
        require(readonly.page_navigation().data()==prefix.data()&&&readonly.menu_music_word()==&music,"const aliases same owner");
        require(&music==&a.navigation_words[49]&&reinterpret_cast<std::uint8_t*>(&music)==full.data()+196,"music actual word49");
        for(unsigned i=0;i<52;++i){
            require(&a.source_word(0x5ff3e8+i*4)==&a.navigation_words[i],"all52 typed aliases");
            require(persistence_word(full,i*4)==a.navigation_words[i],"typed-byte alias contents");
        }
        //These formerly split ranges now fit one physical owner.
        require(a.source_bytes(0x5ff440,16).data()==full.data()+88,"old navigation boundary is internal");
        require(a.source_bytes(0x5ff4a8,16).data()==full.data()+192,"span includes music and final two words");
        require(a.source_bytes(0x5ff3eb,203).data()==full.data()+3,"unaligned byte payload subview");
        require(a.source_bytes(0x5ff4b7,1).data()==full.data()+207,"last owned byte");
        require(a.source_bytes(0x5ff4b8,0).empty(),"empty end view has no payload");
        for(auto [address,size]:std::array<std::pair<std::uint32_t,std::size_t>,7>{{
            {0x5ff3e4,8},{0x5ff3e7,1},{0x5ff3e8,209},{0x5ff4b4,8},{0x5ff4bc,1},
            {0x5ff3e8,std::numeric_limits<std::size_t>::max()},{0xfffffffcu,8}}}){
            bool rejected=false;try{(void)a.source_bytes(address,size);}catch(const sgn::FormatError&){rejected=true;}
            require(rejected,"outside full owner or oversized view rejected");
        }
        for(auto address:{0x5ff3e4u,0x5ff3e9u,0x5ff4adu,0x5ff4b5u,0x5ff4bcu}){
            bool rejected=false;try{(void)a.source_word(address);}catch(const sgn::FormatError&){rejected=true;}
            require(rejected,"outside/unaligned typed reference rejected");
        }
        const auto detached_words=a.navigation_words;
        const std::vector<std::uint8_t> detached_payload(prefix.begin(),prefix.end());
        const auto detached_music=music;
        auto view_a=navigation_view(a),view_b=navigation_view(b);const auto copied_view=view_a;
        require(copied_view.navigation.data()==view_a.navigation.data()&&&copied_view.last_music==&music,"view copy is intentional borrow");
        require(view_b.navigation.data()!=view_a.navigation.data()&&&view_b.last_music==&music_b,"separate owner rebound view");
        a.navigation_words=b.navigation_words; //value copy, never a pointer copy
        require(a.page_navigation().data()==prefix.data()&&b.page_navigation().data()==old_b.data(),"bank assignment preserves both view lifetimes");
        require(&view_a.last_music==&a.navigation_words[49]&&&view_b.last_music==&b.navigation_words[49],"music references remain bound after value copy");
        require(music==music_b&&music!=detached_music,"value copied, detached scalar retained");
        require(std::equal(detached_payload.begin(),detached_payload.end(),reinterpret_cast<const std::uint8_t*>(detached_words.data())),"snapshot owns92 bytes, not span metadata");
        const auto retained_b=b.navigation_words;
        initialize_frontend_navigation_43dda0(a);
        require(b.navigation_words==retained_b,"initializing A retains B values");
        require(copied_view.last_music==0&&persistence_word(a.page_navigation(),0)==1,"borrow sees actual initialization writes");
        require(detached_words[49]==detached_music,"detached value snapshot survives initialization");
        auto wanted=std::array<std::uint32_t,52>{};wanted[0]=1;wanted[1]=0x10000;wanted[23]=1;
        require(a.navigation_words==wanted,"all52 source end words");
    }
}
void exact_store_order() {
    using Event=std::pair<std::uint32_t,std::uint32_t>;
    std::vector<Event> actual;actual.reserve(87);
    detail::frontend_navigation_stores_43dda0([&](auto at,auto value){actual.emplace_back(at,value);});
    require(actual.size()==87,"52 clears plus35 literal stores, including duplicates");
    for(unsigned i=0;i<52;++i)require(actual[i]==Event{0x5ff3e8+i*4,0},"ascending initial clear order");
    constexpr std::array<unsigned,35> indices{1,0,2,3,4,5,22,23,24,25,26,27,28,29,30,31,32,33,46,47,48,49,51,34,35,36,37,38,39,40,41,42,43,44,45};
    for(unsigned i=0;i<indices.size();++i){const auto index=indices[i];
        const auto value=index==1?0x10000u:index==0||index==23?1u:0u;
        require(actual[52+i]==Event{0x5ff3e8+index*4,value},"literal store sequence, tail jump order");
    }
    actual.clear();detail::frontend_navigation_tail_stores_43dee0([&](auto at,auto value){actual.emplace_back(at,value);});
    require(actual.size()==12,"standalone tail exactly12 stores");
    for(unsigned i=0;i<12;++i)require(actual[i]==Event{0x5ff470+i*4,0},"tail store addresses/order");
}
}
int main()try{
    ownership_and_copies();exact_store_order();
    std::cout<<"{\"passed\":true,\"checks\":"<<checks<<",\"owner_copy_cases\":32,\"full_store_count\":87,\"tail_store_count\":12,\"original_execution\":false}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
