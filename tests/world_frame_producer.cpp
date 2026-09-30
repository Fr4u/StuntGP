#include "sgn/retail_world_frame_producer.hpp"
#include "sgn/retail_track_part_initialization.hpp"
#include "sgn/retail_vehicle_strip_draw.hpp"
#include "sgn/retail_time_trial.hpp"
#include <fstream>
#include <iostream>
#include <map>
#include <unordered_map>

using namespace sgn;using namespace sgn::retail;
namespace {
void require(bool v,const std::string& message){if(!v)throw FormatError(message);}
unsigned raw_word(const Bytes& b,std::size_t at){unsigned n;require(at+4<=b.size(),"raw PMD offset");std::memcpy(&n,b.data()+at,4);return n;}
struct Reader {
    std::ifstream file;std::uint64_t exact{},unknown{};std::string context;
    explicit Reader(const char* path):file(path,std::ios::binary){require(bool(file),"missing frame corpus");}
    void bytes(void* p,std::size_t n){require(n<16*1024*1024,"oversized frame record");file.read(static_cast<char*>(p),static_cast<std::streamsize>(n));require(bool(file),"truncated frame corpus");}
    template<class T>T value(){T v{};bytes(&v,sizeof(v));return v;}
    unsigned word(){return value<unsigned>();}
    std::string name(){auto n=word();require(n<256,"frame label size");std::string s(n,'\0');bytes(s.data(),n);return s;}
    void equal(std::string_view label,const void* data,std::size_t n){
        require(name()==label,context+" expected section "+std::string(label));require(word()==n,context+" section size "+std::string(label));
        std::vector<unsigned char> b(n);bytes(b.data(),n);const auto count=word();require(count<=n,"mask size");std::vector<bool> mask(n);
        for(unsigned k=0;k<count;++k){auto at=word();require(at<n&&!mask[at],"duplicate/outside undefined byte");
            const auto t=at%80,p=at%0xd0;
            const bool allowed=(label=="view-scratch"&&((at>=12&&at<16)||(at>=28&&at<32)))||
                (label=="model-0-section-4"&&at%16>=12)||(label=="model-0-section-5"&&((t>=44&&t<48)||(t>=60&&t<64)))||
                (label=="particles"&&((p>=0x64&&p<0x68)||(p>=0x90&&p<0x98)||(p>=0xc4&&p<0xd0)));
            require(allowed,context+" unreviewed undefined region "+std::string(label));mask[at]=true;}
        const auto* actual=static_cast<const unsigned char*>(data);
        for(std::size_t i=0;i<n;++i)if(mask[i])++unknown;else{if(actual[i]!=b[i])throw FormatError(context+" "+std::string(label)+" byte "+std::to_string(i)+" native="+std::to_string(actual[i])+" source="+std::to_string(b[i]));++exact;}
    }
    template<class T>void equal(std::string_view label,const T& value){equal(label,&value,sizeof(value));}
};
struct ModelOwners {
    ModelResource& model;Bytes raw;
    void compare(Reader& in,const std::string& prefix){
        const auto lod=model.source_lod_index(0),d=328+raw_word(raw,0x4c),p=328+raw_word(raw,0x48)+64*raw_word(raw,d+0x48);
        auto* vertices=model.mutable_source_vertices(lod,0).data()-raw_word(raw,p+0x30);
        auto* coordinates=model.source_coordinates(lod,0).data()-raw_word(raw,p+0x2c);
        auto* transforms=&model.part(lod,0).transform-raw_word(raw,p+8);
        in.equal(prefix+"0",vertices,raw_word(raw,0xc0));in.equal(prefix+"1",coordinates,raw_word(raw,0xbc));
        in.equal(prefix+"2",model.source_attribute_table().data(),raw_word(raw,0xb8));in.equal(prefix+"3",model.source_corner_indices().data(),raw_word(raw,0xc4));
        in.equal(prefix+"4",model.source_normals().data(),raw_word(raw,0xb4));in.equal(prefix+"5",transforms,raw_word(raw,0xd8));
    }
};
template<class T>unsigned token(std::span<T> owner,const void* p){
    for(std::size_t i=0;i<=owner.size();++i)if(owner.data()+i==p)return static_cast<unsigned>(i+1);
    throw FormatError("unrelated frame pointer owner");
}
std::vector<std::span<std::byte>> generated(SourceSpritePacketStorage& s,VehicleStripGeometry& t){
    return {std::as_writable_bytes(std::span(s.headers)),std::as_writable_bytes(std::span(s.indices)),std::as_writable_bytes(std::span(s.positions)),std::as_writable_bytes(std::span(s.attributes)),
        std::as_writable_bytes(std::span(t.polygons)),std::as_writable_bytes(std::span(t.corner_indices)),std::as_writable_bytes(std::span(t.vertices)),std::as_writable_bytes(std::span(t.corners)),std::as_writable_bytes(std::span(t.normals))};
}
unsigned pose_token(const ModelTransform* pose,const ModelTransform& prior,const WorldModelDrawState& draws){
    if(!pose)return 0;if(pose==&prior)return 1;if(pose==&draws.template_pose)return 2;
    for(unsigned i=0;i<168;++i){if(pose==&draws.regular.records[i].pose)return 100+i;if(pose==&draws.background.records[i].pose)return 300+i;}
    throw FormatError("lost frame pose owner");
}
}
int main(int argc,char** argv)try{
    require(argc==3,"expected retail root and frame corpus");Reader in(argv[2]);std::array<char,8> magic;in.bytes(magic.data(),8);const bool placed_generated=magic==std::array{'S','G','N','F','R','M','0','2'};require(placed_generated||magic==std::array{'S','G','N','F','R','M','0','1'},"frame schema");
    const auto groups=in.word();require(groups>=1&&groups<=30,"frame group count");AssetStore assets(argv[1]);ModelResources resources(assets,argv[1]);std::uint64_t calls{},emitted{},checks{};std::array<std::uint64_t,5> owner_packets{};
    for(unsigned group=0;group<groups;++group){
        std::array<std::string,3> names;std::array<unsigned,3> ids;std::array<ModelResource*,3> models;std::array<unsigned,3> parts;
        for(unsigned m=0;m<3;++m){names[m]=in.name();ids[m]=resources.load(names[m],m==0&&names[m].ends_with("m")?"trackset00":"");models[m]=&resources.at(*resources.resolve(ids[m]));parts[m]=static_cast<unsigned>(models[m]->part_count(0));models[m]->current_detail_level()=0;models[m]->previous_detail_level()=0x12340000u+m;}
        auto& track=*models[0];const auto raw_path=std::filesystem::path(argv[1])/"meshdata";
        std::array<ModelOwners,3> owners{{{track,read_file(raw_path/(names[0]+".PMD"))},{*models[1],read_file(raw_path/(names[1]+".PMD"))},{*models[2],read_file(raw_path/(names[2]+".PMD"))}}};
        in.context=names[0]+" initial";std::array<TrackPartAnimation,32> animations{};TrackPartPreparationScratch preparation;
        initialize_track_part_resources(1,&track,owners[0].raw,animations,preparation);TrackNormalAllocation normal;bind_track_normal_allocation(owners[0].raw,track.source_normals(),normal);
        owners[0].compare(in,"initialized-");in.equal("initialized-animation",animations);
        auto draws=std::make_unique<WorldModelDrawState>();auto sprites=std::make_unique<SourceSpritePacketStorage>();auto strip=std::make_unique<VehicleStripGeometry>();strip->part_flags=1;
        auto generated_owners=generated(*sprites,*strip);for(auto bytes:generated_owners)in.bytes(bytes.data(),bytes.size());
        const auto nc=in.word();normal.cursor=nc==0xffffffffu?std::nullopt:std::optional(nc);VehicleRandom random;random.state=in.word();WorldGravity gravity;gravity.inverse_factor=in.value<float>();
        contact::effects::EffectParticleBank particles;for(auto& p:particles.particles)in.bytes(&p,0xd0);particles.active_signal=in.word();
        draws->template_pose=in.value<ModelTransform>();auto prior=in.value<ModelTransform>();auto rotation=in.value<CameraMatrix>();WorldPolygonFrame frame;allocate_world_polygon_frame(frame);
        frame.model_view=in.value<WorldModelViewScratch>();frame.bound_matrix=in.value<CameraMatrix>();auto world_view=in.value<CameraMatrix>();ViewMatrixArena arena;in.bytes(arena.matrices.data(),sizeof(arena.matrices));arena.cursor=64;arena.count=1;
        const auto identity=vehicle_identity();allocate_outer_model_matrices(prior,arena);track.source_outer_transform()=&prior;
        auto& allocation=*frame.allocations;in.bytes(allocation.vertices.data(),sizeof(allocation.vertices));
        allocation.indexed.fill(allocation.vertices.data());const auto* spatial=track.source_spatial();require(spatial,"track spatial owner");
        for(auto& pool:allocation.spatial)for(auto& b:pool)b={spatial->polygon_indices,spatial->vertex_indices};
        const auto first=world_polygon_part_source(track,0,0);for(unsigned p=0;p<4;++p)for(auto& packet:allocation.packets[p])packet={WorldPolygonHeaderView(track.source_attribute_table()).at(0),track.source_corner_indices(),first.attributes,allocation.vertices,0x87654321u+p};
        PmdLightBank lights;lights.records.resize(16);in.bytes(lights.records.data(),16*sizeof(RetailLightRecord));std::array<std::uint8_t,0x4800> prepared;in.bytes(prepared.data(),prepared.size());LightVertexCursor light_cursor{prepared,137};
        ColorTables colors;in.bytes(colors.raster_gamma.data(),sizeof(colors.raster_gamma));frame.lighting_stride=0xabcdef12;frame.low_quality_initialized=0;const auto ground=in.value<float>();
        seed_world_model_draws(*draws,resources,*resources.resolve(ids[0]),*resources.resolve(ids[1]),*sprites,ground,1,arena);
        std::map<const void*,std::pair<unsigned,unsigned>> attr_ids;
        for(unsigned m=0;m<3;++m)for(unsigned slot=0;slot<4;++slot)for(unsigned p=0;p<parts[m];++p)attr_ids.try_emplace(std::as_bytes(models[m]->source_coordinates(models[m]->source_lod_index(slot),p)).data(),m+1,slot*parts[m]+p+1);
        attr_ids[std::as_bytes(std::span(sprites->attributes)).data()]={4,1};attr_ids[std::as_bytes(std::span(strip->corners)).data()]={5,1};
        std::array<WorldPolygonHeaderView,5> header_owners{WorldPolygonHeaderView(track.source_attribute_table()),WorldPolygonHeaderView(models[1]->source_attribute_table()),WorldPolygonHeaderView(models[2]->source_attribute_table()),WorldPolygonHeaderView(std::span(sprites->headers)),WorldPolygonHeaderView(std::span(strip->polygons))};
        std::array<std::span<const std::uint16_t>,5> index_owners{track.source_corner_indices(),models[1]->source_corner_indices(),models[2]->source_corner_indices(),sprites->indices,strip->corner_indices};
        std::unordered_map<const void*,unsigned> vertex_tokens;for(unsigned i=0;i<=allocation.vertices.size();++i)vertex_tokens.emplace(allocation.vertices.data()+i,i+1);
        std::array<std::unordered_map<const void*,unsigned>,5> index_tokens;for(unsigned m=0;m<5;++m)for(unsigned i=0;i<=index_owners[m].size();++i)index_tokens[m].emplace(index_owners[m].data()+i,i+1);
        std::array<std::unordered_map<const void*,unsigned>,5> header_tokens;for(unsigned m=0;m<5;++m)for(unsigned i=0;i<header_owners[m].size();++i)header_tokens[m].emplace(header_owners[m].at(i).identity(),i+1);
        for(unsigned step=0;step<8;++step,++calls){
            in.context=names[0]+" step "+std::to_string(step);const auto rate=in.word(),lighting=in.word();auto quality=in.word();const auto split_setting=in.word();frame.frame_stamp=in.word();
            CameraState camera;const auto position=in.value<Vec3>();camera.position={position.x,position.y,position.z,0};camera.world_to_camera_reference=2;camera.camera_to_world_reference=2;
            CameraPresentationState presentation;presentation.planes=in.value<std::array<CameraVector,4>>();presentation.forward_axis=in.value<CameraVector>();CameraLifecycle lifecycle;lifecycle.clip_near=in.value<float>();lifecycle.clip_far=in.value<float>();camera.lens=in.value<float>();const auto projection=in.value<ProjectionParameters>();
            LightViewSelection selection;selection.active_count=in.word();for(auto id:in.value<std::array<unsigned,16>>())selection.ordered.emplace_back(id);RetailAmbientLight ambient;ambient.packed=in.value<std::array<unsigned,3>>();
            arena.cursor=64;arena.count=1;for(auto* model:models)for(unsigned slot=0;slot<=model->source_maximum_detail_level();++slot)allocate_model_part_matrices(*model,slot,arena);
            for(unsigned i=0;i<3;++i)allocate_outer_model_matrices(draws->background.records[i].pose,arena);
            CameraState sprite_camera;sprite_camera.camera_to_world_rotation=arena.matrices[0];CameraLifecycle sprite_lifecycle;sprite_lifecycle.clip_far=100000;
            if(placed_generated)sprite_camera.position=camera.position;
            CameraPresentationState sprite_presentation;sprite_presentation.forward_axis={0,0,1,0};ControllerOverlayTransition overlay{};sprites->forward={0,0,1,0};
            const auto sprite_count=in.word();for(unsigned i=0;i<sprite_count;++i){const auto sprite=in.value<SourceSpriteRecord>();require(append_sprite_packet(*sprites,sprite,{sprite_camera,sprite_lifecycle,sprite_presentation,overlay}),"real sprite append failed");}
            const auto strip_count=in.word();require(strip_count<=128,"strip count");strip->polygon_count=static_cast<std::uint16_t>(strip_count);strip->corner_count=strip->vertex_count=static_cast<std::uint16_t>(strip_count*4);strip->field_5712f8=strip_count;
            const auto saved_template=draws->template_pose;if(placed_generated)draws->template_pose=in.value<ModelTransform>();
            append_vehicle_strip_draw(*strip,rate,draws->template_pose,draws->background,arena);
            if(placed_generated)draws->template_pose=saved_template;
            for(unsigned j=0;j<2;++j){auto pose=in.value<ModelTransform>();append_model_draw(j==0?draws->background:draws->regular,*models[2],models[2]->current_detail_level(),ids[2],pose,0,arena);}
            for(unsigned i=0;i<parts[0];++i){animations[i].phase_u=in.word();animations[i].phase_v=in.word();}
            frame.transformed.count=0;frame.indexed.publish_current(frame.allocations->indexed_lifetime.pointer(23));frame.spatial_batch_count=0xdeadbeef;frame.remaining_first=31;frame.accepted_models=0xdeadbeef;for(unsigned i=0;i<2;++i)frame.spatial[i].count=17+i;
            require(in.word()==pose_token(track.source_outer_transform(),prior,*draws),in.context+" prior track descriptor0C");++checks;
            produce_world_frame({resources,*resources.resolve(ids[0]),*resources.resolve(ids[1]),*draws,*sprites,*strip,camera,presentation,lifecycle,arena,identity,projection,frame,world_view,animations,{track.source_normals(),normal},random,particles,gravity,
                {lights,selection,light_cursor,ambient,colors,split_setting,frame.frame_stamp,frame.lighting_stride},lighting,quality,rate,ground,1},{&rotation,nullptr});
            require(frame.active_track==&track,in.context+" active track owner");++checks;
            std::vector<unsigned> scalars{frame.transformed.count,frame.indexed.count(),frame.spatial[0].count,frame.spatial[1].count,frame.spatial_batch_count,frame.remaining_first,frame.accepted_models,frame.low_quality_initialized,frame.lighting_stride,quality,std::bit_cast<unsigned>(lifecycle.clip_far),*arena.cursor,arena.count,light_cursor.offset,draws->previous_sprite_polygons,*draws->regular.cursor,*draws->background.cursor,random.state,normal.cursor?*normal.cursor:0xffffffffu,particles.active_signal};
            for(auto& p:frame.packets){scalars.push_back(p.count());emitted+=p.count();}
            in.equal("scalars",scalars.data(),scalars.size()*4);in.equal("world-view",world_view);in.equal("view-scratch",frame.model_view);in.equal("bound",frame.bound_matrix);in.equal("arena",arena.matrices);in.equal("transformed",allocation.vertices);
            std::vector<unsigned> words;words.reserve(12288);for(auto* v:allocation.indexed)words.push_back(vertex_tokens.at(v)-1);in.equal("indexed",words.data(),words.size()*4);
            words.clear();for(const auto& pool:allocation.spatial)for(const auto& b:pool){words.push_back(token(std::span(spatial->polygon_indices),b.polygons.data())-1);words.push_back(token(std::span(spatial->vertex_indices),b.vertices.data())-1);}in.equal("spatial",words.data(),words.size()*4);
            words.clear();for(const auto& pool:frame.packets)for(unsigned i=0;i<pool.base().storage().size();++i){const auto& p=pool.base().storage()[i];const auto found=attr_ids.find(p.attributes.bytes().data());require(found!=attr_ids.end(),in.context+" attributes owner");const auto [owner,part]=found->second;
                const std::array<unsigned,6> record{owner,header_tokens[owner-1].at(p.header.identity()),index_tokens[owner-1].at(p.indices.data()),part,vertex_tokens.at(p.vertices.data()),p.inside_mask};words.insert(words.end(),record.begin(),record.end());if(i<pool.count())++owner_packets[owner-1];}
            in.equal("packets",words.data(),words.size()*4);words.clear();for(const auto& g:frame.groups)for(const auto& range:g){words.push_back(range.first.is_null()?0u:static_cast<std::uint32_t>(*range.first.offset()+1));words.push_back(range.count);}words.insert(words.end(),frame.totals.begin(),frame.totals.end());in.equal("groups-totals",words.data(),words.size()*4);
            for(unsigned m=0;m<3;++m){auto& model=*models[m];const std::string prefix="model-"+std::to_string(m);std::array<std::byte,16> binding{};
                const std::array<unsigned,3> fields{pose_token(model.source_outer_transform(),prior,*draws),model.current_detail_level(),model.previous_detail_level()};const std::array<std::uint16_t,2> flags{model.source_model_flags_02(),model.source_model_ambience()};
                std::memcpy(binding.data(),fields.data(),12);std::memcpy(binding.data()+12,flags.data(),4);in.equal(prefix+"-binding",binding);owners[m].compare(in,prefix+"-section-");
                std::vector<std::uint16_t> part_flags;for(unsigned slot=0;slot<4;++slot)for(unsigned p=0;p<parts[m];++p){const auto lod=model.source_lod_index(slot);part_flags.push_back(model.part(lod,p).flags);part_flags.push_back(model.source_part_flags_02(lod,p));part_flags.push_back(model.source_part_word_04(lod,p));}in.equal(prefix+"-flags",part_flags.data(),part_flags.size()*2);
            }
            in.equal("animations",animations);in.equal("rotation",rotation);std::array<std::byte,0x3400> bank;for(unsigned i=0;i<64;++i)std::memcpy(bank.data()+i*0xd0,&particles.particles[i],0xd0);in.equal("particles",bank);in.equal("template",draws->template_pose);
            for(unsigned q=0;q<2;++q){auto normalized=q==0?draws->regular.records:draws->background.records;for(auto& command:normalized){if(!command.model_identity)continue;unsigned id=command.model_identity==sprite_model_identity?4:command.model_identity==strip_model_identity?5:0;for(unsigned m=0;m<3;++m)if(command.model_identity==ids[m])id=m+1;require(id!=0,"unrelated queue model");command.model_identity=id;}in.equal(q==0?"regular-queue":"background-queue",normalized);}
            for(unsigned g=0;g<2;++g){std::array<std::byte,16> binding;const auto id=pose_token(g==0?draws->sprite_outer_transform:strip->outer_transform,prior,*draws);std::memcpy(binding.data(),&id,4);
                const std::array<std::uint16_t,6> fields=g==0?std::array<std::uint16_t,6>{draws->sprite_model_flags,draws->sprite_part_flags,draws->sprite_part_flags_02,sprites->polygons,sprites->vertices,sprites->corners}:std::array<std::uint16_t,6>{strip->model_flags_02,strip->part_flags,strip->part_flags_02,strip->polygon_count,strip->corner_count,strip->vertex_count};std::memcpy(binding.data()+4,fields.data(),12);in.equal(g==0?"sprite-binding":"strip-binding",binding);}
            in.equal("sprite-pose",draws->sprite_part_transform);in.equal("strip-pose",strip->part_transform);for(unsigned i=0;i<generated_owners.size();++i)in.equal("generated-"+std::to_string(i),generated_owners[i].data(),generated_owners[i].size());
            in.equal("lights",lights.records.data(),16*sizeof(RetailLightRecord));in.equal("prepared",prepared);
        }
        std::cout<<"checked "<<names[0]<<" calls "<<calls<<'\n';
    }
    require(in.file.peek()==std::char_traits<char>::eof(),"trailing frame corpus");
    std::cout<<"{\"passed\":true,\"groups\":"<<groups<<",\"calls\":"<<calls<<",\"retained_calls\":"<<calls-groups<<",\"packets\":"<<emitted<<",\"defined_bytes\":"<<in.exact<<",\"undefined_bytes\":"<<in.unknown<<",\"host_checks\":"<<checks<<"}\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
