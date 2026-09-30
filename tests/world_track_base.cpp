#include "sgn/retail_world_track_base.hpp"
#include "sgn/retail_track_part_initialization.hpp"
#include <fstream>
#include <iostream>

namespace {
using namespace sgn;using namespace sgn::retail;
void require(bool value,const std::string& why){if(!value)throw FormatError(why);}
unsigned word(std::span<const std::uint8_t> b,std::size_t at){unsigned v;require(at+4<=b.size(),"PMD test metadata");std::memcpy(&v,b.data()+at,4);return v;}
class Reader {
    std::ifstream file_;
public:
    std::uint64_t exact{},undefined{};
    explicit Reader(const char* path):file_(path,std::ios::binary){require(bool(file_),"missing base-track corpus");}
    void bytes(void* p,std::size_t n){require(n<32*1024*1024,"oversized base-track record");file_.read(static_cast<char*>(p),static_cast<std::streamsize>(n));require(bool(file_),"truncated base-track corpus");}
    template<class T>T value(){T v;bytes(&v,sizeof(v));return v;}
    unsigned integer(){return value<unsigned>();}
    template<class T>std::vector<T> array(std::size_t n){require(n<1024*1024,"oversized base-track array");std::vector<T> v(n);bytes(v.data(),n*sizeof(T));return v;}
    template<class T>std::vector<T> array(){return array<T>(integer());}
    std::string name(){const auto n=integer();require(n&&n<128,"base-track name");std::string s(n,'\0');bytes(s.data(),n);return s;}
    void equal(const void* p,std::size_t n,const std::string& label,int kind=-1){
        const auto expected=array<std::uint8_t>(n);auto ignored=array<unsigned>();std::vector<bool> mask(n);
        for(auto at:ignored){require(at<n&&!mask[at],label+" duplicate/outside undefined byte");
            const auto o=at%80,particle=at%0xd0;
            const bool permitted=(kind==4&&at%16>=12)||(kind==5&&((o>=44&&o<48)||(o>=60&&o<64)))||
                (kind==6&&((particle>=0x64&&particle<0x68)||(particle>=0x90&&particle<0x98)||(particle>=0xc4&&particle<0xd0)));
            require(permitted,label+" unreviewed source-undefined region");mask[at]=true;}
        const auto* actual=static_cast<const std::uint8_t*>(p);
        for(std::size_t i=0;i<n;++i)if(mask[i])++undefined;else {if(actual[i]!=expected[i])throw FormatError(label+" byte "+std::to_string(i)+" native "+std::to_string(actual[i])+" source "+std::to_string(expected[i]));++exact;}
    }
    void end(){require(file_.peek()==std::char_traits<char>::eof(),"trailing base-track corpus");}
};
struct ModelOwners {
    ModelResource& model;const Bytes& raw;
    void compare(Reader& in,const std::string& label){
        const auto lod=model.source_lod_index(0),d=328+word(raw,0x4c),p=328+word(raw,0x48)+64*word(raw,d+0x48);
        auto* vertices=model.mutable_source_vertices(lod,0).data()-word(raw,p+0x30);
        auto* coordinates=model.source_coordinates(lod,0).data()-word(raw,p+0x2c);
        auto* transforms=&model.part(lod,0).transform-word(raw,p+8);
        in.equal(vertices,word(raw,0xc0),label+" source vertices",0);
        in.equal(coordinates,word(raw,0xbc),label+" source coordinates",1);
        in.equal(model.source_attribute_table().data(),word(raw,0xb8),label+" source headers",2);
        in.equal(model.source_corner_indices().data(),word(raw,0xc4),label+" source corners",3);
        in.equal(model.source_normals().data(),word(raw,0xb4),label+" source normals",4);
        in.equal(transforms,word(raw,0xd8),label+" source transforms",5);
    }
};
}
int main(int argc,char** argv)try{
    require(argc==3||(argc==4&&std::string_view(argv[3])=="--stale-projection-near"),"expected retail root, base-track corpus and optional stale-near control");Reader in(argv[2]);std::array<char,8> magic;in.bytes(magic.data(),8);require(magic==std::array{'S','G','N','T','B','A','0','1'},"base-track schema");
    const auto groups=in.integer();require(groups==30,"all30 source tracks required");AssetStore assets(argv[1]);ModelResources resources(assets,argv[1]);
    std::uint64_t calls{},packets{},indexed_appends{},batches{};
    for(unsigned group=0;group<groups;++group){
        const auto name=in.name();auto& model=resources.at(*resources.resolve(resources.load(name,name.ends_with("m")?"trackset00":"")));
        const auto raw=read_file(std::filesystem::path(argv[1])/"meshdata"/(name+".PMD"));const auto lod=model.source_lod_index(0),parts=static_cast<unsigned>(model.part_count(lod));
        std::array<TrackPartAnimation,32> animations{};TrackPartPreparationScratch preparation;initialize_track_part_resources(1,&model,raw,animations,preparation);
        TrackNormalAllocation normal;bind_track_normal_allocation(raw,model.source_normals(),normal);
        ModelOwners owners{model,raw};owners.compare(in,name+" initialized");in.equal(animations.data(),sizeof(animations),name+" initialized records");
        VehicleRandom random;random.state=in.integer();WorldGravity gravity;gravity.inverse_factor=in.value<float>();
        contact::effects::EffectParticleBank particles;for(auto& p:particles.particles)in.bytes(&p,0xd0);particles.active_signal=in.integer();
        auto template_pose=in.value<ModelTransform>();CameraMatrix rotation=in.value<CameraMatrix>();const auto cursor=in.integer();normal.cursor=cursor==0xffffffffu?std::nullopt:std::optional<unsigned>{cursor};
        WorldPolygonFrame frame;frame.model_view=in.value<WorldModelViewScratch>();frame.bound_matrix=in.value<CameraMatrix>();
        auto vertices=in.array<WorldPolygonVertex>();const auto begin=in.integer(),capacity=in.integer(),spatial_capacity=in.integer(),indexed_capacity=in.integer();
        require(begin<vertices.size()&&capacity<32768&&spatial_capacity==512&&indexed_capacity==32768,"base-track fixture allocations");
        std::vector<WorldPolygonVertex*> indices(indexed_capacity,vertices.data());WorldIndexedVertexStorageLifetime lifetime(indices);frame.indexed.publish_base(lifetime.pointer());frame.indexed.copy_base_to_current();frame.transformed={vertices,begin};
        const auto source=world_polygon_part_source(model,lod,0);const auto* spatial=model.source_spatial();require(spatial!=nullptr,"actual spatial owner");
        std::array<std::vector<WorldPolygonPacket>,4> pools;std::array<std::unique_ptr<WorldPolygonPacketStorageLifetime>,4> packet_lifetimes;std::array<std::vector<WorldSpatialBatch>,2> spatial_pools;
        for(unsigned p=0;p<4;++p){pools[p].resize(capacity);for(auto& record:pools[p])record={WorldPolygonHeaderView(model.source_attribute_table()).at(0),model.source_corner_indices(),source.attributes,std::span(vertices).subspan(begin),in.integer()};packet_lifetimes[p]=std::make_unique<WorldPolygonPacketStorageLifetime>(pools[p]);frame.packets[p].publish_base(packet_lifetimes[p]->pointer());frame.packets[p].copy_base_to_current();}
        for(unsigned p=0;p<2;++p){spatial_pools[p].assign(spatial_capacity,{spatial->polygon_indices,spatial->vertex_indices});frame.spatial[p]={spatial_pools[p],0};}
        ModelTransform outer;model.source_outer_transform()=&outer;
        const auto steps=in.integer();require(steps==8,"eight retained full base-track calls required");
        for(unsigned step=0;step<steps;++step,++calls){
            const auto stamp=in.integer(),rate=in.integer(),start=in.integer();frame.frame_stamp=stamp;frame.transformed.count=begin;
            for(unsigned i=0;i<parts;++i){animations[i].phase_u=in.integer();animations[i].phase_v=in.integer();}
            outer=in.value<ModelTransform>();const auto position=in.value<Vec3>();CameraState camera;camera.position={position.x,position.y,position.z,0};camera.world_to_camera_reference=2;
            CameraPresentationState presentation;presentation.planes=in.value<std::array<CameraVector,4>>();
            CameraLifecycle lifecycle;lifecycle.clip_near=in.value<float>();lifecycle.clip_far=in.value<float>();auto projection=in.value<ProjectionParameters>();
            if(argc==4)projection.clip_near=std::bit_cast<float>(0x7e800000u);
            ViewMatrixArena arena;in.bytes(arena.matrices.data(),sizeof(arena.matrices));arena.cursor=64;arena.count=1;const auto identity=vehicle_identity();
            allocate_outer_model_matrices(outer,arena);allocate_model_part_matrices(model,0,arena);
            for(unsigned p=0;p<4;++p)frame.packets[p].publish_current(packet_lifetimes[p]->pointer(start));
            frame.spatial_batch_count=0xdeadbeef;frame.spatial[0].count=17;frame.spatial[1].count=19;frame.indexed.publish_current(lifetime.pointer(23));
            frame.accepted_models=0x11223344;frame.low_quality_initialized=0x55667788;frame.remaining_first=31;
            produce_world_track_base(model,camera,presentation,lifecycle,arena,identity,projection,frame,animations,{model.source_normals(),normal},
                {random,particles,gravity,rate,template_pose},{&rotation,nullptr});
            const auto label=name+" step "+std::to_string(step);
            std::vector<unsigned> scalars{frame.transformed.count,frame.indexed.count(),frame.spatial[0].count,frame.spatial[1].count,frame.spatial_batch_count,
                std::bit_cast<unsigned>(lifecycle.clip_far),frame.active_track==&model?1u:0u,frame.accepted_models,frame.low_quality_initialized,frame.remaining_first};
            for(auto& p:frame.packets){scalars.push_back(p.count());packets+=p.count()-start;}
            in.equal(scalars.data(),scalars.size()*4,label+" source cursors");indexed_appends+=frame.indexed.count();batches+=frame.spatial_batch_count;
            in.equal(vertices.data(),vertices.size()*32,label+" transformed owner");
            in.equal(&frame.model_view,sizeof(frame.model_view),label+" retained view scratch");in.equal(frame.bound_matrix.data(),64,label+" untouched bounds scratch");
            in.equal(arena.matrices.data(),sizeof(arena.matrices),label+" original-produced matrices");const std::array<unsigned,2> arena_words{*arena.cursor,arena.count};in.equal(arena_words.data(),8,label+" arena cursor");
            std::vector<unsigned> pointer_words;pointer_words.reserve(indexed_capacity);for(auto* v:indices)pointer_words.push_back(static_cast<unsigned>(v-vertices.data()));
            in.equal(pointer_words.data(),pointer_words.size()*4,label+" indexed owner");
            pointer_words.clear();for(auto& pool:spatial_pools)for(auto& b:pool){pointer_words.push_back(static_cast<unsigned>(b.polygons.data()-spatial->polygon_indices.data()));pointer_words.push_back(static_cast<unsigned>(b.vertices.data()-spatial->vertex_indices.data()));}
            in.equal(pointer_words.data(),pointer_words.size()*4,label+" retained spatial owner");
            pointer_words.clear();for(auto& pool:pools)for(auto& p:pool){require(p.attributes.bytes().data()==source.attributes.bytes().data(),label+" borrowed coordinate owner");
                pointer_words.push_back(static_cast<unsigned>(WorldPolygonHeaderView(model.source_attribute_table()).index_of(p.header)));pointer_words.push_back(static_cast<unsigned>(p.indices.data()-model.source_corner_indices().data()));
                pointer_words.push_back(1);pointer_words.push_back(static_cast<unsigned>(p.vertices.data()-vertices.data()));pointer_words.push_back(p.inside_mask);}
            in.equal(pointer_words.data(),pointer_words.size()*4,label+" retained packet owners");
            owners.compare(in,label);in.equal(animations.data(),sizeof(animations),label+" animation records");in.equal(rotation.data(),64,label+" animation matrix scratch");
            in.equal(&template_pose,80,label+" live template");std::array<std::byte,0x3400> particle_bytes;for(unsigned i=0;i<64;++i)std::memcpy(particle_bytes.data()+i*0xd0,&particles.particles[i],0xd0);
            in.equal(particle_bytes.data(),particle_bytes.size(),label+" retained particles",6);
            const std::array<unsigned,3> effect_words{particles.active_signal,random.state,normal.cursor?*normal.cursor:0xffffffffu};in.equal(effect_words.data(),12,label+" effect owners");
            require(model.source_outer_transform()==&outer,label+" descriptor outer owner");
        }
    }
    in.end();std::cout<<"{\"passed\":true,\"calls\":"<<calls<<",\"tracks\":"<<groups<<",\"packets\":"<<packets<<",\"indexed_appends\":"<<indexed_appends<<",\"batches\":"<<batches<<",\"exact_defined_bytes\":"<<in.exact<<",\"source_undefined_bytes\":"<<in.undefined<<"}\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
