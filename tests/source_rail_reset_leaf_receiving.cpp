// Authored CPU receiving of the actual leaf. Initial owners come from a pinned
// historical image; expected reset stores come only from 4192D2/DC/E1.
#include "sgn/source_rail_reset.hpp"
#include "vehicle_integration_fixture.hpp"
#include <array>
#include <exception>
#include <thread>
#include <type_traits>

namespace {
using namespace sgn;
using namespace sgn::retail;
using namespace sgn::retail::whole_tick_test;
using sgn::retail::integration_test::Fixture;
using sgn::retail::integration_test::Reader;
unsigned checks{}, raw_comparisons{};
std::uint64_t compared_bytes{};
void require(bool value,const char* reason){++checks;if(!value)throw std::runtime_error(reason);}
void same_image(const Bytes& actual,const Bytes& expected,const char* reason){
    ++raw_comparisons;compared_bytes+=expected.size();require(actual==expected,reason);
}
template<class T> Bytes object_bytes(const T& value){
    static_assert(std::is_trivially_copyable_v<T>);
    const auto* begin=reinterpret_cast<const std::uint8_t*>(&value);
    return {begin,begin+sizeof(T)};
}
class Retained {
    struct Region {const void* value;Bytes bytes;};
    std::vector<Region> regions_;
    template<class T> void add(const T& value){regions_.push_back({&value,object_bytes(value)});}
public:
    explicit Retained(const RaceSessionState& state){
        add(state.reset_globals);add(state.model_basis);add(state.dynamics_workspace.steering);
        add(state.effect_staging);add(state.vehicles.rail);add(state.tick_clock);
        add(state.source_scheduler.fixed_mode_a);add(state.source_scheduler.fixed_mode_b);
        add(state.source_scheduler.frequency);add(state.grid.start_spline);add(state.simulation_tick);
        add(state.frame_pause);add(state.demo_ticks);add(state.hud_block);
    }
    void verify() const {
        for(const auto& r:regions_)
            require(std::memcmp(r.value,r.bytes.data(),r.bytes.size())==0,"leaf changed a retained typed owner");
    }
};
std::unique_ptr<Fixture> initial_fixture(const char* root,const char* path){
    Reader reader(path);const auto magic=reader.bytes(8);
    require(std::string_view(reinterpret_cast<const char*>(magic.data()),magic.size())=="SGNRI001",
        "initial-state extraction signature");
    require(reader.word()==1u,"expected one extracted initial image");
    const auto track=reader.text(64);require(track=="track1.PMD","initial track");
    const auto steps=reader.word(),mode=reader.word(),config=reader.word(),size=reader.word(),count=reader.word();
    require(steps>0&&steps<=384&&(mode==1u||mode==3u||mode==4u)&&config>=1&&config<=5&&
        size>=global_size&&size<=8*1024*1024&&count==19u,"initial metadata");
    std::vector<Region> regions;unsigned offset=0;
    for(unsigned j=0;j<count;++j){
        Region r;r.kind=reader.word();r.index=reader.word();r.address=reader.word();
        r.length=reader.word();r.descriptor=reader.word();r.name=reader.text(64);r.offset=offset;
        const auto kind=j==0?0u:j<=12?1u:2u,index=j==0?0u:j<=12?j-1:j-13;
        require(r.kind==kind&&r.index==index&&r.length&&r.length%4==0&&r.length<=size-offset&&
            r.address<=std::numeric_limits<unsigned>::max()-r.length,"initial region bounds");
        if(j==0)require(r.address==global_base&&r.length==global_size&&r.name=="globals"&&r.descriptor==0u,"global region");
        if(kind==1u){
            const auto expected="car"+std::to_string(index/2+1)+(index%2?"sh.PMD":".PMD");
            require(r.name==expected&&r.descriptor>=r.address&&r.descriptor<r.address+r.length,"model region");
        }
        for(const auto& before:regions)
            require(!(r.address<before.address+before.length&&before.address<r.address+r.length),"overlapping regions");
        offset+=r.length;regions.push_back(std::move(r));
    }
    require(offset==size&&reader.word()==93u,"initial image/text size");
    std::vector<std::string> texts;for(unsigned i=0;i<93;++i)texts.push_back(reader.text(63));
    auto image=reader.bytes(size);require(reader.input.peek()==std::char_traits<char>::eof(),"trailing initial image bytes");
    return std::make_unique<Fixture>(root,track,std::move(regions),std::move(image),std::move(texts));
}
struct Vector {unsigned texture,route,raw;};
constexpr std::array<Vector,12> vectors{{
    {0xffffffffu,0,0},{0,1,1},{7,2,0xffffffffu},{0xfffffffeu,1,0x80000000u},
    {0x80000000u,2,0x7fffffffu},{0xffffffffu,1,0xaaaaaaaa},
    {0,2,0x55555555},{7,0,0x11223344},{0xfffffffeu,0,0},
    {0x80000000u,1,0xdeadbeef},{0xffffffffu,2,1},{0,0,0xffffffffu}
}};
constexpr std::array<unsigned,9> neighbor_addresses{
    0x47466c,0x474674,0x4d5c94,0x4d5c98,0x4d5ca4,0x4d5ca8,0x4d5cac,0x4d5cb0,0x4d5cb4};
void seed(Bytes& image,const StateCodec& codec,Vector vector){
    put(image,0x474670-global_base,vector.texture);
    put(image,0x4d5c9c-global_base,vector.route?codec.spline_addresses.at(vector.route):0u);
    put(image,0x4d5ca0-global_base,vector.raw);
    for(unsigned j=0;j<neighbor_addresses.size();++j)
        put(image,neighbor_addresses[j]-global_base,0x13579bdfu^(j*0x1234567u));
}
void invoke(bool binding,RaceSessionPlatform& platform){
    if(binding)reset_platform_source_rail_4192d0(platform);
    else reset_rail_4192d0(platform.state().rail);
}
void expected_reset(Bytes& image){
    put(image,0x474670-global_base,0xffffffffu); // 4192D2
    put(image,0x4d5c9c-global_base,0);           // 4192DC
    put(image,0x4d5ca0-global_base,0);           // 4192E1
}
}

int main(int argc,char** argv)try{
    static_assert(noexcept(reset_rail_4192d0(std::declval<RailState&>())));
    require(argc==3,"supply NEW retail root and pinned initial-state fixture");
    RailState initial;
    require(!initial.spline&&!initial.tube_glass_texture&&initial.source_word_4d5ca0==0u,
        "PE defaults must be NULL/-1/zero");
    auto fixture=initial_fixture(argv[1],argv[2]);
    auto platform=std::make_shared<RaceSessionPlatform>(SourceColdConstructionOnly{},argv[1],0.0f);
    auto& state=platform->state();auto* identity=&state;
    require(state.rail.source_word_4d5ca0==0u,"platform did not admit PE zero-fill");
    auto alias=std::shared_ptr<RaceSessionState>(platform,identity);
    std::weak_ptr<RaceSessionPlatform> lifetime=platform;
    StateCodec codec(state,fixture->models,fixture->splines,fixture->track,fixture->regions);
    const auto base_image=fixture->actual;
    const auto borrowed_route=fixture->splines.find(2u);
    require(borrowed_route.has_value(),"source spline2 must exist in pinned track");
    const Bytes borrowed_bytes(borrowed_route->storage.begin(),borrowed_route->storage.end());
    const auto* borrowed_identity=borrowed_route->storage.data();
    RailState other;other.spline=borrowed_route;other.tube_glass_texture=61;other.source_word_4d5ca0=67;
    const auto other_before=object_bytes(other);
    auto check_host=[&]{
        require(&platform->state()==identity&&alias.get()==identity,"world owner replaced");
        require(!alias.owner_before(platform)&&!platform.owner_before(alias),"world alias changed control block");
        const auto ready=platform->host_readiness();
        require(!ready.assets&&!ready.audio_device&&!ready.audio_stream&&!ready.window_input,"leaf prepared native owners");
        require(!platform->session_attachment_claimed(),"leaf attached a session");
        require(object_bytes(other)==other_before,"leaf wrote a different RailState");
        const auto after=fixture->splines.find(2u);
        require(after&&after->storage.data()==borrowed_identity&&
            std::equal(borrowed_bytes.begin(),borrowed_bytes.end(),after->storage.begin(),after->storage.end()),
            "reset changed or released borrowed PMD storage");
    };
    unsigned successes=0,repeats=0,wrong_threads=0,codec_rejections=0,opaque_view_calls=0;
    for(const bool binding:{false,true})for(const auto vector:vectors){
        auto image=base_image;seed(image,codec,vector);codec.transfer(image,true);
        require(state.rail.source_word_4d5ca0==vector.raw,"raw4D5CA0 was not imported");
        require(state.rail.tube_glass_texture.value_or(0xffffffffu)==vector.texture,"texture import differs");
        require(state.rail.spline.has_value()==(vector.route!=0u),"spline import differs");
        auto before=image;codec.transfer(before,false);same_image(before,image,"initial typed/raw roundtrip differs");
        const Retained retained(state);
        auto expected=image;expected_reset(expected);
        invoke(binding,*platform);++successes;
        require(!state.rail.spline&&!state.rail.tube_glass_texture&&state.rail.source_word_4d5ca0==0u,
            "leaf did not clear all three owners");
        codec.transfer(image,false);same_image(image,expected,"leaf changed bytes beyond its three stores");
        retained.verify();check_host();
        for(unsigned n=0;n<3;++n){
            invoke(binding,*platform);++repeats;
            codec.transfer(image,false);same_image(image,expected,"repeated reset changed retained bytes");
            retained.verify();check_host();
        }
    }
    // An opaque/detached view is discarded without sampling its descriptor or
    // storage. This is a leaf-domain case, not a valid whole-tick codec token.
    for(const bool binding:{false,true}){
        SplineView opaque;opaque.descriptor.id=0xffffffffu;opaque.descriptor.point_count=0xffffffffu;
        opaque.point_storage_end=std::numeric_limits<std::size_t>::max();
        state.rail.spline=opaque;state.rail.tube_glass_texture=0xffffffffu;
        state.rail.source_word_4d5ca0=0x80000000u;
        const Retained retained(state);invoke(binding,*platform);++opaque_view_calls;
        require(!state.rail.spline&&!state.rail.tube_glass_texture&&state.rail.source_word_4d5ca0==0u,
            "leaf tried to resolve an opaque borrowed view");
        retained.verify();check_host();
    }
    for(const auto vector:{vectors[0],vectors[2]}){
        auto image=base_image;seed(image,codec,vector);codec.transfer(image,true);
        const auto rail_before=object_bytes(state.rail);const Retained retained(state);
        std::exception_ptr error;
        std::thread worker([&]{try{reset_platform_source_rail_4192d0(*platform);}catch(...){error=std::current_exception();}});
        worker.join();require(bool(error),"wrong thread entered reset");
        bool correct=false;
        try{std::rethrow_exception(error);}catch(const std::logic_error& e){
            correct=std::string_view(e.what())=="race world and platform belong to the creating game thread";
        }
        require(correct,"wrong-thread failure did not come from existing guard");
        require(object_bytes(state.rail)==rail_before,"wrong-thread guard allowed partial stores");
        auto after=image;codec.transfer(after,false);same_image(after,image,"wrong thread mutated captured world");
        retained.verify();check_host();++wrong_threads;
    }
    // Existing test codec rejects unknown raw spline addresses BEFORE invoking
    // the leaf. No claim of an original4192D0 failure or transactional codec.
    for(const unsigned invalid:{0x4d5ca0u,0xffffffffu,codec.spline_addresses.at(2u)+1u}){
        auto image=base_image;seed(image,codec,vectors[2]);codec.transfer(image,true);
        const auto rail_before=object_bytes(state.rail);
        put(image,0x4d5c9c-global_base,invalid);put(image,0x4d5ca0-global_base,0x12345678u);
        bool rejected=false;
        try{codec.transfer(image,true);}catch(const FormatError& e){rejected=std::string_view(e.what())=="whole tick spline pointer";}
        require(rejected,"invalid spline address was silently normalized");
        require(object_bytes(state.rail)==rail_before,"codec failure executed rail stores");
        ++codec_rejections;
    }
    check_host();platform.reset();require(!lifetime.expired(),"alias lost platform lifetime");
    require(alias->rail.source_word_4d5ca0==vectors[2].raw,"alias did not retain the same raw owner");
    alias.reset();require(lifetime.expired(),"platform lifetime leaked");
    std::cout<<"{\"passed\":true,\"vectors\":"<<vectors.size()<<",\"successful_calls\":"<<successes
        <<",\"repeated_calls\":"<<repeats<<",\"opaque_view_calls\":"<<opaque_view_calls
        <<",\"wrong_thread_cases\":"<<wrong_threads<<",\"codec_domain_rejections\":"<<codec_rejections
        <<",\"checks\":"<<checks<<",\"raw_comparisons\":"<<raw_comparisons
        <<",\"compared_bytes\":"<<compared_bytes<<",\"captured_regions\":"<<fixture->regions.size()
        <<",\"neighbor_sentinels\":"<<neighbor_addresses.size()
        <<",\"native_QPC_calls\":0,\"original_runs\":0,\"GPU_runs\":0}\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"rail reset receiving: "<<e.what()<<'\n';return 1;}
