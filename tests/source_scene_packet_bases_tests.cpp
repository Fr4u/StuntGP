#include "sgn/source_scene_packet_bases.hpp"
#include "sgn/race_session.hpp"
#include <algorithm>
#include <atomic>
#include <functional>
#include <iostream>
#include <thread>

using namespace sgn;using namespace sgn::retail;
namespace {
unsigned checks{},boundaries{};
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);++checks;}
struct Stop{};
template<class F>void rejects(F call){bool bad=false;try{call();}catch(const FormatError&){bad=true;}check(bad,"host guard did not reject");}
template<class T>Bytes image(const T& value){const auto* p=reinterpret_cast<const std::uint8_t*>(&value);return {p,p+sizeof(value)};}
struct FrameObserver final:WorldPolygonFrameStoreObserver {
    std::function<void(unsigned,std::uint32_t,std::uint32_t,const WorldPolygonFrame&)> verify;
    unsigned stop{},seen{};
    void stored(std::uint32_t pc,std::uint32_t address,const WorldPolygonFrame& f) override {
        verify(++seen,pc,address,f);if(stop==seen)throw Stop{};
    }
};
void bases(const std::filesystem::path& root){
    auto platform=std::make_shared<RaceSessionPlatform>(SourceColdConstructionOnly{},root,0.0f);
    auto& state=platform->state();auto& f=state.polygon_frame;
    allocate_world_polygon_frame(f);attach_world_polygon_level(f);auto& a=*f.allocations;
    const auto ticket=capture_world_polygon_level(f);
    for(unsigned p=0;p<4;++p){f.packets[p].publish_current(a.packet_lifetimes[p].pointer(17+p));f.totals[p]=91+p;
        for(unsigned g=0;g<4;++g)f.groups[g][p]={a.packet_lifetimes[p].pointer(7+g+p),3+g};
        a.packets[p][43].inside_mask=0xa5010300u+p;}
    state.screen_polygon_count=71;state.light_vertex_offset=89;
    const auto payload=image(a);const auto groups=f.groups;const auto totals=f.totals;
    constexpr std::array<unsigned,4> order{2,3,0,1},pc{0x42e55a,0x42e560,0x42e566,0x42e56c},address{0x61c5f4,0x61c5f8,0x61c5fc,0x61c600};
    for(unsigned stop=0;stop<=4;++stop){
        for(unsigned p=0;p<4;++p)f.packets[p].publish_base(a.packet_lifetimes[p].pointer(5+p));
        const auto before=image(state);const auto offset=static_cast<std::size_t>(reinterpret_cast<const std::uint8_t*>(&f.packets)-reinterpret_cast<const std::uint8_t*>(&state));
        FrameObserver observer;observer.stop=stop;
        observer.verify=[&](unsigned n,unsigned ip,unsigned target,const WorldPolygonFrame& actual){
            check(&actual==&f&&ip==pc[n-1]&&target==address[n-1],"packet base store PC/address/order");
            for(unsigned p=0;p<4;++p){const auto cleared=std::find(order.begin(),order.begin()+n,p)!=order.begin()+n;
                check(f.packets[p].base().is_null()==cleared,"packet base prefix differs");
                if(!cleared)check(f.packets[p].base().same_pointer(a.packet_lifetimes[p].pointer(5+p)),"uncleared base changed");
                check(f.packets[p].current().same_pointer(a.packet_lifetimes[p].pointer(17+p)),"base clear changed current");
                for(unsigned g=0;g<4;++g)check(f.groups[g][p].first.same_pointer(groups[g][p].first)&&f.groups[g][p].count==groups[g][p].count,"base clear changed group");}
            check(f.totals==totals&&image(a)==payload,"base clear changed total or allocation payload/metadata");
            const auto after=image(state);
            check(std::equal(before.begin(),before.begin()+offset,after.begin())&&std::equal(before.begin()+offset+sizeof(f.packets),before.end(),after.begin()+offset+sizeof(f.packets)),"base clear changed unrelated world bytes");
            validate_world_polygon_level(f,ticket);
        };
        bool stopped=false;try{check(reset_source_scene_packet_bases_42e55a(platform,&observer).next_required_instruction==0x42e572,"incorrect next boundary");}catch(const Stop&){stopped=true;}
        check(stopped==(stop!=0)&&observer.seen==(stop?stop:4),"base interruption boundary");if(stop)++boundaries;
    }
    // Owner was retained: current remains usable after every base was cleared.
    f.packets[0].current().record().inside_mask=0x12345678;
    check(a.packets[0][17].inside_mask==0x12345678,"cleared base hid retained current allocation");
    rejects([&]{(void)f.packets[0].count();}); // mixed NULL/live is outside the supported native difference domain
    const auto old=f.packets[0].current();auto retained=std::move(f.allocations);
    allocate_world_polygon_frame(f);
    check(old.record().inside_mask==0x12345678&&f.packets[0].current().same_pointer(old),"allocation replacement normalized old current");
    rejects([&]{old.require_storage(f.allocations->packet_lifetimes[0]);});
    retained.reset();rejects([&]{(void)old.record();});
    auto cold=std::make_shared<RaceSessionPlatform>(SourceColdConstructionOnly{},root,0.0f);
    reset_source_scene_packet_bases_42e55a(cold);check(!cold->state().polygon_frame.allocations,"unallocated reset created packet payload");
    RaceSession session(SourceColdAttachmentOnly{},cold);rejects([&]{(void)session.models();});rejects([&]{(void)session.scene_resources();});
    std::atomic<unsigned> rejected{};std::thread worker([&]{try{reset_source_scene_packet_bases_42e55a(cold);}catch(const std::logic_error&){++rejected;}});worker.join();check(rejected==1,"foreign-thread base reset admitted");
    std::weak_ptr<RaceSessionPlatform> expired;{auto p=std::make_shared<RaceSessionPlatform>(SourceColdConstructionOnly{},root,0.0f);expired=p;}
    rejects([&]{reset_source_scene_packet_bases_42e55a(expired);});
}
void frame_words(){
    WorldPolygonFrame f;allocate_world_polygon_frame(f);attach_world_polygon_level(f);auto& a=*f.allocations;
    for(unsigned stop=1;stop<=36;++stop){
        for(unsigned p=0;p<4;++p){f.packets[p].publish_base(a.packet_lifetimes[p].pointer(2+p));f.packets[p].publish_current(a.packet_lifetimes[p].pointer(9+p));f.totals[p]=81+p;
            for(unsigned g=0;g<4;++g)f.groups[g][p]={a.packet_lifetimes[p].pointer(5),77};}
        if(stop%2)f.packets[2].clear_base();
        FrameObserver observer;observer.stop=stop;
        observer.verify=[&](unsigned n,unsigned pc,unsigned address,const WorldPolygonFrame&){
            constexpr std::array<unsigned,4> cursor_pc{0x42b120,0x42b12a,0x42b130,0x42b136};
            check(pc==(n<=32?0x42b10d:cursor_pc[n-33])&&address==(n<=32?0x6243e0+(n-1)*4:0x6243c0+(n-33)*8),"42B100 store ordering");
            for(unsigned g=0;g<4;++g)for(unsigned p=0;p<4;++p){const auto i=g*8+p*2;
                check(f.groups[g][p].first.is_null()==(i<n)&&f.groups[g][p].count==(i+1<n?0u:77u),"REP partial first/count state");}
            for(unsigned p=0;p<4;++p){check(f.packets[p].current().same_pointer(n>32+p?f.packets[p].base():a.packet_lifetimes[p].pointer(9+p)),"42B100 premature current publication");check(f.totals[p]==81+p,"42B100 cleared totals");}
        };
        try{reset_world_polygon_groups(f,&observer);}catch(const Stop&){}check(observer.seen==stop,"REP interruption missing");++boundaries;
    }
    for(unsigned p=0;p<4;++p){f.packets[p].publish_base(a.packet_lifetimes[p].pointer(12));f.packets[p].publish_current(a.packet_lifetimes[p].pointer(7));f.groups[1][p]={a.packet_lifetimes[p].pointer(9),0};}
    finish_world_polygon_group(f,1);publish_world_polygon_totals(f);
    for(unsigned p=0;p<4;++p)check(f.groups[1][p].count==0xfffffffeu&&f.totals[p]==0xfffffffbu,"signed /20 was clamped");
    f.groups[0][0]={a.packet_lifetimes[0].pointer(17),2};f.packets[0].clear_base();
    check(world_polygon_group_packets(f,0,0).data()==a.packets[0].data()+17,"group start was recomputed from base");
    f.packets[1].publish_base(a.packet_lifetimes[1].pointer(11));f.totals[1]=3;
    check(world_polygon_alpha_packets(f,1).data()==a.packets[1].data()+11,"alpha did not use its independent base");
    f.groups[0][0]={a.packet_lifetimes[0].pointer(3072),0};check(world_polygon_group_packets(f,0,0).empty(),"live one-past empty group rejected");
    f.groups[0][0].count=1;rejects([&]{(void)world_polygon_group_packets(f,0,0);});
    f.groups[0][0]={a.packet_lifetimes[1].pointer(0),0};rejects([&]{(void)world_polygon_group_packets(f,0,0);});
    rejects([&]{(void)world_polygon_packet_difference(a.packet_lifetimes[0].pointer(),a.packet_lifetimes[1].pointer());});
    check(world_polygon_packet_difference({},{})==0,"NULL/NULL difference");
    rejects([&]{(void)a.packet_lifetimes[0].pointer(3073);});rejects([&]{(void)WorldPolygonPacketStorageLifetime::checked_extent(WorldPolygonPacketStorageLifetime::maximum_elements+1);});
    detach_world_polygon_level(f);rejects([&]{(void)world_polygon_packet_pool(f,0);});
}
void ordered_group_and_allocation_stores(){
    constexpr std::array<unsigned,8> begin_pc{0x42b14b,0x42b154,0x42b157,0x42b161,0x42b164,0x42b167,0x42b16a,0x42b16d};
    constexpr std::array<unsigned,8> begin_word{6,4,0,5,7,1,2,3};
    constexpr std::array<unsigned,4> finish_pool{2,3,0,1},finish_pc{0x42b19f,0x42b1bd,0x42b1dc,0x42b1f8};
    constexpr std::array<unsigned,4> total_pc{0x42b22a,0x42b24f,0x42b274,0x42b28e};
    constexpr std::array<unsigned,4> allocation_pc{0x42e498,0x42e4a9,0x42e4ba,0x42e4cb},allocation_address{0x61c5f4,0x61c5f8,0x61c5fc,0x61c600};
    for(unsigned operation=0;operation<4;++operation)for(unsigned stop=1;stop<=(operation==0?8u:4u);++stop){
        WorldPolygonFrame f;allocate_world_polygon_frame(f);auto& a=*f.allocations;
        std::array<WorldPolygonPacketPointer,4> bases,currents;
        for(unsigned p=0;p<4;++p){bases[p]=a.packet_lifetimes[p].pointer(18);currents[p]=a.packet_lifetimes[p].pointer(7+p);
            f.packets[p].publish_base(bases[p]);f.packets[p].publish_current(currents[p]);f.groups[2][p]={a.packet_lifetimes[p].pointer(11),91};f.totals[p]=97;}
        const auto old_groups=f.groups;std::array<bool,8> written{};
        FrameObserver observer;observer.stop=stop;
        observer.verify=[&](unsigned n,unsigned pc,unsigned address,const WorldPolygonFrame&){
            if(operation==0){
                const auto word=begin_word[n-1];written[word]=true;
                check(pc==begin_pc[n-1]&&address==0x624420+word*4,"42B140 store order");
                for(unsigned p=0;p<4;++p)check(f.groups[2][p].first.same_pointer(written[p*2]?currents[p]:old_groups[2][p].first)&&f.groups[2][p].count==(written[p*2+1]?0u:91u),"42B140 partial first/count");
            }else if(operation==1){
                const auto p=finish_pool[n-1];written[p]=true;
                check(pc==finish_pc[n-1]&&address==0x624424+p*8,"42B180 count store order");
                for(unsigned i=0;i<4;++i)check(f.groups[2][i].first.same_pointer(old_groups[2][i].first)&&f.groups[2][i].count==(written[i]?0xfffffffcu+i:91u),"42B180 partial signed counts");
            }else if(operation==2){
                check(pc==total_pc[n-1]&&address==0x6243c4+(n-1)*8,"42B200 total store order");
                for(unsigned p=0;p<4;++p)check(f.totals[p]==(p<n?0xfffffff5u+p:97u),"42B200 partial signed totals");
            }else{
                const auto p=finish_pool[n-1];written[p]=true;
                check(pc==allocation_pc[n-1]&&address==allocation_address[n-1],"42E430 packet base publication order");
                for(unsigned i=0;i<4;++i)check(f.packets[i].base().same_pointer(written[i]?f.allocations->packet_lifetimes[i].pointer():bases[i]),"allocation partial packet bases");
            }
            for(unsigned p=0;p<4;++p){check(f.packets[p].current().same_pointer(currents[p]),"group/allocation rewrote packet current");
                if(operation!=3)check(f.packets[p].base().same_pointer(bases[p]),"group helper rewrote base");
                if(operation!=2)check(f.totals[p]==97,"group/allocation rewrote total");
                if(operation>=2)check(f.groups[2][p].first.same_pointer(old_groups[2][p].first)&&f.groups[2][p].count==91,"total/allocation rewrote group");}
        };
        try{if(operation==0)begin_world_polygon_group(f,2,&observer);else if(operation==1)finish_world_polygon_group(f,2,&observer);else if(operation==2)publish_world_polygon_totals(f,&observer);else allocate_world_polygon_frame(f,&observer);}catch(const Stop&){}
        check(observer.seen==stop,"group/allocation interruption absent");++boundaries;
    }
}
void invocation_lifetime(const std::filesystem::path& root){
    auto external=std::make_shared<RaceSessionPlatform>(SourceColdConstructionOnly{},root,0.0f);
    const std::weak_ptr<RaceSessionPlatform> weak=external;FrameObserver observer;
    const auto* frame=&external->state().polygon_frame;
    observer.verify=[&](unsigned n,unsigned,unsigned,const WorldPolygonFrame& actual){if(n==1)external.reset();check(!weak.expired()&&&actual==frame,"base prefix lost platform during callback");};
    reset_source_scene_packet_bases_42e55a(weak,&observer);check(observer.seen==4&&weak.expired(),"base prefix retained platform beyond invocation");
}
}
int main(int argc,char** argv)try{
    check(argc==2,"expected NEW retail root");bases(argv[1]);frame_words();ordered_group_and_allocation_stores();invocation_lifetime(argv[1]);
    std::cout<<"{\"passed\":true,\"checks\":"<<checks<<",\"interruptions\":"<<boundaries<<",\"next_required_instruction\":\"42E572\",\"source_allocation_claims\":0,\"native_runs\":0}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
