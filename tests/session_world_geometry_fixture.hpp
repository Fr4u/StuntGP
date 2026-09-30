#pragma once
#include "sgn/race_session.hpp"
#include "sgn/session_world_geometry.hpp"
#include <fstream>
#include <unordered_map>

namespace geometry_fixture {
using namespace sgn;using namespace sgn::retail;
inline void require(bool okay,const std::string& why){if(!okay)throw FormatError(why);}
inline unsigned word(const Bytes& b,std::size_t at){unsigned v;require(at+4<=b.size(),"PMD fixture bounds");std::memcpy(&v,b.data()+at,4);return v;}
struct Region {std::string name;Bytes bytes;};
struct Snapshot {
    std::vector<Region> regions;
    void add(std::string name,const void* p,std::size_t size){Bytes b(size);if(size)std::memcpy(b.data(),p,size);regions.push_back({std::move(name),std::move(b)});}
    template<class T>void value(std::string name,const T& value){add(std::move(name),&value,sizeof(value));}
    template<class T>void vector(std::string name,const std::vector<T>& value){add(std::move(name),value.data(),value.size()*sizeof(T));}
};
struct Writer {
    std::ofstream file;
    explicit Writer(const std::filesystem::path& path):file(path,std::ios::binary){require(bool(file),"Session capture file");}
    void raw(const void* p,std::size_t n){file.write(static_cast<const char*>(p),static_cast<std::streamsize>(n));require(bool(file),"Session capture write");}
    void integer(unsigned v){raw(&v,4);}
    void name(const std::string& s){integer(static_cast<unsigned>(s.size()));raw(s.data(),s.size());}
    void snapshot(const Snapshot& s){integer(static_cast<unsigned>(s.regions.size()));for(const auto& r:s.regions){name(r.name);integer(static_cast<unsigned>(r.bytes.size()));raw(r.bytes.data(),r.bytes.size());}}
};
struct Model {
    ModelResource* model;Bytes raw;std::string name;
    std::span<VehicleVector> vertices() const {
        const auto d=328+word(raw,0x4c),p=328+word(raw,0x48)+64*word(raw,d+0x48);auto* first=model->mutable_source_vertices(0,0).data()-word(raw,p+0x30);
        return {first,word(raw,0xc0)/16};
    }
    std::span<std::array<unsigned,4>> coordinates() const {
        const auto d=328+word(raw,0x4c),p=328+word(raw,0x48)+64*word(raw,d+0x48);auto* first=model->source_coordinates(0,0).data()-word(raw,p+0x2c);
        return {first,word(raw,0xbc)/16};
    }
    std::span<ModelTransform> transforms() const {
        const auto d=328+word(raw,0x4c),p=328+word(raw,0x48)+64*word(raw,d+0x48);auto* first=&model->part(0,0).transform-word(raw,p+8);
        return {first,word(raw,0xd8)/80};
    }
    void snapshot(Snapshot& out,unsigned index) const {
        const auto prefix="model-"+std::to_string(index)+"-section-";
        auto v=vertices();out.add(prefix+"0",v.data(),v.size_bytes());auto c=coordinates();out.add(prefix+"1",c.data(),c.size_bytes());
        auto h=model->source_attribute_table();out.add(prefix+"2",h.data(),h.size_bytes());auto i=model->source_corner_indices();out.add(prefix+"3",i.data(),i.size_bytes());
        auto n=model->source_normals();out.add(prefix+"4",n.data(),n.size_bytes());auto t=transforms();out.add(prefix+"5",t.data(),t.size_bytes());
        std::vector<std::uint16_t> flags;for(unsigned slot=0;slot<4;++slot){const auto lod=model->source_lod_index(slot);for(unsigned part=0;part<model->part_count(lod);++part){flags.push_back(model->part(lod,part).flags);flags.push_back(model->source_part_flags_02(lod,part));flags.push_back(model->source_part_word_04(lod,part));}}
        out.vector("model-"+std::to_string(index)+"-flags",flags);
    }
};
using PointerToken=std::array<unsigned,2>;
struct Context {
    RaceSession& session;
    std::array<Model,3> models;
    std::array<unsigned,3> identities;
    CameraMatrix rotation{};
    std::array<std::array<std::uint8_t,8>,1024> header_audit{};
    std::array<std::array<std::uint8_t,16>,3072> attribute_audit{};
    std::array<std::array<std::uint8_t,32>,3072> vertex_audit{};
    WorldPolygonClipAudit clip_audit{header_audit,attribute_audit,vertex_audit};
    explicit Context(RaceSession& value):session(value),models(make_models(value)) {
        const auto& s=session.state();identities={session.models().identity(session.scene_resources().model_handle(3)),session.models().identity(session.scene_resources().model_handle(2)),session.models().identity(s.vehicles.models[0].model)};
    }
    static std::array<Model,3> make_models(RaceSession& session){
        const std::array handles{session.scene_resources().model_handle(3),session.scene_resources().model_handle(2),session.state().vehicles.models[0].model};
        const auto make=[&](unsigned i){require(handles[i].has_value(),"actual Session model binding absent");auto& model=session.models().at(*handles[i]);std::string name(model.source_name());return Model{&model,read_file(session.shared_platform()->retail_root()/"meshdata"/(name+".PMD")),name};};
        return {make(0),make(1),make(2)};
    }
    unsigned model_token(unsigned identity) const {
        if(!identity)return 0;if(identity==sprite_model_identity)return 4;if(identity==strip_model_identity)return 5;
        for(unsigned i=0;i<3;++i)if(identity==identities[i])return i+1;throw FormatError("Session fixture queue contains a model outside its declared case");
    }
    unsigned pose_token(const ModelTransform* p) const {
        const auto& d=session.state().model_draws;if(!p)return 0;if(p==&d.template_pose)return 1;
        for(unsigned i=0;i<168;++i){if(p==&d.regular.records[i].pose)return 100+i;if(p==&d.background.records[i].pose)return 300+i;}
        throw FormatError("Session fixture lost descriptor pose identity");
    }
    Snapshot snapshot(){
        auto& s=session.state();auto& f=s.polygon_frame;auto& a=*f.allocations;const auto current=s.controls.current;auto& camera=s.controls.controllers[current].camera;auto& life=s.camera_lifecycle[current];auto normal=session.track_normal_view();Snapshot out;
        std::vector<unsigned> scalars{f.frame_stamp,f.remaining_first,f.spatial_batch_count,f.accepted_models,f.low_quality_initialized,f.lighting_stride,
            f.transformed.count,static_cast<unsigned>(f.indexed.current().offset().value_or(0xffffffffu)),f.spatial[0].count,f.spatial[1].count};
        // Existing fixture wire uses owner offsets, with an explicit NULL token
        // matching the indexed cursor. Full independent identity/base/group
        // observations belong to normal_session_frame_observer.hpp.
        for(auto& p:f.packets)scalars.push_back(static_cast<unsigned>(p.current().offset().value_or(0xffffffffu)));for(auto t:f.totals)scalars.push_back(t);
        for(auto n:{f.clip.attribute_count,f.clip.header_count,f.clip.unused_index_count,f.clip.vertex_count,s.light_vertex_offset,s.model_draws.previous_sprite_polygons,s.model_draws.regular.cursor.value_or(0xffffffffu),s.model_draws.background.cursor.value_or(0xffffffffu),s.view_quality.second,s.random.state,s.effect_particles.active_signal,normal.allocation.cursor.value_or(0xffffffffu),f.active_track==models[0].model?1u:0u})scalars.push_back(n);
        out.vector("frame",scalars);
        const std::array<unsigned,7> inputs{s.view_quality.first,s.impact_particles.rate_selector,s.split_setting,s.scene_mode,std::bit_cast<unsigned>(s.level.ground_level),std::bit_cast<unsigned>(s.gravity.inverse_factor),normal.allocation.first_generated};out.value("inputs",inputs);
        Bytes cb;const auto append=[&]<class T>(const T& x){const auto old=cb.size();cb.resize(old+sizeof(x));std::memcpy(cb.data()+old,&x,sizeof(x));};
        append(camera.position);append(camera.lens);append(life.clip_near);append(life.clip_far);append(camera.world_to_camera_reference);append(camera.camera_to_world_reference);append(s.camera_presentation[current].forward_axis);append(s.camera_presentation[current].planes);out.vector("camera",cb);
        out.value("projection",s.frame_projection);out.value("identity",s.shared_model_identity);out.value("world-view",s.frame_world_view);out.value("view-scratch",f.model_view);out.value("bound",f.bound_matrix);out.value("arena",s.view_matrices.matrices);out.value("arena-cursor",std::array{s.view_matrices.cursor.value_or(0xffffffffu),s.view_matrices.count});
        out.value("viewport",std::array{s.viewport.center_x,s.viewport.center_y,s.viewport.scale_x,s.viewport.scale_y});out.value("vertices",a.vertices);
        std::unordered_map<const void*,unsigned> vertices{{nullptr,0}},polygons{{nullptr,0}},spatial_vertices{{nullptr,0}};
        for(unsigned i=0;i<=a.vertices.size();++i)vertices.emplace(a.vertices.data()+i,i+1);const auto* spatial=models[0].model->source_spatial();require(spatial!=nullptr,"actual Session spatial owner absent");
        for(unsigned i=0;i<=spatial->polygon_indices.size();++i)polygons.emplace(spatial->polygon_indices.data()+i,i+1);for(unsigned i=0;i<=spatial->vertex_indices.size();++i)spatial_vertices.emplace(spatial->vertex_indices.data()+i,i+1);
        std::vector<unsigned> words;for(auto* v:a.indexed)words.push_back(vertices.at(v));out.vector("indexed",words);words.clear();for(auto& pool:a.spatial)for(auto& b:pool){words.push_back(polygons.at(b.polygons.data()));words.push_back(spatial_vertices.at(b.vertices.data()));}out.vector("spatial",words);
        std::unordered_map<const void*,PointerToken> headers{{nullptr,{0,0}}},indices{{nullptr,{0,0}}},attributes{{nullptr,{0,0}}};
        const auto add=[&](auto& map,unsigned owner,const auto& span){for(unsigned i=0;i<=span.size();++i)map.try_emplace(span.data()+i,PointerToken{owner,i});};
        for(unsigned m=0;m<3;++m){add(headers,m+1,models[m].model->source_attribute_table());add(indices,m+1,models[m].model->source_corner_indices());add(attributes,m+1,models[m].coordinates());}
        add(headers,4,s.sprite_packets.headers);add(indices,4,s.sprite_packets.indices);add(attributes,4,s.sprite_packets.attributes);add(headers,5,s.strip.polygons);add(indices,5,s.strip.corner_indices);add(attributes,5,s.strip.corners);
        add(headers,6,a.clip_headers);add(indices,6,f.clip.sequential_indices);add(attributes,6,a.clip_attributes);
        words.clear();for(const auto& pool:a.packets)for(const auto& p:pool){const auto h=headers.at(p.header.identity()),i=indices.at(p.indices.data()),c=attributes.at(p.attributes.bytes().data());words.insert(words.end(),{h[0],h[1],i[0],i[1],c[0],c[1],vertices.at(p.vertices.data()),p.inside_mask});}out.vector("packets",words);
        words.clear();for(auto& group:f.groups)for(auto& range:group){words.push_back(range.first.is_null()?0u:static_cast<std::uint32_t>(*range.first.offset()+1));words.push_back(range.count);}out.vector("groups",words);
        out.value("clip-headers",a.clip_headers);out.value("clip-attributes",a.clip_attributes);out.value("clip-unused",a.clip_unused_indices);out.value("clip-sequence",f.clip.sequential_indices);
        out.value("clip-header-audit",header_audit);out.value("clip-attribute-audit",attribute_audit);out.value("clip-vertex-audit",vertex_audit);
        for(unsigned m=0;m<3;++m){auto& model=*models[m].model;Bytes state(32);const std::array<unsigned,3> fields{pose_token(model.source_outer_transform()),model.current_detail_level(),model.previous_detail_level()};const std::array<std::uint16_t,2> flags{model.source_model_flags_02(),model.source_model_ambience()};std::memcpy(state.data(),fields.data(),12);std::memcpy(state.data()+12,flags.data(),4);std::memcpy(state.data()+16,&model.source_model_vector_10(),16);out.vector("model-"+std::to_string(m)+"-state",state);models[m].snapshot(out,m);}
        out.value("template",s.model_draws.template_pose);for(unsigned q=0;q<2;++q){auto queue=q==0?s.model_draws.regular.records:s.model_draws.background.records;for(auto& c:queue)c.model_identity=model_token(c.model_identity);out.value(q==0?"regular-queue":"background-queue",queue);}
        out.value("sprite-meta",std::array<unsigned,11>{pose_token(s.model_draws.sprite_outer_transform),s.model_draws.sprite_model_flags,s.model_draws.sprite_part_flags,s.model_draws.sprite_part_flags_02,s.sprite_packets.polygons,s.sprite_packets.vertices,s.sprite_packets.corners,s.sprite_packets.header_bytes,s.sprite_packets.index_count,s.sprite_packets.position_count,s.sprite_packets.attribute_count});
        out.value("strip-meta",std::array<unsigned,11>{pose_token(s.strip.outer_transform),s.strip.model_flags_02,s.strip.part_flags,s.strip.part_flags_02,s.strip.polygon_count,s.strip.corner_count,s.strip.vertex_count,s.strip.field_5712f0,s.strip.field_5712f4,s.strip.field_5712f8,s.strip.texture_id});
        out.value("sprite-pose",s.model_draws.sprite_part_transform);out.value("strip-pose",s.strip.part_transform);
        out.value("generated-0",s.sprite_packets.headers);out.value("generated-1",s.sprite_packets.indices);out.value("generated-2",s.sprite_packets.positions);out.value("generated-3",s.sprite_packets.attributes);out.value("generated-4",s.strip.polygons);out.value("generated-5",s.strip.corner_indices);out.value("generated-6",s.strip.vertices);out.value("generated-7",s.strip.corners);out.value("generated-8",s.strip.normals);
        const auto records=s.track_part_animations->records();out.add("animations",records.data(),records.size_bytes());out.value("rotation",rotation);
        std::array<std::byte,0x3400> particle_bytes;std::array<std::uint8_t,64*7> holes;for(unsigned i=0;i<64;++i){std::memcpy(particle_bytes.data()+i*0xd0,&s.effect_particles.particles[i],0xd0);for(unsigned j=0;j<7;++j)holes[i*7+j]=s.effect_particles.particles[i].source_undefined[j]?1:0;}out.value("particles",particle_bytes);out.value("particle-undefined",holes);
        out.vector("lights",s.lights.records);words={s.light_selection.active_count};for(unsigned i=0;i<16;++i)words.push_back(i<s.light_selection.ordered.size()?s.light_selection.ordered[i].value_or(0xffffffffu):0xffffffffu);out.vector("light-selection",words);
        out.vector("prepared",s.light_vertices);out.value("ambient",s.ambient_light.packed);out.value("color-gamma",s.color_tables.raster_gamma);
        return out;
    }
};
}
