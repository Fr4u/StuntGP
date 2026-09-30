#include "sgn/retail_frontend_state.hpp"
#include <cstring>
#include <fstream>
#include <iostream>
#include <type_traits>
using namespace sgn;using namespace sgn::retail;
static_assert(!std::is_copy_constructible_v<FrontendSourceStorage> && !std::is_move_constructible_v<FrontendSourceStorage>);
static_assert(!std::is_copy_constructible_v<FrontendState> && !std::is_move_constructible_v<FrontendState>);
namespace {
struct Region {unsigned address;Bytes data;};
using Frame=std::vector<Region>;
template<class T> void add(Frame& f,unsigned at,const T& value){static_assert(std::is_trivially_copyable_v<T>);Bytes b(sizeof(value));std::memcpy(b.data(),&value,b.size());f.push_back({at,std::move(b)});}
//An accidental span-object serialization must fail at compile time.
template<class T,std::size_t N> void add(Frame&,unsigned,std::span<T,N>)=delete;
template<class T> concept ObjectSerializable=requires(Frame& f,const T& value){add(f,0u,value);};
static_assert(!ObjectSerializable<std::span<const std::uint8_t,92>> && ObjectSerializable<std::array<std::uint8_t,92>>);
void add_payload(Frame& f,unsigned at,std::span<const std::uint8_t> payload){
    f.push_back({at,Bytes(payload.begin(),payload.end())});
}
Frame capture(const FrontendSourceStorage& s,const std::uint32_t& audio_pause){
    Frame f;add(f,0x5df560,s.persistence.bytes);add(f,0x5e167c,s.cold_suffix);add(f,0x625938,s.read_scratch.bytes);
    add(f,0x5fd148,s.csv_token_scratch);
    add(f,0x47966c,s.initials_scratch);add(f,0x5ff328,s.numeric_scratch);
    add(f,0x5fd258,s.page_pool);add_payload(f,0x5ff3e8,s.page_navigation());add(f,0x5ff4ac,s.menu_music_word());
    add(f,0x61b9c8,s.challenge_text);add(f,0x61bdcc,s.campaign_text);
    add(f,0x6087a8,s.lookup_a);add(f,0x60883c,s.lookup_b);add(f,0x608cf0,s.lookup_c);
    add(f,0x5fd248,s.menu_state);add(f,0x5fd250,s.initializing);add(f,0x5fd254,s.initialization_count);
    add(f,0x60d61c,s.scene.menu_initialized);add(f,0x4d5b20,audio_pause);
    add(f,0x61bbe0,s.campaign_launch);add(f,0x61bed0,s.campaign_result);add(f,0x5df528,s.race_return_selector);return f;
}
void seed(FrontendSourceStorage& s,std::uint32_t& audio_pause,unsigned n){
    unsigned random=0xa271d361u+n;const auto next=[&]{random^=random<<13;random^=random>>17;random^=random<<5;return random;};
    for(auto& b:s.persistence.bytes)b=static_cast<unsigned char>(next());for(auto& w:s.cold_suffix)w=next();
    for(auto& b:s.read_scratch.bytes)b=static_cast<unsigned char>(next());
    for(auto& b:s.csv_token_scratch)b=static_cast<unsigned char>(next());
    for(auto& b:s.initials_scratch)b=static_cast<unsigned char>(next());for(auto& b:s.numeric_scratch)b=static_cast<unsigned char>(next());
    for(auto& b:s.page_pool)b=static_cast<unsigned char>(next());for(auto& b:s.page_navigation())b=static_cast<unsigned char>(next());
    for(auto& b:s.challenge_text)b=static_cast<unsigned char>(next());for(auto& b:s.campaign_text)b=static_cast<unsigned char>(next());s.menu_music_word()=next();
    for(auto& w:s.lookup_a)w=next();for(auto& w:s.lookup_b)w=next();for(auto& w:s.lookup_c)w=next();
    for(auto& w:s.campaign_launch)w=next();for(auto& w:s.campaign_result)w=next();
    s.menu_state=next();s.initializing=next();s.initialization_count=next();s.scene.menu_initialized=next();audio_pause=next();s.race_return_selector=next();
}
unsigned ownership_checks(FrontendSourceStorage& s){
    unsigned checks=0;const auto require=[&](bool yes){if(!yes)throw FormatError("frontend single-owner binding");++checks;};
    const auto p=s.persistence_view();require(p.live.data()==s.persistence.bytes.data());
    require(p.saved_body.data()==reinterpret_cast<unsigned char*>(s.cold_suffix.data()));require(p.scratch.data()==s.read_scratch.bytes.data());
    s.bind_campaign_return();require(s.race_return_bindings[5].roster.data()==reinterpret_cast<unsigned char*>(s.campaign_launch.data()));
    require(s.race_return_bindings[5].result.data()==reinterpret_cast<unsigned char*>(s.campaign_result.data()));
    require(s.source_bytes(0x5f943c,0x660).data()==reinterpret_cast<unsigned char*>(s.cold_suffix.data())+(0x5f943c-0x5e167c));
    auto& phase=s.source_word(0x5e3794);phase=0xa51fc362u;unsigned actual{};const auto view=s.source_bytes(0x5e3794,4);
    std::memcpy(&actual,view.data(),4);require(actual==phase);
    p.saved_body[0]=0x57;require(s.source_bytes(0x5e167c,1)[0]==0x57);
    require(s.source_bytes(0x5fd258,0x20d0).data()==s.page_pool.data());
    require(s.source_bytes(0x5ff3e8,0x5c).data()==s.page_navigation().data());
    require(s.source_bytes(0x5ff4ac,4).data()==reinterpret_cast<unsigned char*>(&s.menu_music_word()));
    require(s.source_bytes(0x61b9c8,0x104).data()==s.challenge_text.data());
    require(s.source_bytes(0x61bdcc,0x104).data()==s.campaign_text.data());
    s.source_word(0x5ff4ac)=0x6e241792;require(s.menu_music_word()==0x6e241792);
    require(s.source_bytes(0x5ff440,16).data()==reinterpret_cast<unsigned char*>(s.navigation_words.data())+0x58);
    for(const auto address:{0x5ff320u,0x5ff4b4u,0x61bac8u,0x61beccu}){
        bool rejected=false;try{(void)s.source_bytes(address,16);}catch(const FormatError&){rejected=true;}require(rejected);}
    for(const auto address:{0x5df55cu,0x5fd048u,0x61bdd0u,0x5e3795u}){
        bool rejected=false;try{(void)s.source_word(address);}catch(const FormatError&){rejected=true;}require(rejected);}
    bool rejected=false;try{(void)s.source_bytes(0x5e1678,8);}catch(const FormatError&){rejected=true;}require(rejected);
    return checks;
}
}
int main(int argc,char** argv)try{
    if(argc!=2)return 2;std::ofstream out(argv[1],std::ios::binary);if(!out)return 2;
    const auto word=[&](unsigned value){out.write(reinterpret_cast<const char*>(&value),4);};
    out.write("SGNFS001",8);word(24);unsigned checks=0;
    for(unsigned n=0;n<24;++n){FrontendSourceStorage s;std::uint32_t audio_pause{};checks+=ownership_checks(s);seed(s,audio_pause,n);const auto before=capture(s,audio_pause);
        const auto kind=n%3;if(kind==0)clear_frontend_cold_storage(s);else if(kind==1)initialize_frontend_lookup_tables(s);else return_to_frontend(s);
        const auto after=capture(s,audio_pause);word(kind);word(static_cast<unsigned>(before.size()));
        for(std::size_t i=0;i<before.size();++i){const auto& a=before[i];const auto& b=after[i];word(a.address);word(static_cast<unsigned>(a.data.size()));
            out.write(reinterpret_cast<const char*>(a.data.data()),a.data.size());out.write(reinterpret_cast<const char*>(b.data.data()),b.data.size());}
        if(s.race_return_bindings[5].result.data()!=reinterpret_cast<unsigned char*>(s.campaign_result.data()))throw FormatError("frontend binding changed across source reset");
    }
    if(!out)return 3;std::cout<<"{\"passed\":true,\"ownership_checks\":"<<checks<<",\"source_cases\":24}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
