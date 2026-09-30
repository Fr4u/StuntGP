#include "sgn/retail_world_spatial_producer.hpp"
#include <fstream>
#include <iostream>

namespace {
using namespace sgn;using namespace sgn::retail;
void require(bool okay,const char* message){if(!okay)throw FormatError(message);}
class Reader {
    std::ifstream file_;
public:
    std::uint64_t compared{};
    explicit Reader(const char* path):file_(path,std::ios::binary){require(bool(file_),"missing spatial original corpus");}
    void bytes(void* data,std::size_t size){require(size<32*1024*1024,"oversized spatial record");file_.read(static_cast<char*>(data),static_cast<std::streamsize>(size));require(bool(file_),"truncated spatial original corpus");}
    template<class T>T value(){T v;bytes(&v,sizeof(v));return v;}
    std::uint32_t word(){return value<std::uint32_t>();}
    template<class T>std::vector<T> array(std::size_t n){require(n<1024*1024,"oversized spatial array");std::vector<T> v(n);bytes(v.data(),v.size()*sizeof(T));return v;}
    template<class T>std::vector<T> array(){return array<T>(word());}
    void equal(const void* data,std::size_t size,const char* message){const auto expected=array<std::byte>(size);require(size==0||std::memcmp(data,expected.data(),size)==0,message);compared+=size;}
    void end(){require(file_.peek()==std::char_traits<char>::eof(),"trailing spatial original corpus");}
};
struct Fixture{std::vector<VehicleVector> vertices;ModelSpatialData spatial;};
}
int main(int argc,char** argv)try{
    require(argc==2,"expected spatial original corpus");Reader in(argv[1]);const std::array<char,8> magic{'S','G','N','S','P','0','0','1'};in.equal(magic.data(),8,"wrong spatial corpus");
    require(in.word()==30,"expected30 track models");std::array<Fixture,30> models;
    for(auto& fixture:models){fixture.vertices=in.array<VehicleVector>();auto& spatial=fixture.spatial;spatial.bounds=in.array<VehicleVector>();spatial.vertex_indices=in.array<std::uint16_t>();spatial.polygon_indices=in.array<std::uint16_t>();spatial.nodes=in.array<std::array<std::uint16_t,32>>();}
    const auto classifiers=in.word();require(classifiers>=512,"incomplete classifier corpus");
    for(unsigned n=0;n<classifiers;++n){const auto bounds=in.value<std::array<VehicleVector,8>>();const auto camera=in.value<Vec3>();const auto planes=in.value<std::array<CameraVector,4>>();
        require(classify_world_spatial_bounds(bounds,camera,planes)==in.word(),"spatial classifier differs from original");in.compared+=4;}
    const auto gathers=in.word();require(gathers>=180,"incomplete spatial traversal corpus");
    for(unsigned n=0;n<gathers;++n){const auto id=in.word();require(id<models.size(),"invalid traversal model");const auto& spatial=models[id].spatial;
        const auto camera=in.value<Vec3>();const auto planes=in.value<std::array<CameraVector,4>>();const auto capacity=in.word(),start=in.word();require(start<=capacity&&capacity<32768,"spatial batch capacity");
        std::array<std::vector<WorldSpatialBatch>,2> storage{std::vector<WorldSpatialBatch>(capacity),std::vector<WorldSpatialBatch>(capacity)};
        for(auto& list:storage)for(auto& batch:list)batch={spatial.polygon_indices,spatial.vertex_indices};
        WorldSpatialBatchList clipped{storage[0],start},inside{storage[1],start};gather_world_spatial_batches(spatial,0,camera,planes,clipped,inside);
        require(in.word()==clipped.count&&in.word()==inside.count,"spatial traversal cursors differ");
        for(const auto& list:storage)for(const auto& batch:list){const std::array<std::uint32_t,2> offsets{
            static_cast<std::uint32_t>(batch.polygons.data()-spatial.polygon_indices.data()),static_cast<std::uint32_t>(batch.vertices.data()-spatial.vertex_indices.data())};
            in.equal(offsets.data(),sizeof(offsets),"spatial traversal order/retained slots differ");}
    }
    const auto transforms=in.word();require(transforms>=90,"incomplete indexed vertex corpus");std::uint64_t phases=0;
    for(unsigned n=0;n<transforms;++n){const auto id=in.word();require(id<models.size(),"invalid indexed transform model");const auto& fixture=models[id];
        auto vertices=in.array<WorldPolygonVertex>();require(vertices.size()==fixture.vertices.size()+2,"indexed vertex guards missing");
        const auto capacity=in.word(),start=in.word();require(start<=capacity&&capacity<128*1024,"indexed pointer capacity");
        std::vector<WorldPolygonVertex*> pointers(capacity,&vertices[0]);WorldIndexedVertexStorageLifetime lifetime(pointers);WorldIndexedVertexCursor cursor;cursor.publish_base(lifetime.pointer());cursor.publish_current(lifetime.pointer(start));const auto phase_count=in.word();require(phase_count==2,"expected own-state paired transforms");
        for(unsigned phase=0;phase<phase_count;++phase){++phases;const auto clipped=in.word(),frame=in.word();const auto offsets=in.array<std::uint32_t>();
            std::vector<WorldSpatialBatch> batches;for(const auto offset:offsets){require(offset<fixture.spatial.vertex_indices.size(),"indexed source list offset");batches.push_back({{},std::span(fixture.spatial.vertex_indices).subspan(offset)});}
            const auto matrix=in.value<CameraMatrix>();const auto projection=in.value<ProjectionParameters>();
            if(clipped)transform_world_spatial_vertices_clipped(batches,fixture.vertices,vertices,cursor,frame,matrix,projection);
            else transform_world_spatial_vertices_inside(batches,fixture.vertices,vertices,cursor,frame,matrix,projection);
            require(in.word()==cursor.count(),"indexed pointer cursor differs from original");
            const auto label="indexed transform case "+std::to_string(n)+" phase "+std::to_string(phase);
            in.equal(vertices.data(),vertices.size()*sizeof(vertices.front()),label.c_str());
            for(const auto* pointer:pointers){const auto index=static_cast<std::uint32_t>(pointer-vertices.data());require(in.word()==index,"indexed pointer order/retained slots differ");in.compared+=4;}
        }
    }
    in.end();
    // New host guards for malformed spatial storage are separate from source
    // semantic comparison. Invalid roots must not access a missing node.
    ModelSpatialData bad;bad.bounds.resize(8);bad.vertex_indices={0xffff};bad.polygon_indices={0xffff};bad.nodes.resize(1);bad.nodes[0].fill(0xffff);
    bad.nodes[0][0]=1;bad.nodes[0][1]=0;bad.nodes[0][2]=0;bad.nodes[0][3]=0;
    std::array<WorldSpatialBatch,8> storage;WorldSpatialBatchList list{storage,0};bool rejected=false;
    try{gather_world_spatial_batches(bad,2,{},std::array<CameraVector,4>{},list,list);}catch(const FormatError&){rejected=true;}
    require(rejected,"invalid spatial root accepted");
    std::cout<<"{\"passed\":true,\"models\":30,\"classifiers\":"<<classifiers<<",\"traversals\":"<<gathers<<",\"indexed_cases\":"<<transforms<<",\"indexed_calls\":"<<phases<<",\"compared_bytes\":"<<in.compared<<"}\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
