#include "sgn/retail_world_sprite_producer.hpp"
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>

using namespace sgn;using namespace sgn::retail;
namespace {
void require(bool value,const char* text){if(!value)throw FormatError(text);}
struct Reader{
    std::ifstream file;std::uint64_t bytes{},unknown{};
    explicit Reader(const char* path):file(path,std::ios::binary){}
    void read(void* p,std::size_t n){require(n<8*1024*1024,"oversized sprite record");file.read(static_cast<char*>(p),static_cast<std::streamsize>(n));require(bool(file),"truncated sprite corpus");}
    template<class T>T value(){T v{};read(&v,sizeof(v));return v;}
    std::uint32_t word(){return value<std::uint32_t>();}
    void equal(const void* p,std::size_t n){std::vector<std::byte> expected(n);read(expected.data(),n);require(n==0||std::memcmp(p,expected.data(),n)==0,"sprite comparison mismatch");bytes+=n;}
};
std::array<std::uint32_t,5> words(const WorldPolygonPacket& p,const SourceSpritePacketStorage& s,const std::vector<WorldPolygonVertex>& v){
    require(p.attributes.bytes().data()==std::as_bytes(std::span(s.attributes)).data(),"sprite packet lost attribute owner");
    require(WorldPolygonHeaderView(std::span(s.headers)).find(p.header).has_value(),"sprite packet lost header owner");
    return {static_cast<std::uint32_t>(WorldPolygonHeaderView(std::span(s.headers)).index_of(p.header))+1,static_cast<std::uint32_t>(p.indices.data()-s.indices.data())+1,1,
        static_cast<std::uint32_t>(p.vertices.data()-v.data())+1,p.inside_mask};
}
void contracts(){
    auto state=std::make_unique<WorldModelDrawState>();auto s=std::make_unique<SourceSpritePacketStorage>();
    ModelTransform outer{};outer.tail[1]=outer.tail[2]=1;CameraState camera;camera.world_to_camera_reference=1;
    ViewMatrixArena arena;WorldModelViewScratch scratch{};scratch.local_camera={1,2,3,4};scratch.normalized_camera={5,6,7,8};
    std::array<WorldPolygonVertex,4> vertices{};vertices[0].words.fill(0x98173471u);
    std::array<WorldPolygonPacket,2> records{};WorldTransformedVertexCursor cursor{vertices,0};
    WorldPolygonPacketStorageLifetime opaque_lifetime(records);WorldPolygonProducerList opaque{opaque_lifetime.pointer(),opaque_lifetime.pointer()},translucent;const auto before=scratch;const auto vertex_before=vertices;
    unsigned rejected{};
    for(unsigned mode=0;mode<5;++mode){
        state->sprite_part_flags=1;state->sprite_outer_transform=&outer;s->polygons=0;s->corners=0;cursor.count=0;outer.tail[1]=1;
        if(mode==0)state->sprite_outer_transform=nullptr;
        if(mode==1)s->polygons=513;
        if(mode==2)s->corners=2049;
        if(mode==3)cursor.count=5;
        if(mode==4)outer.tail[1]=0;
        try{produce_world_sprite_unlit(*state,*s,camera,arena,vehicle_identity(),{},true,scratch,cursor,opaque,translucent);}
        catch(const FormatError&){++rejected;}
        require(std::memcmp(&scratch,&before,sizeof(before))==0&&std::memcmp(vertices.data(),vertex_before.data(),sizeof(vertices))==0,
            "invalid sprite binding mutated output");
        require(opaque.count()==0&&translucent.count()==0,"invalid sprite binding appended packets");
    }
    require(rejected==5,"invalid sprite binding accepted");
    state->sprite_part_flags=0;state->sprite_outer_transform=nullptr;s->polygons=s->corners=65535;
    produce_world_sprite_unlit(*state,*s,camera,arena,vehicle_identity(),{},true,scratch,cursor,opaque,translucent);
    require(std::memcmp(&scratch,&before,sizeof(before))==0,"hidden sprite touched scratch");
    std::cout<<"sprite_binding_guards=5 hidden_part_noop=1 passed=1\n";
}
}
int main(int argc,char** argv)try{
    if(argc==2&&std::string_view(argv[1])=="--contracts"){contracts();return 0;}
    require(argc==2,"expected sprite corpus");Reader in(argv[1]);const std::array<char,8> magic{'S','G','N','S','P','R','0','1'};in.equal(magic.data(),8);
    const auto groups=in.word();require(groups>0&&groups<1000,"sprite group count");std::uint64_t calls{},packets{};
    for(unsigned group=0;group<groups;++group){
        auto state=std::make_unique<WorldModelDrawState>();auto s=std::make_unique<SourceSpritePacketStorage>();
        in.read(s->headers.data(),sizeof(s->headers));in.read(s->indices.data(),sizeof(s->indices));in.read(s->positions.data(),sizeof(s->positions));in.read(s->attributes.data(),sizeof(s->attributes));
        s->polygons=in.value<std::uint16_t>();s->vertices=in.value<std::uint16_t>();s->corners=in.value<std::uint16_t>();
        //These append cursors are deliberately unrelated.42A260 never reads them.
        s->header_bytes=4096;s->index_count=17;s->position_count=19;s->attribute_count=23;
        state->sprite_part_transform=in.value<ModelTransform>();auto outer=in.value<ModelTransform>();state->sprite_outer_transform=&outer;
        CameraState camera;const auto position=in.value<Vec3>();camera.position={position.x,position.y,position.z,0};camera.world_to_camera_reference=2;
        const auto projection=in.value<ProjectionParameters>();auto scratch=in.value<WorldModelViewScratch>();
        ViewMatrixArena arena;in.read(arena.matrices.data(),sizeof(arena.matrices));arena.cursor=192;arena.count=3;
        const auto total=in.word();require(total>=4&&total<=16384,"sprite vertex allocation");std::vector<WorldPolygonVertex> vertices(total);in.read(vertices.data(),total*sizeof(vertices.front()));
        const auto begin=in.word(),capacity=in.word(),start=in.word();require(begin<total&&capacity<=8192&&start<capacity,"sprite output bounds");
        std::array<std::vector<WorldPolygonPacket>,2> pools{std::vector<WorldPolygonPacket>(capacity),std::vector<WorldPolygonPacket>(capacity)};
        for(auto& pool:pools)for(auto& p:pool)p={world_polygon_header_ref(s->headers[0]),s->indices,WorldPolygonAttributeView(std::span(s->attributes)),std::span(vertices).subspan(begin),in.word()};
        WorldTransformedVertexCursor cursor{vertices,begin};WorldPolygonPacketStorageLifetime opaque_lifetime(pools[0]),alpha_lifetime(pools[1]);WorldPolygonProducerList opaque{opaque_lifetime.pointer(),opaque_lifetime.pointer(start)},translucent{alpha_lifetime.pointer(),alpha_lifetime.pointer(start)};
        const auto geometry_before=*s;const auto matrices_before=arena.matrices;const auto outer_before=outer;const auto part_before=state->sprite_part_transform;
        const auto steps=in.word();require(steps==4,"sprite retained steps");bool expected_unknown=false;
        for(unsigned step=0;step<steps;++step){
            state->sprite_part_flags=in.value<std::uint16_t>();state->sprite_model_flags=in.value<std::uint16_t>();state->sprite_part_flags_02=in.value<std::uint16_t>();const bool clipped=in.word()!=0;
            const auto old_count=opaque.count()+translucent.count();
            if(state->sprite_part_flags==1)expected_unknown=true;
            produce_world_sprite_unlit(*state,*s,camera,arena,vehicle_identity(),projection,clipped,scratch,cursor,opaque,translucent);
            require(in.word()==cursor.count&&in.word()==opaque.count()&&in.word()==translucent.count(),"sprite count comparison mismatch");
            in.equal(vertices.data(),vertices.size()*sizeof(vertices.front()));
            const auto expected=in.value<std::array<std::byte,96>>();const auto mask=in.value<std::array<std::uint8_t,96>>();
            const auto* actual=reinterpret_cast<const std::byte*>(&scratch);
            //Once assigned from the one-part source stack lane, W retains
            //that provenance through skipped subsequent calls as well.
            for(unsigned k=0;k<96;++k){const bool allowed=(k>=12&&k<16)||(k>=28&&k<32);require(mask[k]==unsigned(expected_unknown&&allowed),"sprite W mask provenance");
                if(mask[k])++in.unknown;else{require(actual[k]==expected[k],"sprite scratch comparison mismatch");++in.bytes;}}
            for(const auto& pool:pools)for(const auto& p:pool){const auto record=words(p,*s,vertices);in.equal(record.data(),sizeof(record));}
            require(std::memcmp(s.get(),&geometry_before,sizeof(*s))==0,"sprite producer mutated geometry");
            require(std::memcmp(&outer,&outer_before,sizeof(outer))==0&&std::memcmp(&state->sprite_part_transform,&part_before,sizeof(part_before))==0,"sprite producer mutated poses");
            require(std::memcmp(arena.matrices.data(),matrices_before.data(),sizeof(matrices_before))==0&&arena.cursor==192u&&arena.count==3,"sprite producer mutated matrix arena");
            packets+=opaque.count()+translucent.count()-old_count;++calls;
        }
    }
    require(in.file.peek()==std::char_traits<char>::eof(),"sprite trailing corpus");
    std::cout<<"{\"passed\":true,\"calls\":"<<calls<<",\"retained_calls\":"<<calls-groups<<",\"packets\":"<<packets<<",\"defined_bytes\":"<<in.bytes<<",\"undefined_w_bytes\":"<<in.unknown<<"}\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
