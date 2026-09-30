// AUTHORED ONLY. Compile/run only in a consistently rebuilt receiving ABI.
#include "sgn/source_screen_base.hpp"
#include "sgn/race_session.hpp"
#include "sgn/retail_screen_polygon_submission.hpp"
#include <bit>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <new>
#include <stdexcept>

thread_local bool screen_forbid_allocations=false;
void* operator new(std::size_t n){
    if(screen_forbid_allocations)throw std::bad_alloc();
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
unsigned checks{},cases{};
void check(bool v,const char* why){++checks;if(!v)throw std::runtime_error(why);}
struct NoHeap {
    bool old{screen_forbid_allocations};
    NoHeap(){screen_forbid_allocations=true;}
    ~NoHeap(){screen_forbid_allocations=old;}
};
struct Stop {};
template<class E,class F>void rejected(F&& f){bool caught=false;try{f();}catch(const E&){caught=true;}check(caught,"missing expected rejection");}
struct Event {unsigned kind{},a{},b{};};
struct Sink final:PolygonSubmissionSink {
    std::array<Event,1024> events{};unsigned size{},draws{};
    std::array<std::uint32_t,256> drawn_x{};
    void (*hook)(Sink&,Event){};
    RaceSessionState& state;
    std::weak_ptr<RaceSessionPlatform> platform;
    Sink(RaceSessionState& s,const std::shared_ptr<RaceSessionPlatform>& p):state(s),platform(p){}
    void event(Event e){check(size<events.size(),"event capacity");events[size++]=e;if(hook)hook(*this,e);}
    std::uint32_t set_render_state(std::uint32_t a,std::uint32_t b)override{event({1,a,b});return 0x80004005u;}
    std::uint32_t set_texture(std::uint32_t a,std::uint32_t b)override{event({2,a,b});return 0;}
    std::uint32_t set_texture_stage_state(std::uint32_t a,std::uint32_t b,std::uint32_t)override{event({3,a,b});return 0;}
    std::uint32_t draw_indexed(std::uint32_t,std::uint32_t,std::span<const PolygonSubmissionVertex>,std::span<const std::uint16_t>,std::uint32_t,const ViewportState&)override{throw std::runtime_error("unexpected indexed draw");}
    std::uint32_t draw(std::uint32_t type,std::uint32_t fvf,std::span<const PolygonSubmissionVertex> vertices,std::uint32_t flags,const ViewportState&)override{
        check(type==6&&fvf==0x2c4&&flags==0x14,"draw contract");
        check(draws<drawn_x.size(),"draw capacity");drawn_x[draws++]=vertices.empty()?0:vertices[0].words[0];
        event({4,static_cast<unsigned>(vertices.size()),flags});return 0;
    }
};
struct Probe {
    const ScreenPolygonOwnerAccess& access;
    RaceSessionState& state;
    Sink* sink{};
    mutable unsigned reads{};
    bool change_count{};
    std::uint32_t replacement_count{};
    void visit()const{
        ++reads;
        if(sink)check(sink->size>=1&&sink->events[0].kind==1&&sink->events[0].a==24&&sink->events[0].b==1,"base read preceded432F76");
        if(change_count)state.screen_polygon_count=replacement_count;
    }
    static std::span<const SourceScreenPolygon> read_const(const void* p){
        const auto& self=*static_cast<const Probe*>(p);self.visit();return self.access.published_records();
    }
    static std::span<SourceScreenPolygon> read_mutable(const void* p){
        const auto& self=*static_cast<const Probe*>(p);self.visit();return self.access.published_records();
    }
};
}
int main(int argc,char** argv)try{
    check(argc==2,"expected retail root for actual source-cold platform");
    auto platform=std::make_shared<RaceSessionPlatform>(SourceColdConstructionOnly{},argv[1],0.0f);
    auto& s=platform->state();auto access=lock_screen_polygon_owner(platform);
    auto saved=std::make_unique<std::array<SourceScreenPolygon,256>>();
    std::array<PolygonSubmissionVertex,8> scratch{};
    std::array<PolygonSubmissionMaterial,1> materials{{{123,0,0}}};
    s.viewport.clip_screen={0,640,0,480};s.viewport.clip_expanded={-100,740,-100,580};
    auto seed=[&]{
        for(unsigned i=0;i<256;++i){auto& p=s.screen_polygons[i];p={};p.alpha=255;
            for(unsigned j=0;j<8;++j){auto& v=p.vertices[j];v.x=float(i+10);v.y=float(j+10);v.z=.5f;v.retained_w=0xaabb0000u+i*8+j;v.u=.25f;v.v=.75f;v.color=0xff123456u;v.outcode=15;}}
        s.ui.selected_material=0xffffffffu;s.screen_polygon_count=0;
    };
    auto input=[] {SourceScreenPolygon p{};p.texture=0;p.vertex_count=3;p.alpha=255;
        for(unsigned i=0;i<3;++i){p.vertices[i].x=float(20+i);p.vertices[i].y=float(30+i);p.vertices[i].z=.5f;p.vertices[i].color=0xffffffffu;}return p;}();
    auto submit=[&](Probe& probe,Sink& sink){return submit_screen_polygons_432f60(
        {ConstScreenPolygonBaseReader::live(&probe,Probe::read_const),access.count_word(),s.ui.selected_material,materials,scratch,s.viewport},sink);};

    // The first callback repairs a deliberately invalid entry count and NULL
    // base. No screen count admission or base lookup may precede that callback.
    seed();s.screen_polygon_count=0xffffffffu;s.screen_polygons[0].flags=0x40;*saved=s.screen_polygons;
    check(s.screen_base.is_null(),"source-cold publication must be NULL");
    {Sink sink(s,platform);Probe probe{access,s,&sink};
        sink.hook=[](Sink& out,Event e){if(e.kind==1&&e.a==24&&e.b==1){out.state.screen_polygon_count=0;(void)publish_source_screen_base_42e4dc(out.platform);}};
        {NoHeap guard;const auto result=submit(probe,sink);check(result.entry_count==0&&result.submitted==0,"count captured before initial callback");}
        check(probe.reads==1&&sink.size==3&&sink.events[1].a==27&&sink.events[1].b==0&&sink.events[2].b==160,"zero count omitted trailing flags");
        check(std::memcmp(saved->data(),s.screen_polygons.data(),sizeof(*saved))==0,"submission changed retained records");++cases;
    }
    // A later backend callback clears the global base and grows the live count.
    // EDI remains captured once: both records come from the real original array.
    seed();s.screen_polygons[0].vertex_count=1;s.screen_polygons[1].vertex_count=1;
    s.screen_polygons[0].vertices[0].x=13;s.screen_polygons[1].vertices[0].x=29;s.screen_polygons[2].flags=0x40;
    {Sink sink(s,platform);Probe probe{access,s,&sink};
        sink.hook=[](Sink& out,Event e){
            if(e.kind==1&&e.a==24&&e.b==1)out.state.screen_polygon_count=1;
            if(e.kind==1&&e.a==27&&out.draws==0){(void)reset_source_scene_screen_base_42e572(out.platform);out.state.screen_polygon_count=2;}
        };
        {NoHeap guard;const auto result=submit(probe,sink);check(result.entry_count==1&&result.submitted==2,"later live count was not reread");}
        check(probe.reads==1&&sink.draws==2&&sink.drawn_x[0]==std::bit_cast<std::uint32_t>(13.0f)&&sink.drawn_x[1]==std::bit_cast<std::uint32_t>(29.0f),"EDI base changed during later callbacks");
        check(s.screen_base.is_null()&&s.screen_polygon_count==0,"submission republished base or failed count reset");++cases;
    }
    // Clearing at the FIRST callback must be seen, without touching scratch or
    // resetting count. NULL/count0 is also a dereference failure, not success.
    for(const auto count:std::array<std::uint32_t,2>{1u,0u}){
        seed();s.screen_polygon_count=count;(void)publish_source_screen_base_42e4dc(platform);scratch[0].words[0]=0xdeadbeefu;
        Sink sink(s,platform);Probe probe{access,s,&sink};
        sink.hook=[](Sink& out,Event e){if(e.kind==1&&e.a==24&&e.b==1)(void)reset_source_scene_screen_base_42e572(out.platform);};
        rejected<FormatError>([&]{(void)submit(probe,sink);});
        check(sink.size==1&&probe.reads==1&&s.screen_polygon_count==count&&scratch[0].words[0]==0xdeadbeefu,"NULL changed suffix state");++cases;
    }
    seed();s.screen_polygon_count=0xffffffffu;
    {Sink sink(s,platform);Probe probe{access,s,&sink};
        sink.hook=[](Sink&,Event){throw Stop{};};rejected<Stop>([&]{(void)submit(probe,sink);});
        check(probe.reads==0&&s.screen_polygon_count==0xffffffffu,"first callback failure did not stop before count/base");++cases;
    }
    // Diagnostic reader mutation proves count79 was captured before base7E.
    seed();(void)publish_source_screen_base_42e4dc(platform);s.screen_polygon_count=1;
    {Sink sink(s,platform);Probe probe{access,s,&sink,0,true,0};
        {NoHeap guard;const auto result=submit(probe,sink);check(result.entry_count==1&&result.submitted==1,"count79/base7E order reversed");}
        check(probe.reads==1,"submission reread base");++cases;
    }
    // Bounds are checked at actual dereferences. The retained owner has256
    // slots: count256/FFFFFFFF consumes256 records then rejects, no reset.
    for(const auto count:std::array<std::uint32_t,2>{256u,0xffffffffu}){
        seed();s.screen_polygon_count=count;Sink sink(s,platform);Probe probe{access,s,&sink};
        rejected<FormatError>([&]{(void)submit(probe,sink);});
        check(sink.draws==256&&probe.reads==1&&s.screen_polygon_count==count,"early count gate or missing retained bounds");++cases;
    }
    // Reuse one real queue binding across NULL and live publication changes.
    seed();s.screen_polygon_count=1;
    {Probe probe{access,s,nullptr,0,true,4};
        const SourceScreenPolygonQueue queue{MutableScreenPolygonBaseReader::live(&probe,Probe::read_mutable),access.count_word()};
        {NoHeap guard;check(append_screen_polygon(queue,input,s.viewport),"append rejected valid input");}
        check(probe.reads==1&&s.screen_polygon_count==5&&s.screen_polygons[1].vertex_count==3&&s.screen_polygons[4].vertex_count==0,"append count830/base838 order or live increment");
        probe.change_count=false;(void)reset_source_scene_screen_base_42e572(platform);*saved=s.screen_polygons;
        rejected<FormatError>([&]{(void)append_screen_polygon(queue,input,s.viewport);});
        check(probe.reads==2&&s.screen_polygon_count==5&&std::memcmp(saved->data(),s.screen_polygons.data(),sizeof(*saved))==0,"append cached a previously live base");
        (void)publish_source_screen_base_42e4dc(platform);
        {NoHeap guard;check(append_screen_polygon(queue,input,s.viewport),"republished append failed");}
        check(probe.reads==3&&s.screen_polygon_count==6&&s.screen_polygons[5].vertex_count==3,"append did not load base per invocation");++cases;
    }
    for(const auto count:std::array<std::uint32_t,2>{256u,0xffffffffu}){
        s.screen_polygon_count=count;*saved=s.screen_polygons;Probe probe{access,s};
        rejected<std::invalid_argument>([&]{(void)append_screen_polygon({MutableScreenPolygonBaseReader::live(&probe,Probe::read_mutable),access.count_word()},input,s.viewport);});
        check(probe.reads==1&&s.screen_polygon_count==count&&std::memcmp(saved->data(),s.screen_polygons.data(),sizeof(*saved))==0,"append bounds preceded base read or mutated payload");++cases;
    }
    s.screen_polygon_count=255;
    {NoHeap guard;check(append_screen_polygon({live_screen_polygon_reader(access),access.count_word()},input,s.viewport)&&s.screen_polygon_count==256,"last actual append slot");}++cases;
    // Explicit fixed-buffer fixture adapter, independently owned and named.
    {std::array<SourceScreenPolygon,2> records{};std::uint32_t count{},material{};
        Sink sink(s,platform);records[0].flags=0x40;
        const auto result=submit_screen_polygons_432f60({ConstScreenPolygonBaseReader::fixed_fixture(records),count,material,materials,scratch,s.viewport},sink);
        check(result.entry_count==0&&sink.size==3,"explicit fixed fixture contract");++cases;
    }
    check(cases==13,"missing authored consumer scenario");
    std::cout<<"{\"passed\":true,\"cases\":"<<cases<<",\"checks\":"<<checks<<",\"full_Session_calls\":0}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
