#include "sgn/retail_world_spatial_producer.hpp"
#include <atomic>
#include <iostream>
#include <thread>

using namespace sgn;using namespace sgn::retail;
namespace {
unsigned checks{},boundaries{};
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);++checks;}
struct Stop{};
using Kind=WorldPacketWriteKind;
struct Expected {unsigned pc;Kind kind;bool alpha;};
// Independent transcription of the pinned ten-function source evidence.
constexpr unsigned field_pc[4][2][5]={
    {{0x429cc8,0x429cd0,0x429cd6,0x429cd9,0x429cdc},{0x429cf0,0x429cf8,0x429cfe,0x429d01,0x429d04}},
    {{0x429e43,0x429e3a,0x429e45,0x429e48,0},{0x429e5c,0x429e53,0x429e5e,0x429e61,0}},
    {{0x42a025,0x42a02a,0x42a033,0x42a039,0x42a03c},{0x42a000,0x42a005,0x42a00e,0x42a014,0x42a017}},
    {{0x42a1b1,0x42a1b3,0x42a1b6,0x42a1b9,0},{0x42a18b,0x42a193,0x42a196,0x42a199,0}}};
constexpr unsigned tail_pc[4][2]={{0x429d2e,0x429d30},{0x429e7a,0x429e7c},{0x42a083,0x42a080},{0x42a1ff,0x42a201}};
constexpr unsigned empty_pc[4][2]={{0x429d3a,0x429d3c},{0x429e88,0x429e8a},{0x42a083,0x42a080},{0x42a20d,0x42a212}};
std::vector<Expected> sequence(unsigned path,bool empty){
    std::vector<Expected> result;
    if(!empty)for(unsigned alpha=0;alpha<2;++alpha){
        auto fields=std::array{Kind::header,Kind::indices,Kind::vertices,Kind::attributes,Kind::inside_mask};
        if(path==1)std::swap(fields[0],fields[1]);
        for(const auto kind:fields){const auto pc=field_pc[path][alpha][static_cast<unsigned>(kind)];if(pc)result.push_back({pc,kind,alpha!=0});}
    }
    const auto* tail=empty?empty_pc[path]:tail_pc[path];
    for(unsigned i=0;i<2;++i){const auto alpha=path==2?i==0:i!=0;result.push_back({tail[alpha?1:0],alpha?Kind::alpha_current:Kind::opaque_current,alpha});}
    return result;
}
struct Fixture {
    std::array<WorldPolygonHeader,2> headers{};
    std::array<WorldPolygonHeader,1> previous_headers{};
    std::array<VehicleStripCorner,2> attributes{},previous_attributes{};
    std::array<VehicleVector,2> authored{};std::array<VehicleVector,1> normals{};
    std::array<std::uint16_t,2> indices{0,1},previous_indices{1,0};
    std::array<WorldPolygonVertex,2> vertices{},previous_vertices{};
    std::array<std::array<WorldPolygonPacket,10>,2> payload{};
    WorldPolygonPacketStorageLifetime opaque_lifetime{payload[0]},alpha_lifetime{payload[1]};
    WorldPolygonProducerList opaque{opaque_lifetime.pointer(2),opaque_lifetime.pointer(5)};
    WorldPolygonProducerList alpha{alpha_lifetime.pointer(3),alpha_lifetime.pointer(7)};
    WorldPolygonPartSource source;
    std::array<std::uint16_t,2> first_batch{0,0xffff},second_batch{1,0xffff};
    std::array<WorldSpatialBatch,2> batches{{{first_batch,{}},{second_batch,{}}}};
    unsigned batch_count=19;
    Fixture(){
        for(unsigned i=0;i<2;++i){headers[i][0]=1;headers[i][3]=static_cast<std::uint8_t>(0x80u+i*0x40u);headers[i][6]=static_cast<std::uint8_t>(i);attributes[i].vertex=static_cast<std::uint16_t>(i);vertices[i].words[4]=63;}
        source={WorldPolygonHeaderView(std::span(headers)),WorldPolygonAttributeView(std::span(attributes)),authored,normals,indices};
        for(auto& pool:payload)for(auto& packet:pool)packet={world_polygon_header_ref(previous_headers[0]),previous_indices,WorldPolygonAttributeView(std::span(previous_attributes)),previous_vertices,0xabcddcba};
        // Emission must use retained current after independent bases become NULL.
        opaque.clear_base();alpha.clear_base();
    }
    void run(unsigned path,bool empty,WorldPacketEmissionObserver* observer){
        auto part=source;if(empty)part.headers={};
        const auto selected=empty?std::span<const WorldSpatialBatch>{}:std::span<const WorldSpatialBatch>(batches);
        if(path==0)emit_world_polygons_clipped(part,vertices,{},opaque,alpha,observer);
        else if(path==1)emit_world_polygons_unclipped(part,vertices,{},opaque,alpha,observer);
        else if(path==2)emit_world_spatial_polygons_clipped(selected,part,vertices,{},batch_count,opaque,alpha,observer);
        else emit_world_spatial_polygons_inside(selected,part,vertices,{},batch_count,opaque,alpha,observer);
    }
};
struct Observer final:WorldPacketEmissionObserver {
    Fixture& f;unsigned path,stop,seen{};bool empty;
    std::vector<Expected> expected;
    bool fields[2][5]{};bool published[2]{};
    Observer(Fixture& fixture,unsigned selected,unsigned stop_after,bool is_empty):f(fixture),path(selected),stop(stop_after),empty(is_empty),expected(sequence(selected,is_empty)){}
    void verify(){
        check(f.opaque.base().is_null()&&f.alpha.base().is_null(),"emitter republished base");
        check(f.opaque.current().offset()==5+(published[0]&&!empty?1u:0u)&&f.alpha.current().offset()==7+(published[1]&&!empty?1u:0u),"local packet append leaked to global current");
        check(f.batch_count==(path>=2&&!empty?21u:19u),"spatial batch count/order differs");
        for(unsigned p=0;p<2;++p)for(unsigned i=0;i<10;++i){const auto& record=f.payload[p][i];const auto edited=i==(p?7u:5u);
            const auto written=[&](Kind k){return edited&&fields[p][static_cast<unsigned>(k)];};
            check(record.header==(written(Kind::header)?world_polygon_header_ref(f.headers[p]):world_polygon_header_ref(f.previous_headers[0])),"partial packet header differs");
            check(record.indices.data()==(written(Kind::indices)?f.indices.data()+p:f.previous_indices.data())&&record.indices.size()==(written(Kind::indices)?2-p:2),"partial packet indices differ");
            check(record.vertices.data()==(written(Kind::vertices)?f.vertices.data():f.previous_vertices.data())&&record.vertices.size()==2,"partial packet vertices differ");
            const auto attributes=record.attributes.bytes();
            check(attributes.data()==(written(Kind::attributes)?std::as_bytes(std::span(f.attributes)).data():std::as_bytes(std::span(f.previous_attributes)).data())&&attributes.size()==sizeof(f.attributes),"partial packet attributes differ");
            check(record.inside_mask==(written(Kind::inside_mask)?63u:0xabcddcbau),"partial packet retained mask differs");
        }
    }
    void stored(const WorldPacketWriteEvent& e,const WorldPolygonProducerList& opaque,const WorldPolygonProducerList& alpha) override {
        check(seen<expected.size()&&&opaque==&f.opaque&&&alpha==&f.alpha,"observer list identity/count");const auto& wanted=expected[seen++];
        check(e.instruction==wanted.pc&&e.kind==wanted.kind&&e.alpha==wanted.alpha,"producer source field order/PC differs");
        const auto p=e.alpha?1u:0u;
        if(e.kind==Kind::opaque_current||e.kind==Kind::alpha_current)published[p]=true;else fields[p][static_cast<unsigned>(e.kind)]=true;
        check(e.pointer.same_storage(p?f.alpha_lifetime.pointer():f.opaque_lifetime.pointer())&&e.pointer.offset()==(p?7u:5u)+(published[p]&&!empty?1u:0u),"observer pointer lost source register position");
        verify();if(seen==stop)throw Stop{};
    }
};
void interruptions(){
    for(unsigned path=0;path<4;++path)for(const bool empty:{false,true}){
        const auto count=static_cast<unsigned>(sequence(path,empty).size());
        for(unsigned stop=0;stop<=count;++stop){Fixture f;Observer observer(f,path,stop,empty);bool stopped=false;
            try{f.run(path,empty,&observer);}catch(const Stop&){stopped=true;}
            check(stopped==(stop!=0)&&observer.seen==(stop?stop:count),"emission interruption missing");observer.verify();if(stop)++boundaries;
        }
    }
}
void zero_and_forced(){
    for(unsigned path=0;path<4;++path){
        Fixture zero;zero.headers[0][0]=zero.headers[1][0]=0;zero.run(path,false,nullptr);
        const auto emits=path%2!=0;check(zero.opaque.current().offset()==(emits?6u:5u)&&zero.alpha.current().offset()==(emits?8u:7u),"zero-corner source branch changed");
        Fixture forced;forced.source.model_flags_02=8;forced.source.part_flags_02=0x180;forced.run(path,false,nullptr);
        check(forced.opaque.current().offset()==(path<2?5u:6u)&&forced.alpha.current().offset()==(path<2?9u:8u),"spatial incorrectly inherited part forced translucency");
    }
    Fixture failure;failure.opaque.publish_current(failure.opaque_lifetime.pointer(10));
    bool rejected=false;try{failure.run(0,false,nullptr);}catch(const FormatError&){rejected=true;}
    check(rejected&&failure.alpha.current().offset()==7&&failure.opaque.current().offset()==10,"capacity failure published a cursor");
    Fixture threaded;Observer untouched(threaded,0,0,false);std::atomic<unsigned> rejections{};
    std::thread foreign([&]{try{threaded.run(0,false,&untouched);}catch(const FormatError&){++rejections;}});foreign.join();
    check(rejections==1&&untouched.seen==0,"foreign-thread emitter reached packet fields");untouched.verify();
}
}
int main()try{interruptions();zero_and_forced();std::cout<<"{\"passed\":true,\"checks\":"<<checks<<",\"interruptions\":"<<boundaries<<",\"producer_paths\":4,\"source_execution\":false}\n";return 0;}
catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
