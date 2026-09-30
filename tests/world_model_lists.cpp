#include "sgn/retail_world_model_lists.hpp"
#include <fstream>
#include <iostream>
#include <map>

namespace {
using namespace sgn;using namespace sgn::retail;
void require(bool okay,const std::string& why){if(!okay)throw FormatError(why);}
class Reader {
    std::ifstream file_;
public:
    std::uint64_t compared{},unspecified{};
    explicit Reader(const char* path):file_(path,std::ios::binary){require(bool(file_),"missing model-list corpus");}
    void bytes(void* data,std::size_t count){require(count<32*1024*1024,"oversized model-list record");file_.read(static_cast<char*>(data),static_cast<std::streamsize>(count));require(bool(file_),"truncated model-list corpus");}
    template<class T>T value(){T value;bytes(&value,sizeof(value));return value;}
    std::uint32_t word(){return value<std::uint32_t>();}
    template<class T>std::vector<T> array(std::size_t count){require(count<1024*1024,"oversized model-list array");std::vector<T> values(count);bytes(values.data(),values.size()*sizeof(T));return values;}
    template<class T>std::vector<T> array(){return array<T>(word());}
    std::string name(){const auto count=word();require(count&&count<128,"invalid model name");std::string name(count,'\0');bytes(name.data(),count);return name;}
    void equal(const void* data,std::size_t count,const std::string& label){const auto expected=array<std::byte>(count);
        if(count&&std::memcmp(data,expected.data(),count)!=0){const auto* actual=static_cast<const std::byte*>(data);std::size_t i=0;
            while(i<count&&actual[i]==expected[i])++i;throw FormatError(label+" byte "+std::to_string(i));}compared+=count;}
    void equal_word(std::uint32_t value,const std::string& label){equal(&value,4,label);}
    void end(){require(file_.peek()==std::char_traits<char>::eof(),"trailing model-list corpus");}
};
}
int main(int argc,char** argv)try{
    require(argc==3,"expected retail root and model-list corpus");Reader in(argv[2]);const std::array<char,8> magic{'S','G','N','L','S','T','0','1'};
    in.equal(magic.data(),magic.size(),"model-list corpus schema");const auto group_count=in.word();require(group_count==28,"20 cars and8 skies required");
    AssetStore assets(argv[1]);ModelResources resources(assets,argv[1]);std::uint64_t calls{},packets{},host_checks{};
    for(unsigned group=0;group<group_count;++group){
        const auto name=in.name();const auto parts=in.word();require(parts>0&&parts<40,"invalid source part count");
        std::array<std::uint32_t,2> identities{resources.load(name),resources.load(name)};
        std::array<ModelResource*,2> models{&resources.at(*resources.resolve(identities[0])),&resources.at(*resources.resolve(identities[1]))};
        require(models[0]!=models[1]&&identities[0]!=identities[1],"repeated load must retain separate owners");++host_checks;
        std::uint32_t session_lod=0;models[0]->bind_current_detail_level(session_lod);
        for(unsigned m=0;m<2;++m){models[m]->current_detail_level()=m*2+1;models[m]->previous_detail_level()=0x12340000u+m;require(models[m]->source_outer_transform()==nullptr,"initial descriptor pose");}
        WorldPolygonFrame frame;allocate_world_polygon_frame(frame);frame.model_view=in.value<WorldModelViewScratch>();frame.bound_matrix=in.value<CameraMatrix>();
        PmdLightBank lights;lights.records=in.array<RetailLightRecord>(16);std::array<std::uint8_t,0x4800> prepared;in.bytes(prepared.data(),prepared.size());
        LightVertexCursor light_cursor{prepared,in.word()};ColorTables colors;in.bytes(colors.raster_gamma.data(),sizeof(colors.raster_gamma));
        frame.lighting_stride=in.word();frame.accepted_models=in.word();
        for(auto& group_ranges:frame.groups)for(unsigned p=0;p<4;++p){auto& range=group_ranges[p];const auto first=in.word();range.first=first?frame.allocations->packet_lifetimes[p].pointer(first-1):WorldPolygonPacketPointer{};range.count=in.word();}
        const auto steps=in.word();require(steps==24,"24 retained calls per model required");bool undefined=false;
        std::array<ModelDrawCommand,6> commands{};
        for(unsigned step=0;step<steps;++step,++calls){
            const auto label=name+" step "+std::to_string(step)+" ";
            for(auto* model:models){
                model->source_model_flags_02()=in.value<std::uint16_t>();model->source_model_ambience()=in.value<std::uint16_t>();
                for(unsigned slot=0;slot<4;++slot){const auto lod=model->source_lod_index(slot);require(model->part_count(lod)==parts,"source LOD part count");
                    for(unsigned p=0;p<parts;++p){model->part(lod,p).flags=in.value<std::uint16_t>();model->source_part_flags_02(lod,p)=in.value<std::uint16_t>();model->source_part_word_04(lod,p)=in.value<std::uint16_t>();}}
            }
            const auto operation=in.word(),source_rate=in.word(),lighting_enabled=in.word(),quality=in.word();frame.frame_stamp=in.word();
            LightViewSelection selection;selection.active_count=in.word();for(auto id:in.value<std::array<std::uint32_t,16>>())selection.ordered.emplace_back(id);
            RetailAmbientLight ambient;ambient.packed=in.value<std::array<std::uint32_t,3>>();
            const auto camera_position=in.value<Vec3>();CameraState camera;camera.position={camera_position.x,camera_position.y,camera_position.z,0};camera.world_to_camera_reference=2;
            CameraPresentationState presentation;presentation.forward_axis=in.value<CameraVector>();presentation.planes=in.value<std::array<CameraVector,4>>();
            CameraLifecycle lifecycle;lifecycle.clip_near=in.value<float>();lifecycle.clip_far=in.value<float>();camera.lens=in.value<float>();const auto projection=in.value<ProjectionParameters>();
            ViewMatrixArena arena;in.bytes(arena.matrices.data(),sizeof(arena.matrices));arena.cursor=64;arena.count=1;const auto identity=vehicle_identity();
            in.bytes(commands.data(),sizeof(commands));std::array<std::uint32_t,6> command_tokens;
            for(unsigned i=0;i<commands.size();++i){auto& command=commands[i];command_tokens[i]=command.model_identity;
                if(command.model_identity==1||command.model_identity==2){command.model_identity=identities[command.model_identity-1];allocate_outer_model_matrices(command.pose,arena);}}
            for(auto* model:models)for(unsigned slot=0;slot<=model->source_maximum_detail_level();++slot)allocate_model_part_matrices(*model,slot,arena);
            const auto ignored_queue_cursor=in.word();require(ignored_queue_cursor<6,"fixture queue cursor");
            auto initial_vertices=in.array<WorldPolygonVertex>();require(initial_vertices.size()<=frame.allocations->vertices.size(),"source vertex capacity");
            std::copy(initial_vertices.begin(),initial_vertices.end(),frame.allocations->vertices.begin());
            const auto begin=in.word(),capacity=in.word(),start=in.word();require(begin<initial_vertices.size()&&start<capacity&&capacity<=3072,"source packet capacity");
            auto vertex_owner=std::span(frame.allocations->vertices).first(initial_vertices.size());frame.transformed={vertex_owner,begin};
            auto& first=*models[0];const auto first_source=world_polygon_part_source(first,first.source_lod_index(0),0);
            for(unsigned p=0;p<4;++p){auto storage=std::span(frame.allocations->packets[p]).first(capacity);frame.packets[p].publish_current(frame.allocations->packet_lifetimes[p].pointer(start));frame.totals[p]=0xdead1000u+p;
                for(auto& packet:storage)packet={WorldPolygonHeaderView(first.source_attribute_table()).at(0),first.source_corner_indices(),first_source.attributes,vertex_owner.subspan(begin),in.word()};}
            const auto before_commands=commands;const auto before_groups=frame.groups;const auto before_pose=std::array{models[0]->source_outer_transform(),models[1]->source_outer_transform()};
            if(operation==0)produce_world_regular_models(commands,resources,camera,presentation,lifecycle,arena,identity,projection,frame,source_rate,lighting_enabled,
                {lights,selection,light_cursor,ambient,colors,quality,frame.frame_stamp,frame.lighting_stride});
            else{require(operation==1&&name.starts_with("sky"),"invalid sky operation");produce_world_sky(commands[0],resources,camera,lifecycle,arena,identity,projection,frame);}
            require(std::memcmp(commands.data(),before_commands.data(),sizeof(commands))==0,label+"command pose mutation");++host_checks;
            if(!operation){for(unsigned g=0;g<4;++g)for(unsigned p=0;p<4;++p)require(frame.groups[g][p].first.same_pointer(before_groups[g][p].first)&&frame.groups[g][p].count==before_groups[g][p].count,label+"regular group mutation");++host_checks;}
            if(!operation&&!commands[0].model_identity){require(models[0]->source_outer_transform()==before_pose[0]&&models[1]->source_outer_transform()==before_pose[1],label+"empty-list retained descriptor");++host_checks;}
            require(&models[0]->current_detail_level()==&session_lod,label+"lost sole Session LOD owner");++host_checks;
            in.equal_word(frame.transformed.count,label+"vertex cursor");for(const auto& pool:frame.packets){in.equal_word(pool.count(),label+"packet cursor");packets+=pool.count()-start;}
            in.equal_word(frame.lighting_stride,label+"lighting stride");in.equal_word(frame.accepted_models,label+"accepted models");in.equal_word(light_cursor.offset,label+"light cursor");in.equal(&lifecycle.clip_far,4,label+"sky far");
            for(auto* model:models){std::uint32_t pose_token=0;if(model->source_outer_transform()){
                for(unsigned i=0;i<commands.size();++i)if(model->source_outer_transform()==&commands[i].pose)pose_token=i+1;require(pose_token!=0,label+"descriptor lost retained command owner");}
                in.equal_word(pose_token,label+"descriptor pose token");in.equal_word(model->current_detail_level(),label+"current LOD");in.equal_word(model->previous_detail_level(),label+"previous LOD");}
            in.equal(vertex_owner.data(),vertex_owner.size_bytes(),label+"transformed vertices");
            const auto expected_scratch=in.value<std::array<std::byte,96>>();const auto undefined_token=in.word();
            if(parts<=1&&frame.transformed.count>begin)undefined=true;
            require(undefined_token==unsigned(undefined),label+"undefined W provenance");
            const auto actual_scratch=std::as_bytes(std::span(&frame.model_view,1));static_assert(sizeof(WorldModelViewScratch)==96);
            for(unsigned i=0;i<96;++i){if(undefined&&((i>=12&&i<16)||(i>=28&&i<32)))++in.unspecified;
                else{require(actual_scratch[i]==expected_scratch[i],label+"camera scratch byte "+std::to_string(i));++in.compared;}}
            in.equal(frame.bound_matrix.data(),sizeof(frame.bound_matrix),label+"bound matrix");in.equal(arena.matrices.data(),sizeof(arena.matrices),label+"matrix arena");
            in.equal_word(*arena.cursor,label+"matrix cursor");in.equal_word(arena.count,label+"matrix count");
            auto normalized_commands=commands;for(unsigned i=0;i<commands.size();++i)normalized_commands[i].model_identity=command_tokens[i];
            in.equal(normalized_commands.data(),sizeof(normalized_commands),label+"retained commands");
            std::map<const std::byte*,std::pair<unsigned,unsigned>> coordinate_tokens;
            for(unsigned m=0;m<2;++m)for(unsigned slot=0;slot<4;++slot)for(unsigned p=0;p<parts;++p)
                coordinate_tokens.try_emplace(std::as_bytes(models[m]->source_coordinates(models[m]->source_lod_index(slot),p)).data(),m+1,slot*parts+p+1);
            for(const auto& pool:frame.packets)for(const auto& packet:pool.base().storage().first(capacity)){
                const auto found=coordinate_tokens.find(packet.attributes.bytes().data());require(found!=coordinate_tokens.end(),label+"lost coordinate allocation");
                const auto [owner,token]=found->second;auto& model=*models[owner-1];
                const std::array<std::uint32_t,6> words{owner,static_cast<std::uint32_t>(WorldPolygonHeaderView(model.source_attribute_table()).index_of(packet.header))+1,
                    static_cast<std::uint32_t>(packet.indices.data()-model.source_corner_indices().data())+1,token,
                    static_cast<std::uint32_t>(packet.vertices.data()-vertex_owner.data())+1,packet.inside_mask};
                in.equal(words.data(),sizeof(words),label+"packet owner/attributes/mask");}
            in.equal(frame.totals.data(),sizeof(frame.totals),label+"untouched totals");
            for(const auto& ranges:frame.groups)for(const auto& range:ranges){in.equal_word(range.first.is_null()?0u:static_cast<std::uint32_t>(*range.first.offset()+1),label+"group start");in.equal_word(range.count,label+"group count");}
            in.equal(lights.records.data(),16*sizeof(RetailLightRecord),label+"retained light bank");in.equal(prepared.data(),prepared.size(),label+"retained prepared lights");
            for(auto* model:models)for(unsigned slot=0;slot<4;++slot){const auto lod=model->source_lod_index(slot);
                for(unsigned p=0;p<parts;++p){const std::array<std::uint16_t,3> flags{model->part(lod,p).flags,model->source_part_flags_02(lod,p),model->source_part_word_04(lod,p)};
                    in.equal(flags.data(),sizeof(flags),label+"part fields");const auto coordinates=model->source_coordinates(lod,p);in.equal(coordinates.data(),coordinates.size_bytes(),label+"live coordinate colors");}}
        }
    }
    in.end();require(calls==672&&resources.size()==56,"model-list corpus membership");
    std::cout<<"{\"passed\":true,\"full_calls\":"<<calls<<",\"models\":"<<resources.size()<<",\"emitted_packets\":"<<packets
        <<",\"defined_bytes\":"<<in.compared<<",\"undefined_w_bytes\":"<<in.unspecified<<",\"host_checks\":"<<host_checks<<"}\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
