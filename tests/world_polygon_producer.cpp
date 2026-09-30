#include "sgn/retail_world_polygon_producer.hpp"
#include <fstream>
#include <iostream>
#include <map>

namespace {
using namespace sgn;using namespace sgn::retail;
void require(bool okay,const char* message){if(!okay)throw FormatError(message);}
class Reader {
    std::ifstream file_;
public:
    std::uint64_t compared_bytes{};
    explicit Reader(const char* path):file_(path,std::ios::binary){require(bool(file_),"missing original corpus");}
    void bytes(void* data,std::size_t count){require(count<=32*1024*1024,"oversized original record");file_.read(static_cast<char*>(data),static_cast<std::streamsize>(count));require(bool(file_),"truncated original corpus");}
    template<class T>T value(){T v;bytes(&v,sizeof(v));return v;}
    std::uint32_t word(){return value<std::uint32_t>();}
    template<class T>std::vector<T> array(std::size_t count){require(count<=1024*1024,"oversized original array");std::vector<T> v(count);bytes(v.data(),v.size()*sizeof(T));return v;}
    template<class T>std::vector<T> array(){return array<T>(word());}
    std::string name(){const auto n=word();require(n<128,"invalid original model name");std::string v(n,'\0');bytes(v.data(),n);return v;}
    void equal(const void* data,std::size_t count,const char* message){const auto expected=array<std::byte>(count);if(count&&std::memcmp(data,expected.data(),count)!=0)throw FormatError(message);compared_bytes+=count;}
    void end(){require(file_.peek()==std::char_traits<char>::eof(),"trailing original corpus bytes");}
};
std::array<std::uint32_t,5> canonical(const WorldPolygonPacket& packet,const WorldPolygonPartSource& source,
    std::span<WorldPolygonVertex> vertices){
    require(source.headers.find(packet.header).has_value(),"packet header escaped original owner");
    require(packet.indices.data()>=source.indices.data()&&packet.indices.data()<=source.indices.data()+source.indices.size(),"packet indices escaped original owner");
    require(packet.attributes.bytes().data()==source.attributes.bytes().data()&&packet.attributes.size()==source.attributes.size(),"packet lost live attribute owner");
    require(packet.vertices.data()==vertices.data()&&packet.vertices.size()==vertices.size(),"packet lost transformed vertex owner");
    return {static_cast<std::uint32_t>(source.headers.index_of(packet.header))+1,
        static_cast<std::uint32_t>(packet.indices.data()-source.indices.data())+1,1,1,packet.inside_mask};
}
}
int main(int argc,char** argv)try{
    require(argc==3,"expected retail root and original corpus");Reader in(argv[2]);const std::array<char,8> magic{'S','G','N','W','P','0','0','1'};in.equal(magic.data(),8,"wrong original corpus magic");
    const auto streams=in.word();require(streams>=200,"incomplete transform corpus");
    for(unsigned n=0;n<streams;++n){
        const auto clipped=in.word();const auto input=in.array<VehicleVector>();const auto matrix=in.value<std::array<float,16>>();
        const auto parameters=in.value<ProjectionParameters>();auto output=in.array<WorldPolygonVertex>();
        require(output.size()==input.size()+2,"missing transform retained tail guards");
        if(clipped)transform_world_vertices_clipped(input,output,matrix,parameters);else transform_world_vertices_unclipped(input,output,matrix,parameters);
        in.equal(output.data(),output.size()*sizeof(output.front()),("transform case "+std::to_string(n)).c_str());
    }
    AssetStore assets(argv[1]);ModelResources models(assets,argv[1]);std::map<std::string,std::size_t> loaded;
    const auto emitters=in.word();require(emitters>=3000,"incomplete emitter corpus");std::uint64_t packets=0;
    for(unsigned n=0;n<emitters;++n){
        const auto clipped=in.word();const auto name=in.name();const auto lod=in.word(),part=in.word();
        std::vector<WorldPolygonHeader> headers;std::vector<WorldPolygonAttribute> attributes;std::vector<VehicleVector> points,normals;std::vector<std::uint16_t> indices;
        WorldPolygonPartSource source;
        if(name.empty()){
            headers=in.array<WorldPolygonHeader>();attributes=in.array<WorldPolygonAttribute>();points=in.array<VehicleVector>();normals=in.array<VehicleVector>();indices=in.array<std::uint16_t>();
            source={WorldPolygonHeaderView(std::span(headers)),WorldPolygonAttributeView(std::span(attributes)),points,normals,indices};
        }else{
            auto found=loaded.find(name);if(found==loaded.end()){
                const auto set=name.starts_with("track")&&name.ends_with("m")?"trackset00":"";
                found=loaded.emplace(name,*models.resolve(models.load(name,set))).first;
            }
            source=world_polygon_part_source(models.at(found->second),lod,part);
        }
        source.model_flags_02=in.value<std::uint16_t>();source.part_flags_02=in.value<std::uint16_t>();
        const auto camera=in.value<VehicleVector>();auto vertices=in.array<WorldPolygonVertex>();
        const std::array<std::span<const std::byte>,5> readonly{source.headers.bytes(),source.attributes.bytes(),
            std::as_bytes(source.vertices),std::as_bytes(source.normals),std::as_bytes(source.indices)};
        std::array<std::vector<std::byte>,5> source_before;
        for(unsigned i=0;i<readonly.size();++i)source_before[i].assign(readonly[i].begin(),readonly[i].end());
        const auto capacity=in.word(),start=in.word();require(capacity==source.headers.size()+start+2,"missing packet retained tail guards");
        std::array<std::vector<WorldPolygonPacket>,2> records{std::vector<WorldPolygonPacket>(capacity),std::vector<WorldPolygonPacket>(capacity)};
        const WorldPolygonHeader empty_header{};const auto initial=source.headers.empty()?world_polygon_header_ref(empty_header):source.headers.at(0);
        for(unsigned pool=0;pool<2;++pool)for(unsigned i=0;i<capacity;++i){
            records[pool][i]={initial,source.indices,source.attributes,vertices,in.word()};
        }
        WorldPolygonPacketStorageLifetime opaque_lifetime(records[0]),alpha_lifetime(records[1]);WorldPolygonProducerList opaque{opaque_lifetime.pointer(),opaque_lifetime.pointer(start)},translucent{alpha_lifetime.pointer(),alpha_lifetime.pointer(start)};
        if(clipped)emit_world_polygons_clipped(source,vertices,camera,opaque,translucent);
        else emit_world_polygons_unclipped(source,vertices,camera,opaque,translucent);
        for(unsigned i=0;i<readonly.size();++i)require(std::equal(readonly[i].begin(),readonly[i].end(),source_before[i].begin()),"native emitter mutated source owner");
        require(in.word()==opaque.count()&&in.word()==translucent.count(),"packet cursor differs from original");packets+=opaque.count()+translucent.count()-2*start;
        for(const auto& pool:records)for(const auto& packet:pool){
            if(source.headers.empty()){
                require(packet.header==world_polygon_header_ref(empty_header)&&packet.indices.data()==source.indices.data()&&packet.vertices.data()==vertices.data(),"zero-header emitter wrote a packet");
                const std::array<std::uint32_t,5> actual{1,1,1,1,packet.inside_mask};in.equal(actual.data(),sizeof(actual),"zero-header retained packet differs");
            }else{
                const auto actual=canonical(packet,source,vertices);in.equal(actual.data(),sizeof(actual),("emitter case "+std::to_string(n)).c_str());
            }
        }
        // Emitters only read source/transformed storage. Their source bytes are
        // compared after the call, including live coordinate ownership.
        in.equal(vertices.data(),vertices.size()*sizeof(vertices.front()),"emitter mutated transformed vertices");
    }
    in.end();require(loaded.size()==78,"missing original PMD model coverage");
    bool rejected=false;try{const std::array<VehicleVector,1> input{};transform_world_vertices_clipped(input,{}, {}, {});}catch(const FormatError&){rejected=true;}
    require(rejected,"short transformed allocation accepted");
    std::cout<<"{\"passed\":true,\"transform_calls\":"<<streams<<",\"emitter_calls\":"<<emitters<<",\"models\":"<<loaded.size()<<",\"emitted_packets\":"<<packets<<",\"compared_bytes\":"<<in.compared_bytes<<"}\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
