#include "sgn/retail_world_spatial_producer.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>

namespace {
using namespace sgn;using namespace sgn::retail;
void require(bool okay,const char* message){if(!okay)throw FormatError(message);}
class Reader {
    std::ifstream file_;
public:
    std::uint64_t compared{};
    explicit Reader(const char* path):file_(path,std::ios::binary){require(bool(file_),"missing spatial emitter corpus");}
    void bytes(void* data,std::size_t size){require(size<32*1024*1024,"oversized emitter record");file_.read(static_cast<char*>(data),static_cast<std::streamsize>(size));require(bool(file_),"truncated emitter corpus");}
    template<class T>T value(){T v;bytes(&v,sizeof(v));return v;}
    std::uint32_t word(){return value<std::uint32_t>();}
    template<class T>std::vector<T> array(){const auto n=word();require(n<1024*1024,"oversized emitter array");std::vector<T> v(n);bytes(v.data(),v.size()*sizeof(T));return v;}
    void equal(const void* data,std::size_t size,const char* message){std::vector<std::byte> expected(size);bytes(expected.data(),size);require(size==0||std::memcmp(data,expected.data(),size)==0,message);compared+=size;}
    void end(){require(file_.peek()==std::char_traits<char>::eof(),"trailing emitter corpus");}
};
}
int main(int argc,char** argv)try{
    require(argc==3,"expected retail root and spatial emitter corpus");Reader in(argv[2]);const std::array<char,8> magic{'S','G','N','S','P','E','0','1'};in.equal(magic.data(),8,"wrong emitter corpus");
    require(in.word()==30,"expected30 track models");AssetStore assets(argv[1]);ModelResources models(assets,argv[1]);std::uint64_t calls=0,emitted=0;
    for(unsigned m=0;m<30;++m){const auto name_bytes=in.array<char>();require(!name_bytes.empty()&&name_bytes.size()<128,"invalid emitter model name");const std::string name(name_bytes.begin(),name_bytes.end());
        const std::string set=name.starts_with("track")&&name.ends_with("m")?"trackset00":"";
        auto& model=models.at(*models.resolve(models.load(name,set)));const auto case_count=in.word();require(case_count>=8,"incomplete emitter cases");
        for(unsigned c=0;c<case_count;++c){++calls;const auto headers=in.array<WorldPolygonHeader>();auto actual=model.source_attributes(0,0);require(headers.size()==actual.size(),"source header count");std::ranges::copy(headers,actual.begin());
            const auto camera=in.value<VehicleVector>();auto vertices=in.array<WorldPolygonVertex>();require(vertices.size()==model.source_vertices(0,0).size()+2,"emitter vertex guards missing");
            model.source_model_flags_02()=in.value<std::uint16_t>();model.source_part_flags_02(0,0)=in.value<std::uint16_t>();const auto polygon_indices=in.array<std::uint16_t>();
            std::array<std::vector<WorldSpatialBatch>,2> batches;
            for(auto& list:batches){const auto offsets=in.array<std::uint32_t>();for(const auto offset:offsets){require(offset<polygon_indices.size(),"source emitter list offset");list.push_back({std::span(polygon_indices).subspan(offset),{}});}}
            const auto capacity=in.word();require(capacity>0&&capacity<1024*1024,"emitter pool capacity");const auto starts=in.value<std::array<std::uint32_t,4>>();auto batch_count=in.word();
            const auto source=world_polygon_part_source(model,0,0);std::array<std::vector<WorldPolygonPacket>,4> storage;std::array<WorldPolygonProducerList,4> pools;std::array<std::unique_ptr<WorldPolygonPacketStorageLifetime>,4> packet_lifetimes;
            for(unsigned p=0;p<4;++p){require(starts[p]<=capacity,"emitter pool start");storage[p].resize(capacity);for(auto& packet:storage[p])packet={source.headers.at(0),source.indices,source.attributes,vertices,in.word()};packet_lifetimes[p]=std::make_unique<WorldPolygonPacketStorageLifetime>(storage[p]);pools[p].publish_base(packet_lifetimes[p]->pointer());pools[p].publish_current(packet_lifetimes[p]->pointer(starts[p]));}
            const auto coordinates=model.source_coordinates(0,0);const auto source_vertices=model.source_vertices(0,0);const auto normals=model.source_normals();const auto indices=model.source_corner_indices();
            const auto owner_snapshot=[&](){std::vector<std::byte> result;const auto add=[&](auto view){const auto bytes=std::as_bytes(view);result.insert(result.end(),bytes.begin(),bytes.end());};add(actual);add(coordinates);add(source_vertices);add(normals);add(indices);add(std::span(vertices));return result;};
            const auto before=owner_snapshot();
            emit_world_spatial_polygons(batches[0],batches[1],source,vertices,camera,batch_count,pools[0],pools[1],pools[2],pools[3]);
            require(before==owner_snapshot(),"spatial emitter mutated borrowed owners");require(batch_count==in.word(),"source batch counter differs");in.compared+=4;
            for(unsigned p=0;p<4;++p){require(pools[p].count()==in.word(),"spatial emitter cursor differs");emitted+=pools[p].count()-starts[p];in.compared+=4;}
            for(const auto& list:storage)for(const auto& packet:list){
                require(packet.header&&packet.attributes.bytes().data()==source.attributes.bytes().data()&&packet.vertices.data()==vertices.data(),"spatial packet changed source identity");
                const std::array<std::uint32_t,5> tokens{static_cast<std::uint32_t>(source.headers.index_of(packet.header))+1,
                    static_cast<std::uint32_t>(packet.indices.data()-source.indices.data())+1,1,1,packet.inside_mask};
                const auto label="spatial emitter model "+name+" case "+std::to_string(c);in.equal(tokens.data(),sizeof(tokens),label.c_str());
            }
        }
    }
    in.end();std::cout<<"{\"passed\":true,\"models\":30,\"full_42a220_calls\":"<<calls<<",\"emitted_packets\":"<<emitted<<",\"compared_bytes\":"<<in.compared<<"}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
