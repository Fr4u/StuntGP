#include "sgn/source_scene_indexed_base.hpp"
#include "sgn/race_session.hpp"
#include "source_allocation_begin_witness.hpp"
#include <algorithm>
#include <atomic>
#include <iostream>
#include <thread>

using namespace sgn;
using namespace sgn::retail;
namespace {
unsigned checks{},rejections{},stores{},combinations{},appends{};
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
template<class F>void rejects(F call){bool rejected=false;try{call();}catch(const std::exception&){rejected=true;}check(rejected,"expected host guard");++rejections;}
template<class T>Bytes bytes(const T& value){const auto* p=reinterpret_cast<const std::uint8_t*>(&value);return {p,p+sizeof(value)};}
struct BankImage {
    const SourceAllocationBank& bank;
    allocation_begin_receiving::Words words;
    std::array<SourceAllocationRecord,384> rows;
    std::array<Bytes,384> payloads;
    explicit BankImage(const SourceAllocationBank& b):bank(b),words(allocation_begin_receiving::words(b)){
        std::ranges::copy(b.source_records(),rows.begin());
        for(unsigned i=0;i<384;++i)if(rows[i].raw){const auto* p=reinterpret_cast<const std::uint8_t*>(rows[i].aligned);payloads[i].assign(p,p+rows[i].requested_bytes);}
    }
    void verify() const {
        check(words==allocation_begin_receiving::words(bank),"indexed clear changed bank words");
        for(unsigned i=0;i<384;++i){
            check(allocation_begin_receiving::same(rows[i],bank.source_records()[i]),"indexed clear changed bank row");
            if(rows[i].raw)check(std::equal(payloads[i].begin(),payloads[i].end(),reinterpret_cast<const std::uint8_t*>(rows[i].aligned)),"indexed clear changed bank payload");
        }
    }
};
struct Snapshot {
    RaceSessionState& state;
    Bytes world,payload;
    WorldPolygonFrameAllocations* allocation;
    WorldIndexedVertexPointer base,current;
    BankImage bank;
    std::size_t cursor_begin,cursor_end;
    explicit Snapshot(RaceSessionPlatform& platform):state(platform.state()),world(bytes(state)),
        payload(bytes(*state.polygon_frame.allocations)),allocation(state.polygon_frame.allocations.get()),
        base(state.polygon_frame.indexed.base()),current(state.polygon_frame.indexed.current()),bank(platform.source_allocations()),
        cursor_begin(static_cast<std::size_t>(reinterpret_cast<std::uintptr_t>(&state.polygon_frame.indexed)-reinterpret_cast<std::uintptr_t>(&state))),
        cursor_end(cursor_begin+sizeof(state.polygon_frame.indexed)){}
    void cleared() const {
        check(state.polygon_frame.indexed.base().is_null(),"42E53C did not clear base");
        check(state.polygon_frame.indexed.current().same_pointer(current),"42E53C changed current pointer");
        check(state.polygon_frame.allocations.get()==allocation,"42E53C replaced payload owner");
        check(bytes(*allocation)==payload,"42E53C changed typed allocation payload/metadata");
        const auto after=bytes(state);
        check(std::equal(world.begin(),world.begin()+static_cast<std::ptrdiff_t>(cursor_begin),after.begin()),"42E53C changed world prefix");
        check(std::equal(world.begin()+static_cast<std::ptrdiff_t>(cursor_end),world.end(),after.begin()+static_cast<std::ptrdiff_t>(cursor_end)),"42E53C changed world suffix");
        bank.verify();
    }
};
struct Observer final:SourceSceneIndexedBaseObserver {
    Snapshot& snapshot;const WorldPolygonFrame& expected;bool interrupt;
    Observer(Snapshot& s,const WorldPolygonFrame& f,bool stop):snapshot(s),expected(f),interrupt(stop){}
    void cleared(std::uint32_t pc,std::uint32_t address,const WorldPolygonFrame& actual) override {
        check(pc==0x42e53c&&address==0x61c5ec&&&actual==&expected,"wrong one-store boundary");
        ++stores;snapshot.cleared();if(interrupt)throw std::runtime_error("intentional after-store interruption");
    }
};
void transform(WorldIndexedVertexCursor& cursor,std::span<WorldPolygonVertex> vertices,bool clipped,unsigned stamp){
    const std::array<std::uint16_t,3> indices{0,1,0xffff};
    const std::array<WorldSpatialBatch,1> batches{{{{},indices}}};
    const std::array<VehicleVector,2> source{{{2,4,8,0},{4,8,16,0}}};
    const auto matrix=vehicle_identity();const ProjectionParameters projection{1,1,1,0,1};
    if(clipped)transform_world_spatial_vertices_clipped(batches,source,vertices,cursor,stamp,matrix,projection);
    else transform_world_spatial_vertices_inside(batches,source,vertices,cursor,stamp,matrix,projection);
}
void matrix(const std::filesystem::path& root){
    for(unsigned bits=0;bits<4;++bits){
        ++combinations;auto platform=std::make_shared<RaceSessionPlatform>(SourceColdConstructionOnly{},root,0.0f);
        auto& s=platform->state();auto& frame=s.polygon_frame;auto& cursor=frame.indexed;
        check(cursor.base().is_null()&&cursor.current().is_null(),"PE indexed words not independent NULL");
        allocate_world_polygon_frame(frame);check(!cursor.base().is_null()&&cursor.current().is_null(),"42E487 also wrote current");
        auto& allocation=*frame.allocations;cursor.publish_current(allocation.indexed_lifetime.pointer(7));
        if(!(bits&1))cursor.clear_base();if(!(bits&2))cursor.clear_current();
        if(bits==0)check(cursor.count()==0,"NULL/NULL difference must be zero");
        else if(bits==3)check(cursor.count()==7,"same-owner pointer difference");
        else rejects([&]{(void)cursor.count();});
        s.startup.fields_62dcf0.fill(0xabcdef12);s.startup.fields_62d480.fill(0x13579bdf);
        s.view_matrices.count=77;s.view_matrices.cursor=123;frame.frame_stamp=456;frame.spatial_batch_count=789;
        allocation.indexed[0]=&allocation.vertices[9];allocation.vertices[9].words.fill(0x92bad417);
        auto& bank=platform->source_allocations();bank.initialize_42eab0();const auto retained=bank.allocate_42e8d0(37,1);
        std::ranges::fill(bank.bytes(retained),std::uint8_t{0x39});
        attach_world_polygon_level(frame);const auto ticket=capture_world_polygon_level(frame);
        Snapshot before(*platform);Observer observer(before,frame,false);
        check(reset_source_scene_indexed_base_42e53c(platform,&observer).next_required_instruction==0x42e542,"advanced beyond one store");
        before.cleared();validate_world_polygon_level(frame,ticket);
        check(reset_source_scene_indexed_base_42e53c(platform,&observer).next_required_instruction==0x42e542,"repeat changed boundary");
        before.cleared();
        Observer interrupted(before,frame,true);rejects([&]{reset_source_scene_indexed_base_42e53c(platform,&interrupted);});before.cleared();
        const auto payload_before=bytes(allocation.vertices);
        if(bits&2){
            transform(cursor,allocation.vertices,false,1);check(cursor.current().offset()==9,"inside append after NULL base");
            transform(cursor,allocation.vertices,true,2);check(cursor.current().offset()==11,"clipped append after NULL base");
            check(cursor.base().is_null()&&allocation.indexed[7]==&allocation.vertices[0]&&allocation.indexed[10]==&allocation.vertices[1],"append did not use retained current");appends+=4;
            rejects([&]{(void)world_polygon_indexed_projection_vertices(frame);});
        }else{
            rejects([&]{transform(cursor,allocation.vertices,false,1);});
            rejects([&]{transform(cursor,allocation.vertices,true,2);});
            check(bytes(allocation.vertices)==payload_before&&cursor.current().is_null(),"NULL current guard wrote geometry/current");
        }
        cursor.copy_base_to_current();check(cursor.current().is_null(),"copy of NULL base did not overwrite current");
        check(world_polygon_indexed_projection_vertices(frame).empty(),"NULL/NULL projection not empty");
        cursor.publish_base(allocation.indexed_lifetime.pointer(3));cursor.publish_current(allocation.indexed_lifetime.pointer(7));
        check(cursor.count()==4,"nonzero base offset count");
        frame.transformed.count=81;reset_world_polygon_vertex_cursors(frame);
        check(frame.transformed.count==0&&cursor.current().same_pointer(cursor.base())&&cursor.current().offset()==3,"42E800 did not copy actual base");
        cursor.publish_current(allocation.indexed_lifetime.pointer(2));check(cursor.count()==0,"negative source difference did not take JLE");
        validate_world_polygon_level(frame,ticket);bank.bytes(retained)[0]=0x39;
    }
}
void lifetime_controls(const std::filesystem::path& root){
    auto platform=std::make_shared<RaceSessionPlatform>(SourceColdConstructionOnly{},root,0.0f);
    auto& frame=platform->state().polygon_frame;allocate_world_polygon_frame(frame);attach_world_polygon_level(frame);
    frame.indexed.copy_base_to_current();const auto old_ticket=capture_world_polygon_level(frame);const auto old_current=frame.indexed.current();
    allocate_world_polygon_frame(frame);check(frame.indexed.current().same_pointer(old_current),"allocation publication overwrote old current");
    rejects([&]{validate_world_polygon_level(old_ticket);});rejects([&]{transform(frame.indexed,frame.allocations->vertices,false,1);});
    rejects([&]{(void)frame.indexed.projection_vertices();});frame.indexed.copy_base_to_current();attach_world_polygon_level(frame);
    const auto current_ticket=capture_world_polygon_level(frame);WorldPolygonFrame other;allocate_world_polygon_frame(other);attach_world_polygon_level(other);
    rejects([&]{validate_world_polygon_level(other,current_ticket);});
    frame.indexed.publish_current(other.allocations->indexed_lifetime.pointer(1));rejects([&]{(void)frame.indexed.projection_vertices();});
    // A foreign base cannot block the independent valid current append.
    frame.indexed.publish_base(other.allocations->indexed_lifetime.pointer());
    frame.indexed.publish_current(frame.allocations->indexed_lifetime.pointer());transform(frame.indexed,frame.allocations->vertices,false,3);
    check(frame.indexed.current().offset()==2,"append incorrectly required base identity");
    frame.indexed.publish_current(other.allocations->indexed_lifetime.pointer(1));
    rejects([&]{(void)world_polygon_indexed_projection_vertices(frame);}); //both pointers foreign to this frame
    frame.indexed.publish_base(frame.allocations->indexed_lifetime.pointer());frame.indexed.copy_base_to_current();
    auto end=frame.allocations->indexed_lifetime.pointer(frame.allocations->indexed.size());rejects([&]{end.append(&frame.allocations->vertices[0]);});
    rejects([&]{(void)frame.allocations->indexed_lifetime.pointer(frame.allocations->indexed.size()+1);});
    frame.transformed.count=81;const auto frame_before=bytes(frame),allocation_before=bytes(*frame.allocations);
    std::atomic<unsigned> thread_rejected{};
    std::thread foreign([&]{
        try{reset_source_scene_indexed_base_42e53c(platform);}catch(const std::exception&){++thread_rejected;}
        try{auto p=frame.indexed.current();p.append(nullptr);}catch(const std::exception&){++thread_rejected;}
        try{(void)world_polygon_indexed_projection_vertices(frame);}catch(const std::exception&){++thread_rejected;}
        try{allocate_world_polygon_frame(frame);}catch(const std::exception&){++thread_rejected;}
        try{reset_world_polygon_vertex_cursors(frame);}catch(const std::exception&){++thread_rejected;}
        try{transform(frame.indexed,frame.allocations->vertices,false,4);}catch(const std::exception&){++thread_rejected;}
    });foreign.join();check(thread_rejected==6,"owner-thread guards missing");
    check(bytes(frame)==frame_before&&bytes(*frame.allocations)==allocation_before,"foreign thread changed storage/frame before rejection");
    const auto expired=frame.indexed.current();frame.allocations.reset();
    auto attempt=expired;rejects([&]{attempt.append(nullptr);});
    check(reset_source_scene_indexed_base_42e53c(platform).next_required_instruction==0x42e542,"clear must not dereference expired binding");
    check(frame.indexed.current().same_pointer(expired),"clear normalized expired current");
    std::weak_ptr<RaceSessionPlatform> dead=platform;platform.reset();rejects([&]{reset_source_scene_indexed_base_42e53c(dead);});
    rejects([&]{validate_world_polygon_level(current_ticket);});
}
void unallocated_and_source_guards(const std::filesystem::path& root){
    auto platform=std::make_shared<RaceSessionPlatform>(SourceColdConstructionOnly{},root,0.0f);
    auto& state=platform->state();const auto before=bytes(state);check(!platform->source_allocations().initialized(),"fixture unexpectedly initialized bank");
    check(reset_source_scene_indexed_base_42e53c(platform).next_required_instruction==0x42e542,"unallocated NULL base not clearable");
    check(bytes(state)==before&&!platform->source_allocations().initialized(),"NULL clear modified source world or bank");
    rejects([&]{(void)platform->assets();});rejects([&]{(void)platform->source_scene_resources();});
    RaceSession session(SourceColdAttachmentOnly{},platform);rejects([&]{(void)session.scene_resources();});rejects([&]{(void)session.models();});
    reset_source_scene_indexed_base_42e53c(platform);rejects([&]{(void)session.scene_resources();});
}
void projection_and_publication(){
    constexpr auto limit=WorldIndexedVertexStorageLifetime::maximum_native_projection_elements;
    check(limit==0x1fffffffu,"native signed projection domain uses wrong source stride");
    check(WorldIndexedVertexStorageLifetime::checked_native_projection_extent(limit)==limit,"exact native extent limit rejected");
    check(WorldIndexedVertexStorageLifetime::checked_native_projection_extent(0)==0,"zero native extent rejected");
    rejects([&]{(void)WorldIndexedVertexStorageLifetime::checked_native_projection_extent(limit+1);});
    rejects([&]{(void)WorldIndexedVertexStorageLifetime::checked_native_projection_extent(std::numeric_limits<std::size_t>::max());});
    WorldPolygonFrame frame;allocate_world_polygon_frame(frame);attach_world_polygon_level(frame);
    auto& a=*frame.allocations;auto& cursor=frame.indexed;
    auto& v=a.vertices[4];v.words.fill(0x81234567);v.words[0]=std::bit_cast<unsigned>(8.0f);v.words[1]=std::bit_cast<unsigned>(4.0f);v.words[7]=std::bit_cast<unsigned>(3.0f);
    const auto before=v.words;a.indexed[0]=&a.vertices[3];a.indexed[1]=&v;
    cursor.publish_base(a.indexed_lifetime.pointer(1));cursor.publish_current(a.indexed_lifetime.pointer(2));
    ViewportState view;view.center_x=10;view.center_y=20;view.scale_x=2;view.scale_y=3;
    project_world_polygon_vertices(world_polygon_indexed_projection_vertices(frame),{},{},view);
    check(std::bit_cast<float>(v.words[0])==14&&std::bit_cast<float>(v.words[1])==23&&std::bit_cast<float>(v.words[7])==8,"actual projection used wrong base/range");
    for(unsigned i=2;i<7;++i)check(v.words[i]==before[i],"projection changed retained word");
    check(a.vertices[3].words==std::array<std::uint32_t,8>{},"projection visited element before base");
    std::array<WorldPolygonVertex,2> vertices{};std::array<WorldPolygonVertex*,1> pointers{};
    WorldIndexedVertexStorageLifetime lifetime(pointers);WorldIndexedVertexCursor partial;
    partial.publish_base(lifetime.pointer());partial.copy_base_to_current();
    rejects([&]{transform(partial,vertices,false,1);});
    check(partial.current().offset()==0&&pointers[0]==&vertices[0]&&(vertices[0].words[4]>>16)==1&&vertices[1].words[4]==0,
        "global current published before429868 or partial payload order changed");
}
}
int main(int argc,char** argv)try{
    check(argc==2,"expected NEW retail root");matrix(argv[1]);lifetime_controls(argv[1]);unallocated_and_source_guards(argv[1]);projection_and_publication();
    std::cout<<"{\"passed\":true,\"base_current_combinations\":"<<combinations<<",\"observed_single_stores\":"<<stores
        <<",\"appends_after_base_clear\":"<<appends<<",\"checks\":"<<checks<<",\"host_rejections\":"<<rejections
        <<",\"next_required_instruction\":\"42E542\",\"source_allocation_claims\":0,\"native_runs\":0}\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
