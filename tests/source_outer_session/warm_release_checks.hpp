#pragma once
// Test-only observations of real owners. No game callback or private access seam.
#include "sgn/race_session.hpp"
#include "sgn/session_world_materials.hpp"
#include "sgn/material_d3d12.hpp"
#include "sgn/race_overlay_textures.hpp"
#include <set>
#include <string_view>

namespace warm_release_receiving {
using namespace sgn;
using namespace sgn::retail;
using Bytes=std::vector<unsigned char>;
inline void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>void append(Bytes& out,const T& value){
    const auto* p=reinterpret_cast<const unsigned char*>(&value);out.insert(out.end(),p,p+sizeof(value));
}
template<class Range>void elements(Bytes& out,const Range& range){
    append(out,range.size());for(const auto& value:range)append(out,value);
}
inline void text(Bytes& out,const std::string& value){elements(out,value);}
template<class F>void rejects(F action,const char* reason){
    try{action();}catch(const FormatError& e){check(std::string_view(e.what())==reason,"warm release: wrong rejection reason");return;}
    throw std::runtime_error(std::string("warm release: expected rejection: ")+reason);
}
// Same live objects before/after, including their object representations. This
// observes padding without comparing unrelated objects or restoring raw bytes.
// Heap payloads are copied below; font scratch count is public, bytes are not.
inline Bytes cpu_image(const RaceSessionState& state,const WorldMaterialLoadingState& m,
    const MaterialTransparencyStorage& transparency,const RaceOverlayTextures* overlays,
    const PolygonSubmissionState& scratch,std::optional<std::uint32_t> expected_selection=std::nullopt){
    Bytes out;append(out,m);append(out,transparency);append(out,state.ui.fonts);append(out,state.ui.selected_material);
    append(out,state.effect_resources);append(out,scratch);
    elements(out,scratch.alpha_vertices);elements(out,scratch.indexed_vertices);elements(out,scratch.indices);
    elements(out,m.upload_to_logical);elements(out,m.generated_offsets);
    append(out,m.generated_names.size());for(const auto& name:m.generated_names)text(out,name);
    for(const auto* names:{&m.names.logical_names,&m.names.unique_names}){
        append(out,names->size());for(const auto* name:*names){append(out,name);if(name)text(out,*name);}
    }
    append(out,m.names.materials.active());append(out,m.names.materials.generation());
    append(out,m.names.materials.size());append(out,m.names.materials.source_cursor());
    for(std::uint32_t slot=0;slot<255u;++slot)
        for(std::uint32_t offset=0;offset<28u;offset+=4u)append(out,m.names.materials.source_word(slot,offset));
    std::set<const MaterialImagePayload*> copied;
    for(const auto* images:{&m.images,&m.persistent_images})for(const auto& image:*images){
        const auto* payload=image.payload.get();append(out,payload);
        if(payload&&copied.insert(payload).second){append(out,payload->identity);elements(out,payload->bytes);}
    }
    append(out,state.ui.fonts.scratch_allocations());
    for(std::size_t font=0;font<2u;++font)
        for(std::size_t offset=0;offset<retail_font_record_bytes;offset+=4u)append(out,state.ui.fonts.source_word(font,offset));
    append(out,overlays);
    if(overlays){
        // An authored preimage projection can account for the later exact
        //44600F scalar store; all resource/pixel/completion checks stay present.
        append(out,expected_selection.value_or(overlays->selected_overlay_texture()));
        for(std::uint32_t slot=0;slot<255u;++slot){const auto source=overlays->completed_source(slot);
            append(out,source.has_value());if(source)append(out,*source);}
        for(std::uint32_t source=16;source<=29u;++source){const auto& image=overlays->source_image(source);
            append(out,&image);append(out,image.width);append(out,image.height);elements(out,image.rgba);}
    }
    return out;
}
inline Bytes bank_image(const PolygonGpuTextures& bank){
    Bytes out;append(out,bank.epoch);append(out,bank.entries.size());
    for(const auto& entry:bank.entries){
        check(entry.resource&&entry.recovery,"warm release: missing real GPU/pixel owner");
        append(out,entry.token);append(out,entry.resource.Get());append(out,entry.format);
        append(out,entry.source_creation.has_value());if(entry.source_creation){
            append(out,entry.source_creation->descriptor_flags);append(out,entry.source_creation->texture_stage);
            append(out,entry.source_creation->transparency_byte);
        }
        append(out,entry.recovery.get());append(out,entry.recovery->width);append(out,entry.recovery->height);
        elements(out,entry.recovery->words);
        // Read the actual retained COM resource descriptor, not a saved token.
        const auto desc=entry.resource->GetDesc();append(out,desc.Width);append(out,desc.Height);append(out,desc.Format);
        check(desc.Width==entry.recovery->width&&desc.Height==entry.recovery->height&&desc.Format==entry.format,
            "warm release: retained resource descriptor differs from pixels");
    }
    return out;
}
struct BankWitness {
    std::shared_ptr<const PolygonGpuTextures> bank;
    Bytes image;
    std::uint32_t count;
    std::vector<std::uint32_t> tokens;
    explicit BankWitness(MaterialD3D12& backend):bank(backend.bindings_snapshot()),image(bank_image(*bank)),count(backend.resource_entries()){
        check(count>0&&!bank->entries.empty(),"warm release witness starts from an empty bank");
        for(std::uint32_t slot=0;slot<count;++slot)tokens.push_back(backend.resource_primary_token(slot));
    }
    void retained() const{check(bank_image(*bank)==image,"warm release mutated retained GPU objects/pixel owners");}
    void unchanged(MaterialD3D12& backend) const{
        check(backend.resource_entries()==count&&backend.resource_epoch()==bank->epoch,"refused release changed bank count/epoch");
        const auto current=backend.bindings_snapshot();check(bank_image(*current)==image,"refused release changed current GPU bank");retained();
    }
    void released(MaterialD3D12& backend) const{
        const auto current=backend.bindings_snapshot();
        check(backend.resource_entries()==0&&current->entries.empty()&&backend.resource_epoch()==bank->epoch+1u&&current->epoch==bank->epoch+1u,
            "actual431810 did not empty the current GPU bank exactly once");
        for(std::uint32_t slot=0;slot<255u;++slot)rejects([&]{(void)backend.resource_primary_token(slot);},
            "material replacement resource slot is outside its bank");
        retained();
    }
    void aliases(const WorldMaterialLoadingState& m,bool separate) const{
        check(count==m.names.materials.size()&&m.names.physical_upload_count>0&&m.reload.font_cursor>0,
            "warm release witness lacks actual ordinary/font owners");
        std::size_t ordinary{},fonts{};
        for(const auto& entry:bank->entries){if(entry.source_creation)++ordinary;else ++fonts;}
        check(ordinary>0&&fonts>0,"warm release witness lacks actual ordinary/font GPU resources");
        const auto& record=m.names.materials.record(0);
        check(record.primary_texture!=0&&record.alternate_texture!=0&&
            ((record.primary_texture!=record.alternate_texture)==separate),"ordinary destination alias policy not exercised");
        auto find=[&](std::uint32_t token)->ID3D12Resource*{
            for(const auto& entry:bank->entries)if(entry.token==token)return entry.resource.Get();return nullptr;};
        const auto* primary=find(record.primary_texture);const auto* alternate=find(record.alternate_texture);
        check(primary&&alternate&&((primary!=alternate)==separate),"tokens do not represent expected real resource aliasing");
    }
};
}
