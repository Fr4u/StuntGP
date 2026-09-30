#include "sgn/source_scene_keyboard_prefix.hpp"
#include "sgn/source_scene_indexed_base.hpp"
#include "sgn/race_session.hpp"
#include "sgn/menu_keyboard_acquisition.hpp"
#include "source_allocation_begin_witness.hpp"
#include <algorithm>
#include <atomic>
#include <iostream>
#include <thread>

using namespace sgn;
using namespace sgn::retail;
namespace {
unsigned checks{},rejections{},stores{},interruptions{};
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
template<class F>void rejects(F call){
    bool rejected=false;try{call();}catch(const FormatError&){rejected=true;}
    check(rejected,"expected FormatError host guard");++rejections;
}
template<class T>Bytes image(const T& value){
    const auto* p=reinterpret_cast<const std::uint8_t*>(&value);return {p,p+sizeof(value)};
}
std::uint32_t seed(unsigned bank,unsigned word){return 0xa5010001u+bank*0x123400u+word*0x101u;}
void seed_keyboard(RaceInitializationState& s){
    for(unsigned i=0;i<64;++i){s.fields_62dcf0[i]=seed(0,i);s.fields_62d480[i]=seed(1,i);}
    s.field_5898c8=0x34567812;s.field_60f2c0=0x76398152;s.start_delay_ticks=0x91527463;
}
struct BankImage {
    const SourceAllocationBank& bank;
    allocation_begin_receiving::Words words;
    std::array<SourceAllocationRecord,384> rows;
    std::array<Bytes,384> payloads;
    explicit BankImage(const SourceAllocationBank& b):bank(b),words(allocation_begin_receiving::words(b)){
        std::ranges::copy(b.source_records(),rows.begin());
        for(unsigned i=0;i<384;++i)if(rows[i].raw){
            const auto* p=reinterpret_cast<const std::uint8_t*>(rows[i].aligned);
            payloads[i].assign(p,p+rows[i].requested_bytes);
        }
    }
    void verify() const {
        check(words==allocation_begin_receiving::words(bank),"keyboard reset changed bank words");
        for(unsigned i=0;i<384;++i){
            check(allocation_begin_receiving::same(rows[i],bank.source_records()[i]),"keyboard reset changed bank row");
            if(rows[i].raw)check(std::equal(payloads[i].begin(),payloads[i].end(),reinterpret_cast<const std::uint8_t*>(rows[i].aligned)),"keyboard reset changed bank payload");
        }
    }
};
struct Snapshot {
    RaceSessionState& state;
    Bytes world,payload;
    WorldPolygonFrameAllocations* allocation;
    BankImage bank;
    std::array<std::size_t,2> starts;
    explicit Snapshot(RaceSessionPlatform& p):state(p.state()),world(image(state)),
        payload(state.polygon_frame.allocations?image(*state.polygon_frame.allocations):Bytes{}),
        allocation(state.polygon_frame.allocations.get()),bank(p.source_allocations()){
        const auto base=reinterpret_cast<std::uintptr_t>(&state);
        starts={static_cast<std::size_t>(reinterpret_cast<std::uintptr_t>(state.startup.fields_62dcf0.data())-base),
            static_cast<std::size_t>(reinterpret_cast<std::uintptr_t>(state.startup.fields_62d480.data())-base)};
    }
    void verify(unsigned completed) const {
        auto expected=world;
        const auto previous=std::min(completed,64u),current=completed>64?completed-64:0u;
        std::fill_n(expected.begin()+static_cast<std::ptrdiff_t>(starts[0]),previous*4,std::uint8_t{0});
        std::fill_n(expected.begin()+static_cast<std::ptrdiff_t>(starts[1]),current*4,std::uint8_t{0});
        check(image(state)==expected,"write footprint changed world bytes outside completed keyboard DWORDs");
        check(state.polygon_frame.allocations.get()==allocation,"keyboard reset replaced typed owner");
        if(allocation)check(image(*allocation)==payload,"keyboard reset changed typed payload or lifetime metadata");
        bank.verify();
    }
};
struct Stop {};
struct Observer final:SourceSceneKeyboardPrefixObserver {
    RaceInitializationState& expected;
    unsigned completed{},stop_after{};
    bool initially_zero{};
    Observer(RaceInitializationState& s,unsigned stop=0,bool zero=false):expected(s),stop_after(stop),initially_zero(zero){}
    void cleared(std::uint32_t pc,std::uint32_t address,const RaceInitializationState& actual) override {
        check(&actual==&expected,"observer sees a copied keyboard owner");
        check(completed<128,"too many stores");
        const unsigned bank=completed/64,word=completed%64;
        check(pc==(bank==0?0x42e542u:0x42e54eu),"wrong instruction boundary");
        check(address==(bank==0?0x62dcf0u:0x62d480u)+4*word,"wrong forward DWORD address");
        ++completed;++stores;
        for(unsigned i=0;i<64;++i){
            check(actual.fields_62dcf0[i]==(initially_zero||i<completed?0:seed(0,i)),"previous bank prefix/suffix changed");
            check(actual.fields_62d480[i]==(initially_zero||i+64<completed?0:seed(1,i)),"current bank prefix/suffix changed");
        }
        if(completed==stop_after)throw Stop{};
    }
};
void interruptions_and_owners(const std::filesystem::path& root){
    auto platform=std::make_shared<RaceSessionPlatform>(SourceColdConstructionOnly{},root,0.0f);
    auto& s=platform->state();auto& frame=s.polygon_frame;
    allocate_world_polygon_frame(frame);attach_world_polygon_level(frame);
    const auto ticket=capture_world_polygon_level(frame);
    frame.indexed.publish_current(frame.allocations->indexed_lifetime.pointer(17));
    frame.transformed.count=23;frame.packets[2].publish_current(frame.allocations->packet_lifetimes[2].pointer(31));frame.totals[2]=47;
    frame.allocations->vertices[11].words.fill(0x541923b7);
    frame.allocations->indexed[4]=&frame.allocations->vertices[11];
    s.view_matrices.cursor=123;s.view_matrices.count=77;s.screen_polygon_count=81;s.light_vertex_offset=93;
    auto& bank=platform->source_allocations();bank.initialize_42eab0();const auto retained=bank.allocate_42e8d0(37,1);
    std::ranges::fill(bank.bytes(retained),std::uint8_t{0x39});
    for(unsigned stop=1;stop<=128;++stop){
        seed_keyboard(s.startup);Snapshot before(*platform);Observer observer(s.startup,stop);
        bool stopped=false;
        try{reset_source_scene_keyboard_42e542(platform,&observer);}catch(const Stop&){stopped=true;}
        check(stopped&&observer.completed==stop,"interruption did not preserve exact stopping point");
        before.verify(stop);validate_world_polygon_level(frame,ticket);++interruptions;
    }
    seed_keyboard(s.startup);Snapshot before(*platform);Observer observer(s.startup);
    check(reset_source_scene_keyboard_42e542(platform,&observer).next_required_instruction==0x42e55a,"advanced past first missing packet-base contract");
    check(observer.completed==128,"incomplete keyboard prefix");before.verify(128);
    Observer repeated(s.startup,0,true);Snapshot zero(*platform);
    check(reset_source_scene_keyboard_42e542(platform,&repeated).next_required_instruction==0x42e55a&&repeated.completed==128,"repeat skipped source observations");
    zero.verify(128);validate_world_polygon_level(frame,ticket);
    // Prior indexed leaf remains separate; retained current and all packet state
    // are covered by the full object/payload snapshot after that one store.
    check(reset_source_scene_indexed_base_42e53c(platform).next_required_instruction==0x42e542,"broken preceding boundary");
    seed_keyboard(s.startup);Snapshot composed(*platform);reset_source_scene_keyboard_42e542(platform);composed.verify(128);
    check(frame.indexed.base().is_null()&&frame.indexed.current().offset()==17,"keyboard leaf normalized independent indexed current");
    validate_world_polygon_level(frame,ticket);
    const auto borrowed=menu_keyboard_bytes(s.startup);
    check(borrowed.data()==reinterpret_cast<std::uint8_t*>(s.startup.fields_62d480.data())&&
        std::ranges::all_of(borrowed,[](auto b){return b==0;}),"menu acquisition does not borrow the cleared current owner");
}
void guards_and_unallocated(const std::filesystem::path& root){
    auto platform=std::make_shared<RaceSessionPlatform>(SourceColdConstructionOnly{},root,0.0f);
    auto& state=platform->state();seed_keyboard(state.startup);Snapshot before(*platform);
    check(!platform->source_allocations().initialized()&&!state.polygon_frame.allocations,"source fixture already admitted resources");
    Observer observer(state.startup);reset_source_scene_keyboard_42e542(platform,&observer);before.verify(128);
    check(!platform->source_allocations().initialized()&&!state.polygon_frame.allocations,"keyboard reset allocated resources");
    rejects([&]{(void)platform->assets();});rejects([&]{(void)platform->source_scene_resources();});
    RaceSession session(SourceColdAttachmentOnly{},platform);
    rejects([&]{(void)session.scene_resources();});rejects([&]{(void)session.models();});
    reset_source_scene_keyboard_42e542(platform);
    rejects([&]{(void)session.scene_resources();});rejects([&]{(void)session.models();});
    seed_keyboard(state.startup);Snapshot threaded(*platform);Observer unused(state.startup);
    std::atomic<unsigned> thread_rejections{};
    std::thread foreign([&]{try{reset_source_scene_keyboard_42e542(platform,&unused);}catch(const std::logic_error&){++thread_rejections;}});
    foreign.join();check(thread_rejections==1&&unused.completed==0,"foreign thread reached source write/observer");
    ++rejections;threaded.verify(0);
    std::weak_ptr<RaceSessionPlatform> dead;
    {auto transient=std::make_shared<RaceSessionPlatform>(SourceColdConstructionOnly{},root,0.0f);dead=transient;}
    rejects([&]{reset_source_scene_keyboard_42e542(dead,&unused);});check(unused.completed==0,"expired platform reached observer");
}
struct ReleasingObserver final:SourceSceneKeyboardPrefixObserver {
    std::shared_ptr<RaceSessionPlatform>& external;
    std::weak_ptr<RaceSessionPlatform> weak;
    const RaceInitializationState* startup;
    unsigned completed{};
    explicit ReleasingObserver(std::shared_ptr<RaceSessionPlatform>& p):external(p),weak(p),startup(&p->state().startup){}
    void cleared(std::uint32_t,std::uint32_t,const RaceInitializationState& actual) override {
        if(completed==0)external.reset();
        check(!weak.expired()&&&actual==startup,"platform died or moved inside observer");++completed;
    }
};
void invocation_lifetime(const std::filesystem::path& root){
    auto platform=std::make_shared<RaceSessionPlatform>(SourceColdConstructionOnly{},root,0.0f);
    seed_keyboard(platform->state().startup);ReleasingObserver observer(platform);
    const auto result=reset_source_scene_keyboard_42e542(observer.weak,&observer);
    check(result.next_required_instruction==0x42e55a&&observer.completed==128,"last external release truncated prefix");
    check(!platform&&observer.weak.expired(),"leaf retained platform beyond invocation");
}
}
int main(int argc,char** argv)try{
    check(argc==2,"expected NEW retail root");interruptions_and_owners(argv[1]);guards_and_unallocated(argv[1]);invocation_lifetime(argv[1]);
    std::cout<<"{\"passed\":true,\"interruption_boundaries\":"<<interruptions<<",\"observed_dword_stores\":"<<stores
        <<",\"checks\":"<<checks<<",\"host_rejections\":"<<rejections
        <<",\"next_required_instruction\":\"42E55A\",\"source_allocation_claims\":0,\"native_runs\":0}\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
