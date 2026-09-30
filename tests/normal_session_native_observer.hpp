#pragma once
// Independent output-only observations. No Image/PmdExport/relocation helper is
// used here. Payloads contain original native values and full-width host
// identities, for independent decoder comparison against the mapped wire image.
#include "sgn/race_session.hpp"
#include <cstring>
#include <fstream>
#include <set>
#include <stdexcept>

namespace normal_native_observer {
using namespace sgn;
using namespace sgn::retail;
class Observations {
    struct Entry {std::string key;std::uint64_t address{};std::vector<std::uint8_t> bytes;};
    std::vector<Entry> entries_;
    static void check(bool value){if(!value)throw std::runtime_error("native observation invariant");}
    static void write(std::ostream& file,const void* data,std::size_t size){file.write(static_cast<const char*>(data),static_cast<std::streamsize>(size));check(bool(file));}
    static void u32(std::ostream& file,std::size_t value){check(value<=0xffffffffull);const auto n=static_cast<std::uint32_t>(value);write(file,&n,4);}
public:
    void bytes(std::string key,const void* data,std::size_t length){
        check(length<=64*1024*1024);Entry entry{std::move(key),length?reinterpret_cast<std::uint64_t>(data):0,{}};
        if(length){check(data!=nullptr);entry.bytes.resize(length);std::memcpy(entry.bytes.data(),data,length);}entries_.push_back(std::move(entry));
    }
    template<class T>void ref(std::string key,const T& value){static_assert(std::is_trivially_copyable_v<T>);bytes(std::move(key),&value,sizeof(value));}
    template<class T>void value(std::string key,const T& value){ref(std::move(key),value);entries_.back().address=0;}
    template<class T,std::size_t N>void span(std::string key,std::span<T,N> value){static_assert(std::is_trivially_copyable_v<T>);bytes(std::move(key),value.data(),value.size_bytes());}
    template<class T,std::size_t N>void values(std::string key,std::span<T,N> value){span(std::move(key),value);entries_.back().address=0;}
    void save(const std::filesystem::path& output)const{
        std::ofstream file(output,std::ios::binary);write(file,"SGNOBS01",8);u32(file,entries_.size());std::set<std::string> keys;
        for(const auto& e:entries_){check(!e.key.empty()&&e.key.size()<=512&&keys.insert(e.key).second);u32(file,e.key.size());write(file,e.key.data(),e.key.size());write(file,&e.address,8);u32(file,e.bytes.size());if(!e.bytes.empty())write(file,e.bytes.data(),e.bytes.size());}
        file.flush();check(bool(file));
    }
};
inline void capture(RaceSession& session,const std::filesystem::path& retail_root,const std::filesystem::path& output){
    Observations out;auto& s=session.state();
    out.span("matrix-arena",std::span(s.view_matrices.matrices));out.ref("shared-identity",s.shared_model_identity);out.ref("template-pose",s.model_draws.template_pose);
    out.span("selected-lod",std::span(s.selected_lod));
    for(unsigned i=0;i<2;++i){const auto& queue=i?s.model_draws.background:s.model_draws.regular;const std::string key=i?"background":"regular";
        out.span(key+"/records",std::span(queue.records));out.value(key+"/cursor-present",std::uint32_t(queue.cursor.has_value()));out.value(key+"/cursor-index",queue.cursor.value_or(0));}
    for(unsigned i=0;i<6;++i){const auto& binding=s.vehicles.models[i];const auto key="physical/"+std::to_string(i);
        out.value(key+"/car",session.models().identity(binding.model));out.value(key+"/shadow",session.models().identity(binding.shadow));}
    out.value("track",session.models().identity(session.scene_resources().model_handle(3)));out.value("sky",session.models().identity(session.scene_resources().model_handle(2)));
    out.value("model-count",static_cast<std::uint32_t>(session.models().size()));
    auto& sprite=s.sprite_packets;auto& world=s.model_draws;auto& strip=s.strip;
    out.value("strip/registry-headers-address",reinterpret_cast<std::uint64_t>(session.scene_resources().strip_geometry().polygons.data()));
    out.span("sprite/headers",std::span(sprite.headers));out.span("sprite/indices",std::span(sprite.indices));out.span("sprite/positions",std::span(sprite.positions));out.span("sprite/attributes",std::span(sprite.attributes));
    out.ref("sprite/part-transform",world.sprite_part_transform);out.ref("sprite/model-flags",world.sprite_model_flags);out.ref("sprite/part-flags",world.sprite_part_flags);out.ref("sprite/part-flags02",world.sprite_part_flags_02);
    out.value("sprite/outer-address",reinterpret_cast<std::uint64_t>(world.sprite_outer_transform));out.ref("sprite/previous-polygons",world.previous_sprite_polygons);
    out.ref("sprite/header-bytes",sprite.header_bytes);out.ref("sprite/index-count",sprite.index_count);out.ref("sprite/position-count",sprite.position_count);out.ref("sprite/attribute-count",sprite.attribute_count);
    out.ref("sprite/polygons",sprite.polygons);out.ref("sprite/vertices",sprite.vertices);out.ref("sprite/corners",sprite.corners);out.ref("sprite/right",sprite.right);out.ref("sprite/up",sprite.up);out.ref("sprite/forward",sprite.forward);
    auto& source=world.sprite_source;out.span("sprite/normals",std::span(source.normals));out.span("sprite/bound-vertices",std::span(source.bound_vertices));out.ref("sprite/texture-word",source.texture_namespace_word);
    out.value("sprite/bound-present",std::uint32_t(source.bound_indices[0].has_value()));out.value("sprite/bound-index",source.bound_indices[0].value_or(0));
    out.value("sprite/bound-target-address",source.bound_indices[0]?reinterpret_cast<std::uint64_t>(&source.bound_vertex(0)):std::uint64_t{0});
    out.span("strip/headers",std::span(strip.polygons));out.span("strip/indices",std::span(strip.corner_indices));out.span("strip/positions",std::span(strip.vertices));out.span("strip/attributes",std::span(strip.corners));out.span("strip/normals",std::span(strip.normals));out.span("strip/texture-names",std::span(strip.texture_names));
    out.ref("strip/part-transform",strip.part_transform);out.ref("strip/model-flags",strip.model_flags_02);out.ref("strip/part-flags",strip.part_flags);out.ref("strip/part-flags02",strip.part_flags_02);out.value("strip/outer-address",reinterpret_cast<std::uint64_t>(strip.outer_transform));
    out.ref("strip/model-field20",strip.model_field20);out.ref("strip/model-field22",strip.model_field22);out.ref("strip/part-field20",strip.part_field20);out.ref("strip/part-field22",strip.part_field22);
    out.ref("strip/polygons",strip.polygon_count);out.ref("strip/corners",strip.corner_count);out.ref("strip/vertices",strip.vertex_count);out.ref("strip/texture-id",strip.texture_id);
    out.ref("strip/field-f0",strip.field_5712f0);out.ref("strip/field-f4",strip.field_5712f4);out.ref("strip/field-f8",strip.field_5712f8);
    out.ref("strip/bound-tail",strip.retained_bound_tail);out.value("strip/bound-present",std::uint32_t(strip.bound_index.has_value()));out.value("strip/bound-index",strip.bound_index.value_or(0));
    for(std::size_t i=0;i<session.models().size();++i){auto& model=session.models().at(i);const auto key="model/"+std::to_string(i)+"/";
        out.value(key+"identity",session.models().identity(i));const auto name=model.source_name();out.bytes(key+"name",name.data(),name.size());
        out.ref(key+"flags02",model.source_model_flags_02());out.ref(key+"ambience",model.source_model_ambience());out.ref(key+"vector10",model.source_model_vector_10());out.ref(key+"bounds20",model.source_model_bounds());
        out.ref(key+"current-detail",model.current_detail_level());out.ref(key+"previous-detail",model.previous_detail_level());out.value(key+"maximum-detail",model.source_maximum_detail_level());
        out.value(key+"outer-address",reinterpret_cast<std::uint64_t>(model.source_outer_transform()));
        out.span(key+"headers",model.source_attribute_table());out.span(key+"indices",model.source_corner_indices());out.span(key+"normals",model.source_normals());out.span(key+"bound-indices",model.source_bound_indices());
        const auto bounds=model.source_bound_indices();for(std::size_t n=0;n<bounds.size();++n)out.ref(key+"bound/"+std::to_string(n),model.source_bound_vertex(n));
        const auto names=model.raw_source_texture_names();out.value(key+"name-count",static_cast<std::uint32_t>(names.size()));
        for(std::size_t n=0;n<names.size();++n)out.bytes(key+"name/"+std::to_string(n),names[n].c_str(),names[n].size()+1);
        // The count comes only from the original section extent. Public API
        // validates the requested span; no whole owner is inferred by subtracting
        // a pointer from a partial span.
        const auto raw=read_file(retail_root/"meshdata"/(std::string(model.source_name())+".PMD"));std::uint32_t transform_bytes{};
        if(raw.size()<328)throw std::runtime_error("observation PMD header");std::memcpy(&transform_bytes,raw.data()+0xd8,4);
        if(transform_bytes%80||transform_bytes>4*1024*1024)throw std::runtime_error("observation transform count");
        out.span(key+"transforms",model.source_transform_run(model.source_lod_index(0),0,transform_bytes/80));
        for(unsigned slot=0;slot<4;++slot){const auto lod=model.source_lod_index(slot);const auto lkey=key+"lod/"+std::to_string(slot)+"/";
            out.value(lkey+"index",lod);out.value(lkey+"part-count",static_cast<std::uint32_t>(model.part_count(lod)));
            for(unsigned part_index=0;part_index<model.part_count(lod);++part_index){const auto pkey=lkey+"part/"+std::to_string(part_index)+"/";const auto part=model.source_part(lod,part_index);
                out.ref(pkey+"flags",part.flags);out.ref(pkey+"flags02",model.source_part_flags_02(lod,part_index));out.ref(pkey+"word04",model.source_part_word_04(lod,part_index));
                out.ref(pkey+"pivot",part.pivot);out.ref(pkey+"bounds20",model.source_part_bounds(lod,part_index));out.ref(pkey+"transform",part.transform);
                out.span(pkey+"vertices",model.source_vertices(lod,part_index));out.span(pkey+"coordinates",model.source_coordinates(lod,part_index));out.span(pkey+"headers",model.source_attributes(lod,part_index));}}
        if(const auto* spatial=model.source_spatial()){
            out.span(key+"spatial/bounds",std::span(spatial->bounds));out.span(key+"spatial/vertices",std::span(spatial->vertex_indices));out.span(key+"spatial/polygons",std::span(spatial->polygon_indices));out.span(key+"spatial/nodes",std::span(spatial->nodes));}
    }
    out.save(output);
}
}
