#pragma once
#include "normal_session_owner_wire.hpp"
#include "sgn/race_session.hpp"
#include <bit>
#include <set>

namespace normal_session_owner {
using namespace sgn;
using namespace sgn::retail;
inline constexpr std::uint32_t regular_base=0x62f5d8,background_base=0x632cf8,template_base=0x4864e8,matrix_base=0x61c7cc;
inline std::uint32_t matrix_address(std::uint32_t token){
    if(!token)return 0;if(token==1)return 0x61c468;
    require(token>=2&&(token-2)%64==0&&(token-2)/64<384,"model matrix token outside real arena");return matrix_base+token-2;
}
inline ModelTransform canonical_pose(ModelTransform value,bool part){
    for(const auto i:part?std::array{0u,1u}:std::array{1u,2u})value.tail[i]=matrix_address(value.tail[i]);return value;
}

// A union of actual checked public spans, never pointer subtraction before an
// exposed span. Each expected element must be reached; aliases must name the
// same actual object and all elements must form the original contiguous owner.
template<class T>class SpanUnion {
    std::vector<const T*> entries_;
public:
    explicit SpanUnion(std::size_t count):entries_(count){require(count<=4*1024*1024/sizeof(T),"model union bound");}
    template<std::size_t N>void include(std::span<const T,N> values,std::size_t first){
        require(first<=entries_.size()&&values.size()<=entries_.size()-first,"model span outside authored section");
        for(std::size_t i=0;i<values.size();++i){auto& entry=entries_[first+i];const auto* actual=&values[i];require(!entry||entry==actual,"aliased source span names different live objects");entry=actual;}
    }
    template<std::size_t N>void include(std::span<T,N> values,std::size_t first){include(std::span<const T>(values),first);}
    std::span<const T> complete() const {
        if(entries_.empty())return {};
        require(entries_[0]!=nullptr,"model union does not reach its first element");const auto base=reinterpret_cast<std::uintptr_t>(entries_[0]);
        for(std::size_t i=0;i<entries_.size();++i)require(entries_[i]&&reinterpret_cast<std::uintptr_t>(entries_[i])==base+i*sizeof(T),"incomplete or noncontiguous live model section");
        return {entries_[0],entries_.size()};
    }
};

struct PmdExport {
    ModelResource& model;std::uint32_t token{},base{},descriptor{},parts_base{};Bytes raw;std::string prefix;
    std::array<std::uint32_t,4> first{};std::uint32_t parts{};
    std::span<const VehicleVector> vertices,bounds;
    std::span<const std::array<std::uint32_t,4>> coordinates;
    std::span<ModelTransform> transforms;
    std::vector<std::uint32_t> strings;
    PmdExport(ModelResource& value,std::uint32_t identity,std::uint32_t address,Bytes input)
        :model(value),token(identity),base(address),raw(std::move(input)),prefix("model-"+std::to_string(identity)+"-"+std::string(value.source_name())){
        require(raw.size()>=328&&raw.size()<0x400000&&std::memcmp(raw.data(),"PMD V1.83",9)==0,"unsupported loaded PMD layout");
        descriptor=section(0x4c);parts_base=section(0x48);parts=word(raw,descriptor+0x24);
        require(descriptor<=raw.size()&&raw.size()-descriptor>=104&&parts>0&&parts<=raw.size()/64,"PMD descriptor/part shape");
        require(section_bytes(0x48)%64==0&&model.source_maximum_detail_level()==word(raw,descriptor+0x58),"PMD source detail layout mismatch");
        SpanUnion<VehicleVector> vertex_union(elements(0x2c,16)),bound_union(elements(0x3c,16));
        SpanUnion<std::array<std::uint32_t,4>> coordinate_union(elements(0x28,16));
        require(elements(0x44,80)>0,"empty model transform section");
        for(unsigned slot=0;slot<4;++slot){first[slot]=word(raw,descriptor+0x48+slot*4);
            require(first[slot]<=section_bytes(0x48)/64&&parts<=section_bytes(0x48)/64-first[slot],"nonstandard/outside 64-byte source part layout");
            const auto lod=model.source_lod_index(slot);require(model.part_count(lod)==parts,"native PMD part count differs");
            for(unsigned i=0;i<parts;++i){const auto p=parts_base+(first[slot]+i)*64;
                const auto v=model.source_vertices(lod,i);const auto c=model.source_coordinates(lod,i);
                require(v.size()==word(raw,p+0x28)&&c.size()==(word(raw,p+0x24)>>16),"live span length differs from original part");
                vertex_union.include(v,word(raw,p+0x30));coordinate_union.include(c,word(raw,p+0x2c));}}
        vertices=vertex_union.complete();coordinates=coordinate_union.complete();
        const auto p0=parts_base+first[0]*64;require(word(raw,p0+8)==0,"full transform owner lacks reachable index zero");
        transforms=model.source_transform_run(model.source_lod_index(0),0,elements(0x44,80));
        const auto indices=model.source_bound_indices();require(indices.size()==elements(0x40,4),"bound table extent differs");
        for(std::size_t i=0;i<indices.size();++i){const auto& b=model.source_bound_vertex(i);bound_union.include(std::span(&b,1),indices[i]);}
        bounds=bound_union.complete();
        const auto names=model.raw_source_texture_names();const auto table=section(0x38);const auto count=elements(0x38,4);bool terminated=false;
        for(unsigned i=0;i<count;++i){const auto at=word(raw,table+i*4);if(!at){require(i==names.size(),"native texture namespace count differs");terminated=true;break;}
            require(i<names.size(),"native texture namespace shorter than authored table");const auto offset=add(328,at);
            require(offset>=section(0x34)&&offset<section(0x34)+section_bytes(0x34),"texture string outside authored string owner");
            require(std::find(strings.begin(),strings.end(),offset)==strings.end(),"aliased authored name strings need an explicit native identity proof");strings.push_back(offset);}
        require(terminated||count==0,"missing texture namespace terminator");
        require(model.source_attribute_table().size()==elements(0x24,8)&&model.source_corner_indices().size()==elements(0x30,2)&&model.source_normals().size()==elements(0x20,16),"whole live table extent differs");
    }
    std::uint32_t section(std::uint32_t at) const {
        const auto first_byte=std::uint64_t{328}+word(raw,at);
        const auto size=word(raw,at+0x94);
        require(first_byte<=raw.size()&&size<=raw.size()-first_byte,"PMD section outside pinned file");return static_cast<std::uint32_t>(first_byte);
    }
    std::uint32_t section_bytes(std::uint32_t at) const {section(at);return word(raw,at+0x94);}
    std::uint32_t elements(std::uint32_t at,std::uint32_t stride) const {const auto n=section_bytes(at);require(n%stride==0,"PMD section stride mismatch");return n/stride;}
    std::uint32_t guest(std::uint32_t header) const{return add(base,section(header));}
    void bind(NativeAddresses& addresses) const {
        addresses.array(prefix+"-current-detail",std::span(&model.current_detail_level(),1),add(base,descriptor+0x5c));
        addresses.array(prefix+"-vertices",vertices,guest(0x2c));addresses.array(prefix+"-coordinates",coordinates,guest(0x28));
        addresses.array(prefix+"-headers",model.source_attribute_table(),guest(0x24));addresses.array(prefix+"-indices",model.source_corner_indices(),guest(0x30));
        addresses.array(prefix+"-normals",model.source_normals(),guest(0x20));addresses.array(prefix+"-transforms",transforms,guest(0x44));addresses.array(prefix+"-bounds",bounds,guest(0x3c));
        if(const auto* spatial=model.source_spatial()){
            addresses.array(prefix+"-spatial-bounds",std::span(spatial->bounds),guest(0x7c));addresses.array(prefix+"-spatial-vertices",std::span(spatial->vertex_indices),guest(0x80));
            addresses.array(prefix+"-spatial-polygons",std::span(spatial->polygon_indices),guest(0x84));addresses.array(prefix+"-spatial-nodes",std::span(spatial->nodes),guest(0x88));}
        const auto names=model.raw_source_texture_names();for(unsigned i=0;i<strings.size();++i)addresses.array(prefix+"-name-"+std::to_string(i),std::span(names[i].c_str(),names[i].size()+1),add(base,strings[i]));
    }
    template<class T,std::size_t N>void table(Image& image,std::uint32_t id,std::uint32_t header,std::span<T,N> values,std::string name) const {
        require(values.size_bytes()==section_bytes(header),"live model table byte count mismatch");
        if(!values.empty())image.write(id,section(header),values.data(),values.size_bytes(),Origin::live,prefix+"-"+name);
    }
    void write(Image& image,std::uint32_t id,const NativeAddresses& addresses) const {
        table(image,id,0x2c,vertices,"vertices");table(image,id,0x28,coordinates,"coordinates");table(image,id,0x24,model.source_attribute_table(),"headers");
        table(image,id,0x30,model.source_corner_indices(),"indices");table(image,id,0x20,model.source_normals(),"normals");table(image,id,0x3c,bounds,"bound-vectors");
        std::vector<ModelTransform> poses;poses.reserve(transforms.size());for(const auto& t:transforms)poses.push_back(canonical_pose(t,true));table(image,id,0x44,std::span(poses),"transforms-with-matrix-identities");
        const auto indices=model.source_bound_indices();for(unsigned i=0;i<indices.size();++i){const auto& b=model.source_bound_vertex(i);const auto p=addresses.address(&b,sizeof(b));
            require(p==add(guest(0x3c),std::size_t(indices[i])*16),"bound reference alias differs");image.pointer(id,section(0x40)+i*4,p,prefix+"-bound-ref-"+std::to_string(i));}
        image.live(id,descriptor+2,model.source_model_flags_02(),prefix+"-flags");image.live(id,descriptor+4,model.source_model_ambience(),prefix+"-ambience");
        image.live(id,descriptor+0x10,model.source_model_vector_10(),prefix+"-descriptor-vector");image.live(id,descriptor+0x20,model.source_model_bounds(),prefix+"-descriptor-bounds");
        image.live(id,descriptor+0x5c,model.current_detail_level(),prefix+"-current-detail");image.live(id,descriptor+0x64,model.previous_detail_level(),prefix+"-previous-detail");
        const auto outer=addresses.address(model.source_outer_transform(),model.source_outer_transform()?80:0);image.pointer(id,descriptor+12,outer,prefix+"-outer");
        if(outer)image.alias(prefix+"-outer-target",outer,80);
        for(const auto [from,to]:std::array<std::pair<unsigned,unsigned>,9>{{{0x20,0x28},{0x24,0x2c},{0x28,0x30},{0x2c,0x34},{0x30,0x38},{0x44,0x3c},{0x40,0x40},{0x48,0x44},{0x38,0x60}}})
            image.pointer(id,descriptor+to,guest(from),prefix+"-descriptor-section-"+std::to_string(to));
        std::set<unsigned> done;
        for(unsigned slot=0;slot<4;++slot){const auto lod=model.source_lod_index(slot);image.pointer(id,descriptor+0x48+slot*4,add(guest(0x48),std::size_t(first[slot])*64),prefix+"-lod-"+std::to_string(slot));
            image.alias_in(prefix+"-lod-view-"+std::to_string(slot),id,parts_base+first[slot]*64,parts*64);
            for(unsigned i=0;i<parts;++i){const auto p=parts_base+(first[slot]+i)*64;auto part=model.source_part(lod,i);const auto transform=word(raw,p+8);
                require(transform<transforms.size()&&&part.transform==&transforms[transform],"part transform lost original alias");
                const auto v=model.source_vertices(lod,i);const auto c=model.source_coordinates(lod,i);const auto h=model.source_attributes(lod,i);
                if(!v.empty())addresses.check_view(v,add(guest(0x2c),std::size_t(word(raw,p+0x30))*16));
                if(!c.empty())addresses.check_view(c,add(guest(0x28),std::size_t(word(raw,p+0x2c))*16));
                if(!h.empty())addresses.check_view(h,add(guest(0x24),std::size_t(word(raw,p+0x34))*8));
                if(!done.insert(p).second)continue;
                image.live(id,p,part.flags,prefix+"-part-flags-"+std::to_string(p));image.live(id,p+2,model.source_part_flags_02(lod,i),prefix+"-part-flags02-"+std::to_string(p));
                image.live(id,p+4,model.source_part_word_04(lod,i),prefix+"-part-word04-"+std::to_string(p));image.live(id,p+16,part.pivot,prefix+"-part-pivot-"+std::to_string(p));
                image.live(id,p+0x20,model.source_part_bounds(lod,i),prefix+"-part-bounds-"+std::to_string(p));
                image.pointer(id,p+8,add(guest(0x44),std::size_t(transform)*80),prefix+"-part-transform-"+std::to_string(p));
                for(const auto [off,header,stride]:std::array<std::array<unsigned,3>,3>{{{0x2c,0x28,16},{0x30,0x2c,16},{0x34,0x24,8}}})
                    image.pointer(id,p+off,add(guest(header),std::size_t(word(raw,p+off))*stride),prefix+"-part-section-"+std::to_string(p)+"-"+std::to_string(off));}}
        if(const auto* spatial=model.source_spatial()){
            table(image,id,0x7c,std::span(spatial->bounds),"spatial-bounds");table(image,id,0x80,std::span(spatial->vertex_indices),"spatial-vertex-indices");
            table(image,id,0x84,std::span(spatial->polygon_indices),"spatial-polygon-indices");table(image,id,0x88,std::span(spatial->nodes),"spatial-nodes");}
        const auto names=model.raw_source_texture_names();
        for(unsigned i=0;i<strings.size();++i){auto limit=section(0x34)+section_bytes(0x34);for(const auto offset:strings)if(offset>strings[i])limit=std::min(limit,offset);
            require(names[i].size()+1<=limit-strings[i],"live texture name outgrows its actual original string slot");
            image.write(id,strings[i],names[i].c_str(),names[i].size()+1,Origin::live,prefix+"-name-value-"+std::to_string(i));
            image.pointer(id,section(0x38)+i*4,add(base,strings[i]),prefix+"-name-ref-"+std::to_string(i));}
    }
};

// Generated owners have no PMD file. Their descriptor/part baseline is the
// explicit zero stores in434090 and41C530, followed by the listed source stores.
// Source-initialized bytes remain distinct from observed live values. The
// generated sprite source fields now have mutable owners. Their initial PE
// values are never substituted for the current values observed below.
inline constexpr std::uint32_t sprite_base=0x70000000,strip_base=0x71000000;
inline void bind_generated(RaceSessionState& s,NativeAddresses& addresses){
    auto& p=s.sprite_packets;auto& t=s.strip;
    addresses.array("sprite-headers",std::span(p.headers),sprite_base);addresses.array("sprite-indices",std::span(p.indices),sprite_base+0x10000);
    addresses.array("sprite-positions",std::span(p.positions),sprite_base+0x20000);addresses.array("sprite-attributes",std::span(p.attributes),sprite_base+0x30000);
    addresses.array("sprite-part-transform",std::span(&s.model_draws.sprite_part_transform,1),0x4788c0);
    auto& source=s.model_draws.sprite_source;
    addresses.array("sprite-normals",std::span(source.normals),0x4788b0);addresses.array("sprite-bound-vectors",std::span(source.bound_vertices),0x5da220);
    addresses.array("sprite-bound-table",std::span(source.bound_indices),0x4788a8,4);addresses.array("sprite-texture-word",std::span(&source.texture_namespace_word,1),0x5da230);
    addresses.array("strip-headers",std::span(t.polygons),strip_base);addresses.array("strip-indices",std::span(t.corner_indices),strip_base+0x10000);
    addresses.array("strip-positions",std::span(t.vertices),strip_base+0x20000);addresses.array("strip-attributes",std::span(t.corners),strip_base+0x30000);
    addresses.array("strip-normals",std::span(t.normals),0x570ad8);addresses.array("strip-part-transform",std::span(&t.part_transform,1),0x474820);
    addresses.array("strip-texture-names",std::span(t.texture_names),0x5712e0);
    addresses.array("strip-bound-tail",std::span(&t.retained_bound_tail,1),0x5712ec);addresses.array("strip-bound-table",std::span(&t.bound_index,1),0x5712d8,4);
}
inline void export_generated_arrays(RaceSessionState& s,Image& out){
    auto& p=s.sprite_packets;auto& t=s.strip;
    out.create_live("sprite-headers",sprite_base,std::span(p.headers));out.create_live("sprite-indices",sprite_base+0x10000,std::span(p.indices));
    out.create_live("sprite-positions",sprite_base+0x20000,std::span(p.positions));out.create_live("sprite-attributes",sprite_base+0x30000,std::span(p.attributes));
    const auto sprite_pose=canonical_pose(s.model_draws.sprite_part_transform,true);out.create_live("sprite-part-transform",0x4788c0,std::span(&sprite_pose,1));
    auto& source=s.model_draws.sprite_source;out.create_live("sprite-normals",0x4788b0,std::span(source.normals));out.create_live("sprite-bound-vectors",0x5da220,std::span(source.bound_vertices));out.create_live("sprite-texture-word",0x5da230,std::span(&source.texture_namespace_word,1));
    out.create_live("sprite-right",0x5da1e0,std::span(&p.right,1));out.create_live("sprite-up",0x5da1f0,std::span(&p.up,1));out.create_live("sprite-forward",0x5da200,std::span(&p.forward,1));
    out.create_live("sprite-previous-polygons",0x6249b0,std::span(&s.model_draws.previous_sprite_polygons,1));
    out.create_live("strip-headers",strip_base,std::span(t.polygons));out.create_live("strip-indices",strip_base+0x10000,std::span(t.corner_indices));
    out.create_live("strip-positions",strip_base+0x20000,std::span(t.vertices));out.create_live("strip-attributes",strip_base+0x30000,std::span(t.corners));out.create_live("strip-normals",0x570ad8,std::span(t.normals));
    const auto strip_pose=canonical_pose(std::bit_cast<ModelTransform>(t.part_transform),true);out.create_live("strip-part-transform",0x474820,std::span(&strip_pose,1));
    // Native source namespace entries are currently null bindings. A nonnull
    // value requires a real native name owner, not a host-token reinterpretation.
    require(std::all_of(t.texture_names.begin(),t.texture_names.end(),[](auto value){return value==0;}),"strip native texture name needs an explicit owner");
    const std::array<unsigned,4> bound_words{t.texture_names[0],t.texture_names[1],t.texture_names[2],t.retained_bound_tail};
    const auto bound_record=out.create_live("strip-bound-record",0x5712e0,std::span(bound_words));out.alias_in("strip-texture-names",bound_record,0,12);out.alias_in("strip-bound-tail",bound_record,12,4);
    out.create_live("strip-texture-id",0x5712fc,std::span(&t.texture_id,1));
    out.create_live("strip-field-f0",0x5712f0,std::span(&t.field_5712f0,1));out.create_live("strip-field-f4",0x5712f4,std::span(&t.field_5712f4,1));out.create_live("strip-field-f8",0x5712f8,std::span(&t.field_5712f8,1));
}
inline void export_generated_descriptors(RaceSessionState& s,Image& out,const NativeAddresses& addresses){
    const std::array<std::uint8_t,104> zero_descriptor{};const std::array<std::uint8_t,64> zero_part{};auto& p=s.sprite_packets;auto& t=s.strip;auto& world=s.model_draws;
    for(unsigned index=0;index<2;++index){const bool sprite=index==0;const std::string name=sprite?"sprite":"strip";const auto base=sprite?sprite_base:strip_base;const auto pose=sprite?0x4788c0u:0x474820u;
        const auto descriptor=out.create(name+"-descriptor",base+0x50000,zero_descriptor,Origin::source_initialized);
        const auto part=out.create(name+"-part",base+0x40000,zero_part,Origin::source_initialized);
        out.live(descriptor,2,sprite?world.sprite_model_flags:t.model_flags_02,name+"-model-flags");
        const auto* outer=sprite?world.sprite_outer_transform:t.outer_transform;const auto outer_address=addresses.address(outer,outer?80:0);out.pointer(descriptor,12,outer_address,name+"-outer");if(outer_address)out.alias(name+"-outer-target",outer_address,80);
        if(!sprite){out.live(descriptor,0x20,t.model_field20,"strip-model-field20");out.live(descriptor,0x22,t.model_field22,"strip-model-field22");}
        out.initialized(descriptor,0x24,std::uint32_t{1},name+"-source-one-part");
        for(const auto [offset,value]:std::array<std::pair<unsigned,unsigned>,13>{{{0x28,sprite?0x4788b0u:0x570ad8u},{0x2c,base},{0x30,base+0x30000},{0x34,base+0x20000},{0x38,base+0x10000},{0x3c,pose},{0x40,sprite?0x4788a8u:0x5712d8u},{0x44,base+0x40000},{0x48,base+0x40000},{0x4c,base+0x40000},{0x50,base+0x40000},{0x54,base+0x40000},{0x60,sprite?0x5da230u:0x5712fcu}}})
            out.pointer(descriptor,offset,value,name+"-descriptor-pointer-"+std::to_string(offset));
        out.live(part,0,sprite?world.sprite_part_flags:t.part_flags,name+"-part-flags");out.live(part,2,sprite?world.sprite_part_flags_02:t.part_flags_02,name+"-part-flags02");out.pointer(part,8,pose,name+"-part-transform-pointer");
        if(!sprite){out.live(part,0x20,t.part_field20,"strip-part-field20");out.live(part,0x22,t.part_field22,"strip-part-field22");}
        out.live(part,0x24,sprite?p.polygons:t.polygon_count,name+"-polygons");out.live(part,0x26,sprite?p.vertices:t.corner_count,name+"-source-count26");out.live(part,0x28,sprite?p.corners:t.vertex_count,name+"-source-count28");
        for(const auto [offset,value]:std::array<std::pair<unsigned,unsigned>,3>{{{0x2c,base+0x30000},{0x30,base+0x20000},{0x34,base}}})out.pointer(part,offset,value,name+"-part-pointer-"+std::to_string(offset));
        for(unsigned lod=0;lod<4;++lod)out.alias_in(name+"-lod-view-"+std::to_string(lod),part,0,64);
        const std::array<unsigned,6> roots{base,base+0x10000,base+0x20000,base+0x30000,base+0x40000,base+0x50000};out.create_live(name+"-allocation-roots",sprite?0x5da234:0x571300,std::span(roots));
        const auto model=base+0x50000;out.create_live(name+"-resource-binding",sprite?0x636418:0x636428,std::span(&model,1));
    }
    require(p.header_bytes<=4096&&p.index_count<=2048&&p.position_count<=2048&&p.attribute_count<=2048,"sprite cursor outside full owner");
    const std::array<unsigned,6> cursors{sprite_base+p.header_bytes,sprite_base+0x10000+p.index_count*2,sprite_base+0x20000+p.position_count*16,sprite_base+0x30000+p.attribute_count*16,sprite_base+0x40000,sprite_base+0x50000};
    out.create_live("sprite-cursors",0x5da24c,std::span(cursors));
    const std::array<std::uint8_t,4> zero_word{};const auto& source=world.sprite_source;
    const auto sprite_table=out.create("sprite-bound-table",0x4788a8,zero_word,Origin::source_initialized);
    const auto* sprite_bound=source.bound_indices[0]?&source.bound_vertex(0):nullptr;
    out.pointer(sprite_table,0,addresses.address(sprite_bound,sprite_bound?16:0),"sprite-actual-bound-binding");
    require(!t.bound_index||*t.bound_index==0,"strip bound index outside represented owner");
    const auto strip_table=out.create("strip-bound-table",0x5712d8,zero_word,Origin::source_initialized);
    const auto strip_bound=t.bound_index?addresses.address(t.texture_names.data(),sizeof(t.texture_names)):0;
    out.pointer(strip_table,0,strip_bound,"strip-actual-bound-binding");if(strip_bound)out.alias("strip-bound-target",strip_bound,16);
}

class ModelsExport {
    RaceSession& session_;std::vector<PmdExport> models_;
    std::uint32_t model_address(std::uint32_t identity) const {
        if(!identity)return 0;
        if(identity==sprite_model_identity)return sprite_base+0x50000;
        if(identity==strip_model_identity)return strip_base+0x50000;
        for(const auto& m:models_)if(m.token==identity)return add(m.base,m.descriptor);
        throw std::runtime_error("queue model needs the generated strip/sprite codec or an unknown imported model");
    }
public:
    std::uint32_t guest_base_for(const ModelResource* model) const {
        require(model!=nullptr,"guest PMD base requires an actual model owner");
        for(const auto& entry:models_)if(&entry.model==model)return entry.base;
        throw std::runtime_error("guest PMD base requested for a generated or foreign model");
    }
    std::uint32_t descriptor_for(const ModelResource* model) const {
        if(!model)return 0;
        for(const auto& entry:models_)if(&entry.model==model)return add(entry.base,entry.descriptor);
        throw std::runtime_error("active track is not a registered Session PMD owner");
    }
    std::uint32_t track_part_for(const SourceTrackPartPointer& publication) const {
        const auto part=publication.native_part();
        if(!part.owner)return 0;
        for(const auto& entry:models_)if(&entry.model==part.owner){
            require(part.part<entry.parts,"active track part lies outside its retained owner");
            for(unsigned slot=0;slot<4;++slot)if(entry.model.source_lod_index(slot)==part.lod)
                return add(entry.base,entry.parts_base+(entry.first[slot]+part.part)*64);
            throw std::runtime_error("active track part has no registered source LOD identity");
        }
        throw std::runtime_error("active track part is not a registered Session PMD owner");
    }
    explicit ModelsExport(RaceSession& session,const std::filesystem::path& retail_root):session_(session){
        auto& models=session.models();require(models.size()>=2&&models.size()<=32,"actual Session model registry extent");models_.reserve(models.size());
        for(std::size_t i=0;i<models.size();++i){auto& model=models.at(i);models_.emplace_back(model,models.identity(i),add(0x60000000,std::size_t(i)*0x400000),read_file(retail_root/"meshdata"/(std::string(model.source_name())+".PMD")));}
    }
    // Imported/generated model owners and actual queue/matrix aliases only;
    // the remaining normal-parent state is outside this pre-call export.
    Image snapshot(NativeAddresses* exported_addresses=nullptr,unsigned wire_version=1){
        auto& s=session_.state();NativeAddresses addresses;Image out(wire_version);std::vector<unsigned> ids;
        require(&session_.scene_resources().strip_geometry()==&s.strip,"scene registry must borrow the sole Session strip");
        for(const auto& m:models_)ids.push_back(out.create(m.prefix,m.base,m.raw,Origin::authored));
        addresses.array("regular-queue",std::span(s.model_draws.regular.records),regular_base);addresses.array("background-queue",std::span(s.model_draws.background.records),background_base);
        addresses.array("template-pose",std::span(&s.model_draws.template_pose,1),template_base);addresses.array("matrix-arena",std::span(s.view_matrices.matrices),matrix_base);
        addresses.array("shared-identity",std::span(&s.shared_model_identity,1),0x61c468);
        bind_generated(s,addresses);export_generated_arrays(s,out);
        for(const auto& m:models_)m.bind(addresses);
        const auto template_pose=canonical_pose(s.model_draws.template_pose,false);out.create_live("template-pose",template_base,std::span(&template_pose,1));
        out.create_live("matrix-arena",matrix_base,std::span(s.view_matrices.matrices));out.create_live("shared-identity",0x61c468,std::span(&s.shared_model_identity,1));
        for(unsigned q=0;q<2;++q){const auto& queue=q==0?s.model_draws.regular:s.model_draws.background;const auto base=q==0?regular_base:background_base;const std::string name=q==0?"regular":"background";
            auto copy=queue.records;for(auto& record:copy){record.model_identity=model_address(record.model_identity);record.pose=canonical_pose(record.pose,false);}
            const auto id=out.create_live(name+"-queue",base,std::span(copy));
            for(unsigned i=0;i<queue.records.size();++i){require(addresses.address(&queue.records[i].pose,80)==base+i*84+4,"queue pose identity mismatch");out.alias_in(name+"-pose-"+std::to_string(i),id,i*84+4,80);}
            require(!queue.cursor||*queue.cursor<168,"actual source queue cursor outside terminator capacity");const auto cursor=queue.cursor?base+*queue.cursor*84:0;
            out.create_live(name+"-cursor",0x4955e0+q*4,std::span(&cursor,1));}
        for(unsigned i=0;i<models_.size();++i)models_[i].write(out,ids[i],addresses);
        export_generated_descriptors(s,out,addresses);
        const auto track=session_.scene_resources().model_handle(3),sky=session_.scene_resources().model_handle(2);require(track&&sky,"actual Session track/sky absent");
        require(s.model_draws.background.records[0].model_identity==session_.models().identity(track)&&s.model_draws.background.records[1].model_identity==session_.models().identity(sky),"first background records do not name actual track and sky");
        for(unsigned i=0;i<6;++i){const auto& binding=s.vehicles.models[i];
            for(const auto [suffix,handle]:std::array<std::pair<std::string,std::optional<std::size_t>>,2>{{{"car",binding.model},{"shadow",binding.shadow}}}){
                if(!handle)continue;const auto address=model_address(session_.models().identity(handle));out.alias("physical-"+std::to_string(i)+"-"+suffix,address,104);}
            if(binding.model){require(&session_.models().at(*binding.model).current_detail_level()==&s.selected_lod[i],"body selected LOD is not the actual sole Session word");
                out.alias("selected-lod-"+std::to_string(i),addresses.address(&s.selected_lod[i],4),4);}}
        if(exported_addresses)*exported_addresses=std::move(addresses);
        return out;
    }
};
}
