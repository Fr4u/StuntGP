#include "sgn/retail_world_track_parts.hpp"
#include "sgn/retail_track_part_initialization.hpp"
#include <fstream>
#include <iostream>

namespace {
using namespace sgn;using namespace sgn::retail;
void require(bool okay,const std::string& why){if(!okay)throw FormatError(why);}
class Reader {
    std::ifstream file_;
public:
    std::uint64_t compared{};
    explicit Reader(const char* path):file_(path,std::ios::binary){require(bool(file_),"missing track-parts corpus");}
    void bytes(void* data,std::size_t count){require(count<32*1024*1024,"oversized track-parts record");file_.read(static_cast<char*>(data),static_cast<std::streamsize>(count));require(bool(file_),"truncated track-parts corpus");}
    template<class T>T value(){T v;bytes(&v,sizeof(v));return v;}
    std::uint32_t word(){return value<std::uint32_t>();}
    template<class T>std::vector<T> array(std::size_t n){require(n<1024*1024,"oversized track array");std::vector<T> v(n);bytes(v.data(),v.size()*sizeof(T));return v;}
    template<class T>std::vector<T> array(){return array<T>(word());}
    std::string name(){const auto n=word();require(n&&n<128,"invalid track name");std::string s(n,'\0');bytes(s.data(),n);return s;}
    void equal(const void* data,std::size_t count,const std::string& label){const auto expected=array<std::byte>(count);
        if(count&&std::memcmp(data,expected.data(),count)!=0){const auto* actual=static_cast<const std::byte*>(data);std::size_t i=0;
            while(i<count&&actual[i]==expected[i])++i;throw FormatError(label+" byte "+std::to_string(i));}compared+=count;}
    void end(){require(file_.peek()==std::char_traits<char>::eof(),"trailing track-parts corpus");}
};
}
int main(int argc,char** argv)try{
    require(argc==3||(argc==4&&std::string_view(argv[3])=="--stale-projection-near"),"expected retail root, track-parts corpus and optional stale-near control");Reader in(argv[2]);const std::array<char,8> magic{'S','G','N','T','W','P','0','1'};in.equal(magic.data(),8,"wrong track-parts schema");
    const auto groups=in.word();require(groups==30,"all30 original tracks required");AssetStore assets(argv[1]);ModelResources resources(assets,argv[1]);
    std::uint64_t calls{},emitted{};
    for(unsigned group=0;group<groups;++group){
        const auto name=in.name();auto& model=resources.at(*resources.resolve(resources.load(name,name.starts_with("track")&&name.ends_with("m")?"trackset00":"")));
        const auto raw=read_file(std::filesystem::path(argv[1])/"meshdata"/(name+".PMD"));
        std::array<TrackPartAnimation,32> animations{};TrackPartPreparationScratch preparation;
        initialize_track_part_resources(1,&model,raw,animations,preparation);
        auto scratch=in.value<WorldModelViewScratch>();auto bound_matrix=in.value<CameraMatrix>();
        PmdLightBank lights;lights.records=in.array<RetailLightRecord>();LightViewSelection selection;
        std::array<std::uint8_t,0x4800> prepared;in.bytes(prepared.data(),prepared.size());LightVertexCursor light_cursor{prepared,in.word()};
        ColorTables colors;in.bytes(colors.raster_gamma.data(),sizeof(colors.raster_gamma));
        ModelTransform input_pose{},template_pose{};auto& bound_pose=model.source_outer_transform();bound_pose=&input_pose;
        const auto steps=in.word();require(steps==32,"32 retained track calls required");
        for(unsigned step=0;step<steps;++step,++calls){
            const auto operation=in.word(),lod=model.source_lod_index(0);model.current_detail_level()=in.word();const auto& current_lod=model.current_detail_level();require(operation<2,"track-parts operation");
            const auto flags=in.array<std::uint16_t>(),flags02=in.array<std::uint16_t>();require(flags.size()==model.part_count(lod)&&flags02.size()==flags.size(),"track part flags");
            for(unsigned i=0;i<flags.size();++i){model.part(lod,i).flags=flags[i];model.source_part_flags_02(lod,i)=flags02[i];}
            model.previous_detail_level()=in.word();model.source_model_ambience()=in.value<std::uint16_t>();
            const auto quality_first=in.word();auto quality_second=in.word();const auto rate=in.word();auto initialized=in.word();const auto stamp=in.word();auto stride=in.word(),accepted=in.word();
            selection.active_count=in.word();selection.ordered.clear();for(auto id:in.value<std::array<std::uint32_t,16>>())selection.ordered.emplace_back(id);
            RetailAmbientLight ambient;ambient.packed=in.value<std::array<std::uint32_t,3>>();input_pose=in.value<ModelTransform>();template_pose=in.value<ModelTransform>();
            const auto position=in.value<Vec3>();CameraState camera;camera.position={position.x,position.y,position.z,0};camera.world_to_camera_reference=2;
            CameraPresentationState presentation;presentation.forward_axis=in.value<CameraVector>();presentation.planes=in.value<std::array<CameraVector,4>>();
            CameraLifecycle lifecycle;lifecycle.clip_near=in.value<float>();lifecycle.clip_far=in.value<float>();auto projection=in.value<ProjectionParameters>();
            if(argc==4)projection.clip_near=std::bit_cast<float>(0x7e800000u);
            ViewMatrixArena matrices;in.bytes(matrices.matrices.data(),sizeof(matrices.matrices));matrices.cursor=64;matrices.count=1;
            allocate_outer_model_matrices(input_pose,matrices);allocate_outer_model_matrices(template_pose,matrices);allocate_model_part_matrices(model,0,matrices);const auto identity=vehicle_identity();
            auto vertices=in.array<WorldPolygonVertex>();const auto begin=in.word(),capacity=in.word(),start=in.word();require(begin<vertices.size()&&capacity<32768,"track-parts allocation");
            const auto source=world_polygon_part_source(model,lod,0);
            std::array<std::vector<WorldPolygonPacket>,4> pools;std::array<WorldPolygonProducerList,4> packets;std::array<std::unique_ptr<WorldPolygonPacketStorageLifetime>,4> packet_lifetimes;
            for(unsigned p=0;p<4;++p){pools[p].resize(capacity);for(auto& record:pools[p])record={WorldPolygonHeaderView(model.source_attribute_table()).at(0),model.source_corner_indices(),source.attributes,std::span(vertices).subspan(begin),in.word()};packet_lifetimes[p]=std::make_unique<WorldPolygonPacketStorageLifetime>(pools[p]);packets[p].publish_base(packet_lifetimes[p]->pointer());packets[p].publish_current(packet_lifetimes[p]->pointer(start));}
            WorldTransformedVertexCursor cursor{vertices,begin};WorldModelLightingBinding lighting{lights,selection,light_cursor,ambient,colors,in.word(),stamp,stride};
            const WorldTrackModelBinding binding{model,bound_pose,current_lod};
            if(operation==0)initialize_world_track_shading(binding,matrices,identity,scratch,quality_first,initialized,lighting);
            else produce_world_track_parts(binding,template_pose,camera,presentation,lifecycle,matrices,identity,projection,scratch,bound_matrix,cursor,
                packets,quality_first,quality_second,rate,initialized,accepted,lighting);
            const auto label="track "+name+" step "+std::to_string(step);
            require(in.word()==cursor.count,label+" vertex cursor");for(const auto& list:packets){require(in.word()==list.count(),label+" packet cursor");emitted+=list.count()-start;}
            const std::array<std::uint32_t,7> scalar{bound_pose==&template_pose?1u:0u,quality_second,initialized,stride,accepted,light_cursor.offset,model.previous_detail_level()};
            in.equal(scalar.data(),sizeof(scalar),label+" retained source words");in.equal(vertices.data(),vertices.size()*sizeof(WorldPolygonVertex),label+" vertices");
            in.equal(&scratch,sizeof(scratch),label+" scratch");in.equal(bound_matrix.data(),sizeof(bound_matrix),label+" bound matrix");
            in.equal(matrices.matrices.data(),sizeof(matrices.matrices),label+" matrix arena");require(in.word()==*matrices.cursor&&in.word()==matrices.count,label+" matrix cursor");
            for(const auto& pool:pools)for(const auto& packet:pool){
                std::uint32_t attribute=0;for(unsigned p=0;p<flags.size();++p)if(std::as_bytes(model.source_coordinates(lod,p)).data()==packet.attributes.bytes().data()){attribute=p+1;break;}
                require(attribute!=0,label+" coordinate binding");
                const std::array<std::uint32_t,5> record{static_cast<std::uint32_t>(WorldPolygonHeaderView(model.source_attribute_table()).index_of(packet.header))+1,
                    static_cast<std::uint32_t>(packet.indices.data()-model.source_corner_indices().data())+1,attribute,
                    static_cast<std::uint32_t>(packet.vertices.data()-vertices.data())+1,packet.inside_mask};in.equal(record.data(),sizeof(record),label+" packet");
            }
            in.equal(lights.records.data(),lights.records.size()*sizeof(RetailLightRecord),label+" lights");in.equal(prepared.data(),prepared.size(),label+" prepared light owner");
            for(unsigned i=0;i<flags.size();++i){const auto f=model.source_part_flags_02(lod,i);in.equal(&f,2,label+" part02");const auto coordinates=model.source_coordinates(lod,i);in.equal(coordinates.data(),coordinates.size_bytes(),label+" coordinate colors");}
        }
    }
    in.end();std::cout<<"{\"passed\":true,\"calls\":"<<calls<<",\"tracks\":"<<groups<<",\"packets\":"<<emitted<<",\"compared_bytes\":"<<in.compared<<"}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
