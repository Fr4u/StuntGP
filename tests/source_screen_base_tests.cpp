// AUTHORED ONLY: this source has not been compiled or executed.
#include "sgn/source_screen_base.hpp"
#include "sgn/race_session.hpp"
#include <array>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <iostream>
#include <new>
#include <stdexcept>
#include <thread>

// Receiving-only ordinary-new tripwire, following the existing NEW
// world_packet_join_cpu test policy. Not linked into a game target.
thread_local bool forbid_allocations=false;
void* operator new(std::size_t n){
    if(forbid_allocations)throw std::bad_alloc();
    if(auto* p=std::malloc(n?n:1))return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p) noexcept{std::free(p);}
void operator delete[](void* p) noexcept{std::free(p);}
void operator delete(void* p,std::size_t) noexcept{std::free(p);}
void operator delete[](void* p,std::size_t) noexcept{std::free(p);}

using namespace sgn;
using namespace sgn::retail;
namespace {
unsigned checks{},stores{},rejects{};
void check(bool ok,const char* text){++checks;if(!ok)throw std::runtime_error(text);}
struct NoHeap {
    bool previous{forbid_allocations};
    NoHeap(){forbid_allocations=true;}
    ~NoHeap(){forbid_allocations=previous;}
};
struct Stop {};
template<class F> void rejects_host(F&& call){
    bool rejected=false;try{call();}catch(const FormatError&){rejected=true;}
    check(rejected,"expected screen host rejection");++rejects;
}
using Records=std::array<SourceScreenPolygon,256>;
static_assert(sizeof(Records)==69632);
void seed(RaceSessionState& s){
    for(std::size_t p=0;p<s.screen_polygons.size();++p){
        auto& record=s.screen_polygons[p];const auto n=static_cast<std::uint32_t>(p);
        record.texture=0x10200000u+n;record.flags=0x33440000u+n;
        record.alpha=0x55660000u+n;record.vertex_count=0x77880000u+n;
        for(std::size_t v=0;v<record.vertices.size();++v){
            auto& vertex=record.vertices[v];const auto k=static_cast<std::uint32_t>(v);
            vertex.x=float(n);vertex.y=float(k);vertex.z=-float(n+k);
            vertex.retained_w=0xaabb0000u+n*8+k;vertex.u=0.25f;vertex.v=0.75f;
            vertex.color=0xff120000u+n*8+k;vertex.outcode=0xccdd0000u+n*8+k;
        }
    }
    allocate_world_polygon_frame(s.polygon_frame); //existing actual owner, fixture setup only
    auto& f=s.polygon_frame;auto& a=*f.allocations;
    f.indexed.clear_base();f.indexed.publish_current(a.indexed_lifetime.pointer(9));
    for(unsigned p=0;p<4;++p){
        f.packets[p].clear_base();f.packets[p].publish_current(a.packet_lifetimes[p].pointer(7+p));
        f.totals[p]=0xfff00000u+p;
        for(unsigned g=0;g<4;++g)f.groups[g][p]={a.packet_lifetimes[p].pointer(11+p),0xffaa0000u+g*4+p};
        for(std::size_t n=0;n<a.packets[p].size();++n)a.packets[p][n].inside_mask=0xcafe0000u+p*4096+static_cast<std::uint32_t>(n);
    }
    s.view_matrices.count=0xabbacddcu;s.light_vertex_offset=0xaabbccddu;
    s.startup.fields_62dcf0[0]=0x10203040u;s.startup.fields_62d480[63]=0x50607080u;
}
void retained(const RaceSessionState& s,const Records& before,std::uint32_t count,
              const WorldPolygonFrameAllocations* allocation){
    check(std::memcmp(s.screen_polygons.data(),before.data(),sizeof(before))==0,"full256-record payload changed");
    check(s.screen_polygon_count==count,"61C604 changed during base-only store");
    const auto& f=s.polygon_frame;check(f.allocations.get()==allocation,"other actual allocation replaced");
    const auto& a=*allocation;
    check(f.indexed.base().is_null()&&f.indexed.current().same_pointer(a.indexed_lifetime.pointer(9)),"indexed base/current changed");
    for(unsigned p=0;p<4;++p){
        check(f.packets[p].base().is_null()&&f.packets[p].current().same_pointer(a.packet_lifetimes[p].pointer(7+p)),"packet base/current changed");
        check(f.totals[p]==0xfff00000u+p,"packet total changed");
        for(unsigned g=0;g<4;++g)check(f.groups[g][p].first.same_pointer(a.packet_lifetimes[p].pointer(11+p))&&f.groups[g][p].count==0xffaa0000u+g*4+p,"group changed");
        for(std::size_t n=0;n<a.packets[p].size();++n)check(a.packets[p][n].inside_mask==0xcafe0000u+p*4096+static_cast<std::uint32_t>(n),"retained packet mask changed");
    }
    check(s.view_matrices.count==0xabbacddcu&&s.light_vertex_offset==0xaabbccddu,"unrelated cursor/count changed");
    check(s.startup.fields_62dcf0[0]==0x10203040u&&s.startup.fields_62d480[63]==0x50607080u,"earlier keyboard stores replayed");
}
struct Observer final:SourceScreenBaseStoreObserver {
    const RaceSessionState& expected_world;
    const Records& records;
    const WorldPolygonFrameAllocations* allocation;
    std::uint32_t count{},pc{};
    bool null{},stop{};
    Observer(const RaceSessionState& s,const Records& r):expected_world(s),records(r),allocation(s.polygon_frame.allocations.get()){}
    void stored(std::uint32_t instruction,std::uint32_t address,const RaceSessionState& s) override{
        check(&s==&expected_world,"observer received a mirror/foreign world");
        check(instruction==pc&&address==0x61c5f0,"source event order/address differs");
        check(s.screen_base.is_null()==null,"observer ran before publication");
        retained(s,records,count,allocation);++stores;
        if(stop)throw Stop{};
    }
};
}
int main(int argc,char** argv)try{
    check(argc==2,"expected retail root for existing source-cold platform");
    auto platform=std::make_shared<RaceSessionPlatform>(SourceColdConstructionOnly{},argv[1],0.0f);
    std::weak_ptr<RaceSessionPlatform> weak=platform;auto& s=platform->state();
    check(s.screen_base.is_null(),"new source61C5F0 was not loader-NULL");
    seed(s);auto saved=std::make_unique<Records>(s.screen_polygons);Observer observer(s,*saved);
    for(const auto count:std::array<std::uint32_t,5>{0u,1u,255u,256u,0xffffffffu}){
        s.screen_polygon_count=count;observer.count=count;
        observer.pc=0x42e572;observer.null=true;observer.stop=false;
        {NoHeap guard;check(reset_source_scene_screen_base_42e572(weak,&observer).next_required_instruction==0x42e578,"reset frontier");}
        {auto access=lock_screen_polygon_owner(weak);
            check(access.base_is_null() && &access.count_word()==&s.screen_polygon_count,"NULL/count owner mismatch");
            check(access.retained_records().data()==s.screen_polygons.data(),"NULL loses actual retained owner");
            rejects_host([&]{(void)access.published_records();});}
        observer.pc=0x42e4dc;observer.null=false;
        {NoHeap guard;
            check(publish_source_screen_base_42e4dc(weak,&observer).next_required_instruction==0x42e4e1,"publication frontier");
            (void)publish_source_screen_base_42e4dc(weak,&observer); //already-live store must remain observable
            auto access=lock_screen_polygon_owner(weak);
            check(!access.base_is_null()&&access.published_records().data()==s.screen_polygons.data(),"offset0 confused withNULL or wrong owner");
            check(access.published_records().size()==256&&access.count_word()==count,"publication modified count/capacity");
        }
        observer.pc=0x42e572;observer.null=true;observer.stop=true;
        bool stopped=false;try{(void)reset_source_scene_screen_base_42e572(weak,&observer);}catch(const Stop&){stopped=true;}
        check(stopped&&s.screen_base.is_null(),"reset observer failure rolled back store");
        observer.pc=0x42e4dc;observer.null=false;
        stopped=false;try{(void)publish_source_screen_base_42e4dc(weak,&observer);}catch(const Stop&){stopped=true;}
        check(stopped&&!s.screen_base.is_null(),"publication observer failure rolled back store");
        observer.pc=0x42e572;observer.null=true;observer.stop=false;
        {NoHeap guard;(void)reset_source_scene_screen_base_42e572(weak,&observer);(void)reset_source_scene_screen_base_42e572(weak,&observer);}
    }
    // Thread rejection occurs before state/observer access. No worker owns the
    // last platform reference, and no throwing check runs on that worker.
    const auto before_stores=stores;unsigned thread_rejections{};
    {auto live_access=lock_screen_polygon_owner(weak);
    std::thread foreign([&]{
        try{(void)reset_source_scene_screen_base_42e572(weak,&observer);}catch(const FormatError&){++thread_rejections;}
        try{(void)publish_source_screen_base_42e4dc(weak,&observer);}catch(const FormatError&){++thread_rejections;}
        try{auto access=lock_screen_polygon_owner(weak);(void)access;}catch(const FormatError&){++thread_rejections;}
        try{(void)live_access.base_is_null();}catch(const FormatError&){++thread_rejections;}
        try{(void)live_access.retained_records();}catch(const FormatError&){++thread_rejections;}
        try{(void)live_access.published_records();}catch(const FormatError&){++thread_rejections;}
        try{(void)live_access.count_word();}catch(const FormatError&){++thread_rejections;}
    });foreign.join();}
    check(thread_rejections==7&&stores==before_stores,"wrong-thread admission changed source/observer");
    retained(s,*saved,observer.count,observer.allocation);rejects+=thread_rejections;
    auto other=std::make_shared<RaceSessionPlatform>(SourceColdConstructionOnly{},argv[1],0.0f);
    {auto first=lock_screen_polygon_owner(weak);auto second=lock_screen_polygon_owner(other);
        check(first.retained_records().data()!=second.retained_records().data(),"worlds share screen owner");
        (void)publish_source_screen_base_42e4dc(other);
        check(first.base_is_null()&&!second.base_is_null(),"publication crossed worlds");}
    other.reset();
    // The transient access uses the existing control block and keeps the
    // actual world alive until its scope ends, with no Session construction.
    {auto access=lock_screen_polygon_owner(weak);platform.reset();
        check(!weak.expired()&&access.base_is_null(),"access did not retain actual owner");
        check(std::memcmp(access.retained_records().data(),saved->data(),sizeof(Records))==0,"lease changed payload");}
    check(weak.expired(),"access leaked a strong world cycle");
    rejects_host([&]{(void)reset_source_scene_screen_base_42e572(weak);});
    rejects_host([&]{(void)publish_source_screen_base_42e4dc(weak);});
    rejects_host([&]{auto access=lock_screen_polygon_owner(weak);(void)access;});
    check(stores==35,"missing or replayed source store events");
    std::cout<<"{\"passed\":true,\"checks\":"<<checks<<",\"source_store_events\":"<<stores
             <<",\"host_rejections\":"<<rejects<<",\"screen_records_compared\":256,\"screen_payload_bytes\":69632,\"next_reset_pc\":\"42E578\",\"source13alloc_claim\":false,\"full_Session_calls\":0}\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
