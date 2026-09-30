#include "sgn/retail_world_background_models.hpp"
#include "sgn/retail_vehicle_strip.hpp"
#include <fstream>
#include <iostream>
#include <map>

using namespace sgn;
using namespace sgn::retail;
namespace {
void require(bool value,const std::string& message){if(!value)throw FormatError(message);}
struct Reader {
    std::ifstream file;
    std::uint64_t compared{},unknown{};
    explicit Reader(const char* path):file(path,std::ios::binary){require(bool(file),"missing background corpus");}
    void bytes(void* p,std::size_t n){require(n<32*1024*1024,"oversized background record");file.read(static_cast<char*>(p),static_cast<std::streamsize>(n));require(bool(file),"truncated background corpus");}
    template<class T>T value(){T result{};bytes(&result,sizeof(result));return result;}
    std::uint32_t word(){return value<std::uint32_t>();}
    template<class T>std::vector<T> array(std::size_t n){require(n<1024*1024,"oversized background array");std::vector<T> result(n);bytes(result.data(),n*sizeof(T));return result;}
    template<class T>std::vector<T> array(){return array<T>(word());}
    void equal(const void* p,std::size_t n,const std::string& label){const auto expected=array<std::byte>(n);
        const auto* actual=static_cast<const std::byte*>(p);for(std::size_t i=0;i<n;++i)if(actual[i]!=expected[i])throw FormatError(label+" byte "+std::to_string(i));compared+=n;}
    void equal_word(std::uint32_t n,const std::string& label){equal(&n,4,label);}
};
template<class T>std::uint32_t pointer_token(std::span<T> owner,const void* pointer){
    // Equality against actual owner elements is valid across unrelated owners.
    for(std::size_t i=0;i<=owner.size();++i)if(owner.data()+i==pointer)return static_cast<std::uint32_t>(i+1);
    throw FormatError("background packet has unrelated indexed owner");
}
void generated_parts(WorldModelDrawState& draws,VehicleStripGeometry& strip,ViewMatrixArena& arena){
    const Vec3 pivot{};
    const std::array<ModelPartBinding,1> sprites{{{draws.sprite_part_flags,draws.sprite_part_transform,pivot}}};
    allocate_model_part_matrices(sprites,arena);
    auto part=std::bit_cast<ModelTransform>(strip.part_transform);
    const std::array<ModelPartBinding,1> strips{{{strip.part_flags,part,pivot}}};
    allocate_model_part_matrices(strips,arena);strip.part_transform=std::bit_cast<VehicleStripTransform>(part);
}
void read_sprite(Reader& in,SourceSpritePacketStorage& s){
    in.bytes(s.headers.data(),sizeof(s.headers));in.bytes(s.indices.data(),sizeof(s.indices));
    in.bytes(s.positions.data(),sizeof(s.positions));in.bytes(s.attributes.data(),sizeof(s.attributes));
}
void read_strip(Reader& in,VehicleStripGeometry& s){
    in.bytes(s.polygons.data(),sizeof(s.polygons));in.bytes(s.corner_indices.data(),sizeof(s.corner_indices));
    in.bytes(s.vertices.data(),sizeof(s.vertices));in.bytes(s.corners.data(),sizeof(s.corners));in.bytes(s.normals.data(),sizeof(s.normals));
}
void equal_sprite(Reader& in,const SourceSpritePacketStorage& s,const std::string& label){
    in.equal(s.headers.data(),sizeof(s.headers),label+" sprite headers");in.equal(s.indices.data(),sizeof(s.indices),label+" sprite indices");
    in.equal(s.positions.data(),sizeof(s.positions),label+" sprite positions");in.equal(s.attributes.data(),sizeof(s.attributes),label+" sprite attributes");
}
void equal_strip(Reader& in,const VehicleStripGeometry& s,const std::string& label){
    in.equal(s.polygons.data(),sizeof(s.polygons),label+" strip headers");in.equal(s.corner_indices.data(),sizeof(s.corner_indices),label+" strip indices");
    in.equal(s.vertices.data(),sizeof(s.vertices),label+" strip vertices");in.equal(s.corners.data(),sizeof(s.corners),label+" strip attributes");in.equal(s.normals.data(),sizeof(s.normals),label+" strip normals");
}
}
int main(int argc,char** argv)try{
    require(argc==3,"expected retail root and background corpus");Reader in(argv[2]);
    const std::array<char,8> magic{'S','G','N','B','G','D','0','1'};in.equal(magic.data(),magic.size(),"background schema");
    require(in.word()==20,"20 actual car groups required");AssetStore assets(argv[1]);ModelResources resources(assets,argv[1]);
    std::uint64_t calls{},emitted{},checks{};std::array<std::uint64_t,4> owner_packets{};
    for(unsigned group=0;group<20;++group){
        const auto raw_name=in.array<char>();require(!raw_name.empty()&&raw_name.size()<128,"invalid background model name");const std::string name(raw_name.begin(),raw_name.end());
        const auto parts=in.word();require(parts==25,"actual car parts required");
        std::array<std::uint32_t,2> ids{resources.load(name),resources.load(name)};
        std::array<ModelResource*,2> models{&resources.at(*resources.resolve(ids[0])),&resources.at(*resources.resolve(ids[1]))};
        require(models[0]!=models[1]&&ids[0]!=ids[1],"distinct PMD instances lost");++checks;
        std::uint32_t session_lod{};models[0]->bind_current_detail_level(session_lod);
        for(unsigned m=0;m<2;++m){models[m]->current_detail_level()=m*2+1;models[m]->previous_detail_level()=0x12340000u+m;}
        auto draws=std::make_unique<WorldModelDrawState>();auto sprites=std::make_unique<SourceSpritePacketStorage>();auto strip=std::make_unique<VehicleStripGeometry>();
        read_sprite(in,*sprites);read_strip(in,*strip);
        sprites->polygons=in.value<std::uint16_t>();sprites->vertices=in.value<std::uint16_t>();sprites->corners=in.value<std::uint16_t>();
        //Deliberately unrelated append cursors; only live part counts feed42A260.
        sprites->header_bytes=4096;sprites->index_count=17;sprites->position_count=19;sprites->attribute_count=23;
        draws->sprite_part_transform=in.value<ModelTransform>();strip->part_transform=in.value<VehicleStripTransform>();
        WorldPolygonFrame frame;allocate_world_polygon_frame(frame);frame.model_view=in.value<WorldModelViewScratch>();frame.bound_matrix=in.value<CameraMatrix>();
        frame.accepted_models=in.word();frame.lighting_stride=in.word();
        for(auto& g:frame.groups)for(unsigned p=0;p<4;++p){auto& range=g[p];const auto first=in.word();range.first=first?frame.allocations->packet_lifetimes[p].pointer(first-1):WorldPolygonPacketPointer{};range.count=in.word();}
        require(in.word()==24,"24 retained calls required");bool unknown=false;std::array<ModelDrawCommand,8> commands{};
        for(unsigned step=0;step<24;++step,++calls){
            const auto label=name+" step "+std::to_string(step);
            for(auto* model:models){model->source_model_flags_02()=in.value<std::uint16_t>();
                for(unsigned slot=0;slot<4;++slot){const auto lod=model->source_lod_index(slot);
                    for(unsigned p=0;p<parts;++p){model->part(lod,p).flags=in.value<std::uint16_t>();model->source_part_flags_02(lod,p)=in.value<std::uint16_t>();model->source_part_word_04(lod,p)=in.value<std::uint16_t>();}}}
            draws->sprite_part_flags=in.value<std::uint16_t>();draws->sprite_model_flags=in.value<std::uint16_t>();draws->sprite_part_flags_02=in.value<std::uint16_t>();
            strip->part_flags=in.value<std::uint16_t>();strip->model_flags_02=in.value<std::uint16_t>();strip->part_flags_02=in.value<std::uint16_t>();
            strip->polygon_count=in.value<std::uint16_t>();strip->corner_count=in.value<std::uint16_t>();strip->vertex_count=in.value<std::uint16_t>();
            const auto rate=in.word();const auto camera_position=in.value<Vec3>();CameraState camera;camera.position={camera_position.x,camera_position.y,camera_position.z,0};camera.world_to_camera_reference=2;
            CameraPresentationState presentation;presentation.forward_axis=in.value<CameraVector>();presentation.planes=in.value<std::array<CameraVector,4>>();
            CameraLifecycle lifecycle;lifecycle.clip_near=in.value<float>();lifecycle.clip_far=in.value<float>();camera.lens=in.value<float>();const auto projection=in.value<ProjectionParameters>();
            ViewMatrixArena arena;in.bytes(arena.matrices.data(),sizeof(arena.matrices));arena.cursor=64;arena.count=1;const auto identity=vehicle_identity();
            in.bytes(commands.data(),sizeof(commands));std::array<std::uint32_t,8> tokens{};
            bool ended=false;
            for(unsigned i=0;i<commands.size();++i){auto& command=commands[i];tokens[i]=command.model_identity;
                if(!ended){if(tokens[i]==0)ended=true;else if(tokens[i]==3&&draws->sprite_part_flags==1)unknown=true;else if(tokens[i]==4&&strip->part_flags==1)unknown=true;}
                if(tokens[i]>=1&&tokens[i]<=4){command.model_identity=tokens[i]<=2?ids[tokens[i]-1]:tokens[i]==3?sprite_model_identity:strip_model_identity;allocate_outer_model_matrices(command.pose,arena);}}
            require(ended,"source fixture missing terminator");
            for(auto* model:models)for(unsigned slot=0;slot<=model->source_maximum_detail_level();++slot)allocate_model_part_matrices(*model,slot,arena);
            generated_parts(*draws,*strip,arena);require(in.word()<8,"invalid irrelevant queue cursor");
            const auto initial=in.array<WorldPolygonVertex>();require(initial.size()<=frame.allocations->vertices.size(),"background vertex capacity");
            std::copy(initial.begin(),initial.end(),frame.allocations->vertices.begin());const auto begin=in.word(),capacity=in.word(),start=in.word();
            require(begin<initial.size()&&start<capacity&&capacity<=3072,"background cursor capacity");
            auto vertices=std::span(frame.allocations->vertices).first(initial.size());frame.transformed={vertices,begin};
            const auto first=world_polygon_part_source(*models[0],models[0]->source_lod_index(0),0);
            for(unsigned p=0;p<4;++p){auto storage=std::span(frame.allocations->packets[p]).first(capacity);frame.packets[p].publish_current(frame.allocations->packet_lifetimes[p].pointer(start));frame.totals[p]=0xdead1000u+p;
                for(auto& packet:storage)packet={world_polygon_header_ref(models[0]->source_attribute_table().front()),models[0]->source_corner_indices(),first.attributes,vertices.subspan(begin),in.word()};}
            const auto commands_before=commands;const auto camera_before=camera;const auto lifecycle_before=lifecycle;
            produce_world_background_models(commands,resources,*draws,*sprites,*strip,camera,presentation,lifecycle,arena,identity,projection,frame,rate);
            require(std::memcmp(commands.data(),commands_before.data(),sizeof(commands))==0,label+" command mutation");++checks;
            require(std::memcmp(&camera,&camera_before,sizeof(camera))==0&&std::memcmp(&lifecycle,&lifecycle_before,sizeof(lifecycle))==0,label+" camera mutation");++checks;
            require(&models[0]->current_detail_level()==&session_lod,label+" lost canonical LOD");++checks;
            in.equal_word(frame.transformed.count,label+" vertex cursor");for(auto& p:frame.packets){in.equal_word(p.count(),label+" packet cursor");emitted+=p.count()-start;}
            in.equal_word(frame.accepted_models,label+" accepted");in.equal_word(frame.lighting_stride,label+" lighting stride");
            for(unsigned m=0;m<4;++m){const auto* pose=m<2?models[m]->source_outer_transform():m==2?draws->sprite_outer_transform:strip->outer_transform;
                std::uint32_t token{};if(pose){for(unsigned i=0;i<commands.size();++i)if(pose==&commands[i].pose)token=i+1;require(token!=0,label+" lost pose identity");}
                in.equal_word(token,label+" descriptor pose");in.equal_word(m<2?models[m]->current_detail_level():0,label+" current LOD");in.equal_word(m<2?models[m]->previous_detail_level():0,label+" previous LOD");}
            in.equal(vertices.data(),vertices.size_bytes(),label+" transformed vertices");const auto scratch=in.value<std::array<std::byte,96>>();require(in.word()==unsigned(unknown),label+" W provenance");
            const auto actual=std::as_bytes(std::span(&frame.model_view,1));static_assert(sizeof(WorldModelViewScratch)==96);
            for(unsigned i=0;i<96;++i)if(unknown&&((i>=12&&i<16)||(i>=28&&i<32)))++in.unknown;else{require(actual[i]==scratch[i],label+" scratch byte "+std::to_string(i));++in.compared;}
            in.equal(frame.bound_matrix.data(),sizeof(frame.bound_matrix),label+" bound matrix");in.equal(arena.matrices.data(),sizeof(arena.matrices),label+" arena");
            in.equal_word(*arena.cursor,label+" arena cursor");in.equal_word(arena.count,label+" arena count");auto normalized=commands;
            for(unsigned i=0;i<commands.size();++i)normalized[i].model_identity=tokens[i];in.equal(normalized.data(),sizeof(normalized),label+" prepared commands");
            std::map<const void*,std::pair<std::uint32_t,std::uint32_t>> attr_ids;
            for(unsigned m=0;m<2;++m)for(unsigned slot=0;slot<4;++slot)for(unsigned p=0;p<parts;++p)
                attr_ids.try_emplace(std::as_bytes(models[m]->source_coordinates(models[m]->source_lod_index(slot),p)).data(),m+1,slot*parts+p+1);
            attr_ids[std::as_bytes(std::span(sprites->attributes)).data()]={3,1};attr_ids[std::as_bytes(std::span(strip->corners)).data()]={4,1};
            const std::array<WorldPolygonHeaderView,4> header_owners{WorldPolygonHeaderView(models[0]->source_attribute_table()),WorldPolygonHeaderView(models[1]->source_attribute_table()),WorldPolygonHeaderView(std::span(sprites->headers)),WorldPolygonHeaderView(std::span(strip->polygons))};
            const std::array<std::span<const std::uint16_t>,4> index_owners{models[0]->source_corner_indices(),models[1]->source_corner_indices(),sprites->indices,strip->corner_indices};
            for(const auto& pool:frame.packets)for(std::size_t i=0;i<capacity;++i){const auto& packet=pool.base().storage()[i];const auto found=attr_ids.find(packet.attributes.bytes().data());
                require(found!=attr_ids.end(),label+" lost attribute owner");const auto [owner,part]=found->second;
                const std::array<std::uint32_t,6> words{owner,static_cast<std::uint32_t>(header_owners[owner-1].index_of(packet.header)+1),pointer_token(index_owners[owner-1],packet.indices.data()),part,pointer_token(vertices,packet.vertices.data()),packet.inside_mask};
                in.equal(words.data(),sizeof(words),label+" packet identity");if(start<=i&&i<pool.count())++owner_packets[owner-1];}
            for(auto n:frame.totals)in.equal_word(n,label+" retained totals");for(const auto& g:frame.groups)for(const auto& range:g){in.equal_word(range.first.is_null()?0u:static_cast<std::uint32_t>(*range.first.offset()+1),label+" group first");in.equal_word(range.count,label+" group count");}
            for(auto* model:models)for(unsigned slot=0;slot<4;++slot)for(unsigned p=0;p<parts;++p){const auto lod=model->source_lod_index(slot);
                const std::array<std::uint16_t,3> flags{model->part(lod,p).flags,model->source_part_flags_02(lod,p),model->source_part_word_04(lod,p)};in.equal(flags.data(),sizeof(flags),label+" part flags");
                const auto coords=model->source_coordinates(lod,p);in.equal(coords.data(),coords.size_bytes(),label+" coordinates");}
            in.equal(&draws->sprite_part_transform,sizeof(ModelTransform),label+" sprite part pose");in.equal(&strip->part_transform,sizeof(VehicleStripTransform),label+" strip part pose");
            equal_sprite(in,*sprites,label);equal_strip(in,*strip,label);
        }
    }
    require(in.file.peek()==std::char_traits<char>::eof(),"background trailing data");
    require(owner_packets[0]&&owner_packets[1]&&owner_packets[2]&&owner_packets[3],"missing mixed packet owners");
    std::cout<<"{\"passed\":true,\"calls\":"<<calls<<",\"retained_calls\":"<<calls-20<<",\"models\":40,\"packets\":"<<emitted<<",\"defined_bytes\":"<<in.compared<<",\"undefined_w_bytes\":"<<in.unknown<<",\"host_checks\":"<<checks<<"}\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
