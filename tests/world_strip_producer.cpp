#include "sgn/retail_world_strip_producer.hpp"
#include <fstream>
#include <iostream>
#include <memory>

using namespace sgn;using namespace sgn::retail;
namespace {
void require(bool value,const std::string& label){if(!value)throw FormatError(label);}
struct Reader {
    std::ifstream file;std::uint64_t bytes{},unknown{};
    explicit Reader(const char* path):file(path,std::ios::binary){require(bool(file),"missing strip corpus");}
    void read(void* p,std::size_t n){require(n<8*1024*1024,"oversized strip record");file.read(static_cast<char*>(p),static_cast<std::streamsize>(n));require(bool(file),"truncated strip corpus");}
    template<class T>T value(){T result{};read(&result,sizeof(result));return result;}
    std::uint32_t word(){return value<std::uint32_t>();}
    void equal(const void* p,std::size_t n,const std::string& label){std::vector<std::byte> expected(n);read(expected.data(),n);const auto* b=static_cast<const std::byte*>(p);
        for(std::size_t i=0;i<n;++i)if(b[i]!=expected[i])throw FormatError(label+" byte "+std::to_string(i));bytes+=n;}
};
void geometry(Reader& in,VehicleStripGeometry& g,bool compare,const std::string& label){
    const auto field=[&](auto& value,const char* suffix){if(compare)in.equal(value.data(),sizeof(value),label+suffix);else in.read(value.data(),sizeof(value));};
    field(g.polygons," headers");field(g.corner_indices," indices");field(g.vertices," vertices");field(g.corners," attributes");field(g.normals," normals");
}
template<class T>std::uint32_t token(std::span<T> span,const void* p){for(std::size_t i=0;i<=span.size();++i)if(span.data()+i==p)return static_cast<std::uint32_t>(i+1);throw FormatError("strip packet lost indexed owner");}
void contracts(){
    auto g=std::make_unique<VehicleStripGeometry>();ModelTransform outer{};outer.tail[1]=outer.tail[2]=1;CameraState camera;camera.world_to_camera_reference=1;
    ViewMatrixArena arena;WorldModelViewScratch scratch{};std::array<WorldPolygonVertex,4> vertices{};WorldTransformedVertexCursor cursor{vertices,0};
    std::array<WorldPolygonPacket,2> records{};WorldPolygonPacketStorageLifetime opaque_lifetime(records);WorldPolygonProducerList opaque{opaque_lifetime.pointer(),opaque_lifetime.pointer()},alpha;unsigned rejected{};
    for(unsigned test=0;test<5;++test){g->part_flags=1;g->outer_transform=&outer;g->polygon_count=0;g->vertex_count=0;g->corner_count=65535;cursor.count=0;outer.tail[1]=1;
        if(test==0)g->outer_transform=nullptr;if(test==1)g->polygon_count=129;if(test==2)g->vertex_count=513;if(test==3)cursor.count=5;if(test==4)outer.tail[1]=0;
        try{produce_world_strip_unlit(*g,camera,arena,vehicle_identity(),{},test%2!=0,scratch,cursor,opaque,alpha);}catch(const FormatError&){++rejected;}
        require(scratch.local_camera.x==0&&scratch.model_view==CameraMatrix{}&&opaque.count()==0&&alpha.count()==0,"invalid strip mutated output");}
    require(rejected==5,"invalid strip binding accepted");g->part_flags=0;g->outer_transform=nullptr;g->polygon_count=g->vertex_count=65535;
    produce_world_strip_unlit(*g,camera,arena,vehicle_identity(),{},true,scratch,cursor,opaque,alpha);
    require(opaque.count()==0&&alpha.count()==0,"hidden strip appended output");std::cout<<"strip contracts passed:5 atomic rejects,hidden part bypass\n";
}
}
int main(int argc,char** argv)try{
    if(argc==2&&std::string_view(argv[1])=="--contracts"){contracts();return 0;}
    require(argc==2,"expected strip corpus");Reader in(argv[1]);const std::array<char,8> magic{'S','G','N','S','T','P','0','1'};in.equal(magic.data(),magic.size(),"strip schema");require(in.word()==12,"strip groups");
    std::uint64_t calls{},packets{};
    for(unsigned group=0;group<12;++group){
        auto g=std::make_unique<VehicleStripGeometry>();geometry(in,*g,false,"");g->polygon_count=in.value<std::uint16_t>();g->corner_count=in.value<std::uint16_t>();g->vertex_count=in.value<std::uint16_t>();
        g->part_transform=in.value<VehicleStripTransform>();auto outer=in.value<ModelTransform>();g->outer_transform=&outer;
        const auto position=in.value<Vec3>();CameraState camera;camera.position={position.x,position.y,position.z,0};camera.world_to_camera_reference=2;
        const auto projection=in.value<ProjectionParameters>();auto scratch=in.value<WorldModelViewScratch>();ViewMatrixArena arena;in.read(arena.matrices.data(),sizeof(arena.matrices));arena.cursor=in.word();arena.count=in.word();
        const auto matrix_cursor=*arena.cursor,matrix_count=arena.count;const auto matrix_before=arena.matrices;
        const auto total=in.word();require(total<=2048,"strip vertex capacity");std::vector<WorldPolygonVertex> vertices(total);in.read(vertices.data(),vertices.size()*sizeof(WorldPolygonVertex));
        const auto begin=in.word(),capacity=in.word(),start=in.word();require(begin<total&&start<capacity&&capacity<=512,"strip output bounds");
        std::array<std::vector<WorldPolygonPacket>,2> pools{std::vector<WorldPolygonPacket>(capacity),std::vector<WorldPolygonPacket>(capacity)};
        for(auto& pool:pools)for(auto& p:pool)p={world_polygon_header_ref(g->polygons[0]),g->corner_indices,WorldPolygonAttributeView(std::span(g->corners)),std::span(vertices).subspan(begin),in.word()};
        WorldTransformedVertexCursor cursor{vertices,begin};WorldPolygonPacketStorageLifetime opaque_lifetime(pools[0]),alpha_lifetime(pools[1]);WorldPolygonProducerList opaque{opaque_lifetime.pointer(),opaque_lifetime.pointer(start)},alpha{alpha_lifetime.pointer(),alpha_lifetime.pointer(start)};require(in.word()==4,"strip retained steps");bool unknown=false;
        for(unsigned step=0;step<4;++step,++calls){
            const auto label="strip group "+std::to_string(group)+" step "+std::to_string(step);g->part_flags=in.value<std::uint16_t>();g->model_flags_02=in.value<std::uint16_t>();g->part_flags_02=in.value<std::uint16_t>();const auto clipped=in.word();require(clipped<=1,"strip clipping route");
            unknown|=g->part_flags==1;const auto before=opaque.count()+alpha.count();
            produce_world_strip_unlit(*g,camera,arena,vehicle_identity(),projection,clipped!=0,scratch,cursor,opaque,alpha);
            in.equal(&cursor.count,4,label+" cursor");const auto opaque_count=opaque.count(),alpha_count=alpha.count();in.equal(&opaque_count,4,label+" opaque cursor");in.equal(&alpha_count,4,label+" alpha cursor");
            in.equal(vertices.data(),vertices.size()*sizeof(WorldPolygonVertex),label+" transformed");const auto expected=in.value<std::array<std::byte,96>>();require(in.word()==unsigned(unknown),label+" W provenance");
            const auto actual=std::as_bytes(std::span(&scratch,1));static_assert(sizeof(WorldModelViewScratch)==96);
            for(unsigned i=0;i<96;++i)if(unknown&&((i>=12&&i<16)||(i>=28&&i<32)))++in.unknown;else{require(actual[i]==expected[i],label+" scratch byte "+std::to_string(i));++in.bytes;}
            const WorldPolygonHeaderView headers{std::span(g->polygons)};
            for(const auto& pool:pools)for(const auto& p:pool){require(p.attributes.bytes().data()==std::as_bytes(std::span(g->corners)).data(),label+" attribute borrow");
                const std::array<std::uint32_t,5> record{static_cast<std::uint32_t>(headers.index_of(p.header)+1),token(std::span<const std::uint16_t>(g->corner_indices),p.indices.data()),1,token(std::span(vertices),p.vertices.data()),p.inside_mask};in.equal(record.data(),sizeof(record),label+" packet");}
            geometry(in,*g,true,label);in.equal(&g->part_transform,sizeof(g->part_transform),label+" part pose");in.equal(&outer,sizeof(outer),label+" outer pose");
            require(g->outer_transform==&outer&&arena.cursor==matrix_cursor&&arena.count==matrix_count&&std::memcmp(arena.matrices.data(),matrix_before.data(),sizeof(matrix_before))==0,label+" retained owner/matrix");
            packets+=opaque.count()+alpha.count()-before;
        }
    }
    require(in.file.peek()==std::char_traits<char>::eof(),"strip trailing corpus");
    std::cout<<"{\"passed\":true,\"calls\":"<<calls<<",\"retained_calls\":"<<calls-12<<",\"packets\":"<<packets<<",\"defined_bytes\":"<<in.bytes<<",\"undefined_w_bytes\":"<<in.unknown<<"}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
