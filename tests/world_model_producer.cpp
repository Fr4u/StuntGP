#include "sgn/retail_world_model_producer.hpp"
#include <fstream>
#include <iostream>
#include <map>

namespace {
using namespace sgn;using namespace sgn::retail;
void require(bool okay,const char* message){if(!okay)throw FormatError(message);}
class Reader {
    std::ifstream file_;
public:
    std::uint64_t compared{},unspecified{};
    explicit Reader(const char* path):file_(path,std::ios::binary){require(bool(file_),"missing full-model corpus");}
    void bytes(void* data,std::size_t count){require(count<32*1024*1024,"oversized full-model corpus record");file_.read(static_cast<char*>(data),static_cast<std::streamsize>(count));require(bool(file_),"truncated full-model corpus");}
    template<class T>T value(){T v;bytes(&v,sizeof(v));return v;}
    std::uint32_t word(){return value<std::uint32_t>();}
    template<class T>std::vector<T> array(std::size_t n){require(n<1024*1024,"oversized model corpus array");std::vector<T> v(n);bytes(v.data(),v.size()*sizeof(T));return v;}
    template<class T>std::vector<T> array(){return array<T>(word());}
    std::string name(){const auto n=word();require(n&&n<128,"invalid model corpus name");std::string s(n,'\0');bytes(s.data(),n);return s;}
    void equal(const void* data,std::size_t count,const char* message){const auto expected=array<std::byte>(count);require(count==0||std::memcmp(data,expected.data(),count)==0,message);compared+=count;}
    void end(){require(file_.peek()==std::char_traits<char>::eof(),"trailing full-model corpus");}
};
}
int main(int argc,char** argv)try{
    require(argc==3,"expected retail root and full-model corpus");Reader in(argv[2]);const std::array<char,8> magic{'S','G','N','M','P','0','0','1'};in.equal(magic.data(),8,"wrong full-model corpus");
    const auto cases=in.word();require(cases==624,"missing original model/LOD/source slot cases");
    AssetStore assets(argv[1]);ModelResources models(assets,argv[1]);std::map<std::string,std::size_t> loaded;std::uint64_t emitted=0;
    for(unsigned n=0;n<cases;++n){
        const auto name=in.name();auto found=loaded.find(name);if(found==loaded.end()){
            const auto set=name.starts_with("track")&&name.ends_with("m")?"trackset00":"";found=loaded.emplace(name,*models.resolve(models.load(name,set))).first;
        }
        auto& model=models.at(found->second);const auto slot=in.word(),clipped=in.word(),lod=model.source_lod_index(slot);
        const auto flags=in.array<std::uint16_t>();require(flags.size()==model.part_count(lod),"model part flags count");
        for(unsigned p=0;p<flags.size();++p)model.part(lod,p).flags=flags[p];
        model.source_model_flags_02()=in.value<std::uint16_t>();const auto part_flags=in.array<std::uint16_t>();require(part_flags.size()==flags.size(),"model part alpha flags count");
        for(unsigned p=0;p<part_flags.size();++p)model.source_part_flags_02(lod,p)=part_flags[p];
        auto outer=in.value<ModelTransform>();const auto camera_position=in.value<Vec3>();const auto projection=in.value<ProjectionParameters>();
        WorldModelViewScratch scratch=in.value<WorldModelViewScratch>();
        ViewMatrixArena arena;in.bytes(arena.matrices.data(),sizeof(arena.matrices));arena.cursor=64;arena.count=1;
        const auto identity=vehicle_identity();CameraState camera;camera.position={camera_position.x,camera_position.y,camera_position.z,0};camera.world_to_camera_reference=2;
        allocate_outer_model_matrices(outer,arena);allocate_model_part_matrices(model,slot,arena);
        auto vertices=in.array<WorldPolygonVertex>();const auto begin=in.word(),capacity=in.word(),start=in.word();require(begin<vertices.size()&&capacity<32768,"full-model capacity");
        const auto first=world_polygon_part_source(model,lod,0);
        std::array<std::vector<WorldPolygonPacket>,2> pools{std::vector<WorldPolygonPacket>(capacity),std::vector<WorldPolygonPacket>(capacity)};
        for(auto& pool:pools)for(auto& packet:pool)packet={WorldPolygonHeaderView(model.source_attribute_table()).at(0),model.source_corner_indices(),first.attributes,std::span(vertices).subspan(begin),in.word()};
        WorldTransformedVertexCursor cursor{vertices,begin};WorldPolygonPacketStorageLifetime opaque_lifetime(pools[0]),alpha_lifetime(pools[1]);WorldPolygonProducerList opaque{opaque_lifetime.pointer(),opaque_lifetime.pointer(start)},translucent{alpha_lifetime.pointer(),alpha_lifetime.pointer(start)};
        produce_world_model_unlit(model,slot,outer,camera,arena,identity,projection,clipped!=0,scratch,cursor,opaque,translucent);
        require(in.word()==cursor.count&&in.word()==opaque.count()&&in.word()==translucent.count(),"full-model source cursors differ");emitted+=opaque.count()+translucent.count()-2*start;
        const auto label="full-model case "+std::to_string(n)+" "+name+" slot "+std::to_string(slot);
        in.equal(vertices.data(),vertices.size()*sizeof(vertices.front()),(label+" vertices").c_str());
        const auto expected=in.value<std::array<std::byte,96>>();const auto mask=in.value<std::array<std::uint8_t,96>>();
        std::array<std::byte,96> actual;static_assert(sizeof(scratch)==96);std::memcpy(actual.data(),&scratch,96);
        const bool undefined=flags.size()<=1&&std::find(flags.begin(),flags.end(),1)!=flags.end();
        for(unsigned i=0;i<96;++i){const bool allowed=undefined&&((i>=12&&i<16)||(i>=28&&i<32));
            require(mask[i]==unsigned(allowed),"full-model unspecified W provenance");
            if(allowed)++in.unspecified;else{require(actual[i]==expected[i],(label+" camera/view scratch").c_str());++in.compared;}}
        in.equal(arena.matrices.data(),sizeof(arena.matrices),(label+" arena").c_str());require(in.word()==*arena.cursor&&in.word()==arena.count,"full-model matrix allocator state");
        for(const auto& pool:pools)for(const auto& packet:pool){
            std::uint32_t attribute=0;
            for(unsigned p=0;p<flags.size();++p)if(std::as_bytes(model.source_coordinates(lod,p)).data()==packet.attributes.bytes().data()){attribute=p+1;break;}
            require(attribute!=0,"full-model lost coordinate owner");
            const std::array<std::uint32_t,5> words{static_cast<std::uint32_t>(WorldPolygonHeaderView(model.source_attribute_table()).index_of(packet.header))+1,
                static_cast<std::uint32_t>(packet.indices.data()-model.source_corner_indices().data())+1,attribute,
                static_cast<std::uint32_t>(packet.vertices.data()-vertices.data())+1,packet.inside_mask};
            in.equal(words.data(),sizeof(words),(label+" packet").c_str());
        }
    }
    in.end();require(loaded.size()==78,"full-model original resource coverage");
    std::cout<<"{\"passed\":true,\"full_model_calls\":"<<cases<<",\"models\":"<<loaded.size()<<",\"emitted_packets\":"<<emitted<<",\"defined_bytes\":"<<in.compared<<",\"undefined_w_bytes\":"<<in.unspecified<<"}\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
