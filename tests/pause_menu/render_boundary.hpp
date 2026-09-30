#pragma once
#include "bindings.hpp"
#include <functional>
#include <map>

namespace pause_menu_test {

// CPU EXTERNAL device boundary. It does not execute MaterialTextureReplacement
// or D3D12 and is not a new native-child430D40 proof. The actual GPU adapter
// receipt is separate. It owns the resulting image bytes, so
// a successful replacement cannot be represented by a notification alone.
// on_upload runs at this external boundary after copying the device payload.
// It must not resize an allocation borrowed by the currently executing child.
struct MemoryTextureDevice final:r::TextureReplacementDevice {
    struct Upload {std::uint32_t slot;sgn::Image image;};
    r::PhysicalMaterialStorage& materials;
    std::map<std::uint32_t,sgn::Image> destinations;
    std::map<std::uint32_t,std::uint32_t> primary_tokens;
    std::vector<Upload> uploads;
    std::function<void(std::size_t)> on_upload;
    explicit MemoryTextureDevice(r::PhysicalMaterialStorage& owner):materials(owner){}
    std::uint16_t destination_width(std::uint32_t slot)const override{
        if(!destinations.contains(slot)||!primary_tokens.contains(slot))throw sgn::FormatError("pause CPU texture slot is unbound");
        return materials.dimension(slot);
    }
    void upload_to_destination(std::uint32_t slot,const sgn::Image& image)override{
        const auto found=destinations.find(slot);
        if(found==destinations.end())throw sgn::FormatError("pause CPU texture slot is unbound");
        if(image.width!=materials.dimension(slot)||image.rgba.size()!=std::size_t(image.width)*image.height*4)
            throw sgn::FormatError("pause CPU texture upload extent");
        (void)materials.next_index();
        found->second=image;
        const auto event=uploads.size();uploads.push_back({slot,image});
        if(on_upload)on_upload(event);
        // Same receiving responsibility as MaterialTextureReplacement:
        //430F24..430F2B publishes the next-free primary pair after upload.
        auto& next=materials.record(materials.next_index());
        next.primary_texture=primary_tokens.at(slot);next.alternate_texture=primary_tokens.at(slot);
    }
};

// The render boundary logs each actual state call and owns every draw payload
// before allowing the next source child to overwrite its shared scratch.
// HRESULTs and external effects are explicit fixture INPUTS. The capture
// callback reads each side's own complete state; it never supplies outcomes.
struct DrawEvent {
    std::vector<std::uint32_t> request;
    std::vector<std::uint32_t> state_before;
    std::vector<r::PolygonSubmissionVertex> vertices;
    std::vector<std::uint16_t> indices;
    r::ViewportState viewport{};
    bool local_vertices{};
};
class RecordingDrawDevice final:public r::PolygonSubmissionSink {
    r::PolygonSubmissionState& scratch_;
    const r::ViewportState& viewport_;
    std::uint32_t finish(DrawEvent event){
        const auto index=events.size();
        if(capture)event.state_before=capture();
        events.push_back(std::move(event));
        if(on_call)on_call(index);
        return result;
    }
    DrawEvent draw_event(std::uint32_t kind,std::uint32_t type,std::uint32_t fvf,
        std::span<const r::PolygonSubmissionVertex> vertices,std::span<const std::uint16_t> indices,
        std::uint32_t flags,const r::ViewportState& viewport){
        if(&viewport!=&viewport_)throw sgn::FormatError("pause draw must borrow the shared viewport");
        DrawEvent event;
        event.request={kind,type,fvf,static_cast<std::uint32_t>(vertices.size()),
            static_cast<std::uint32_t>(indices.size()),flags};
        event.vertices.assign(vertices.begin(),vertices.end());
        event.indices.assign(indices.begin(),indices.end());event.viewport=viewport;
        event.local_vertices=vertices.data()!=scratch_.alpha_vertices.data();
        return event;
    }
public:
    std::uint32_t result{};
    std::vector<DrawEvent> events;
    std::function<std::vector<std::uint32_t>()> capture;
    std::function<void(std::size_t)> on_call;
    RecordingDrawDevice(r::PolygonSubmissionState& scratch,const r::ViewportState& viewport):
        scratch_(scratch),viewport_(viewport){}
    std::uint32_t set_render_state(std::uint32_t state,std::uint32_t value)override{
        DrawEvent event;event.request={1,state,value};return finish(std::move(event));
    }
    std::uint32_t set_texture(std::uint32_t stage,std::uint32_t token)override{
        DrawEvent event;event.request={2,stage,token};return finish(std::move(event));
    }
    std::uint32_t set_texture_stage_state(std::uint32_t stage,std::uint32_t state,std::uint32_t value)override{
        DrawEvent event;event.request={3,stage,state,value};return finish(std::move(event));
    }
    std::uint32_t draw(std::uint32_t type,std::uint32_t fvf,std::span<const r::PolygonSubmissionVertex> vertices,
        std::uint32_t flags,const r::ViewportState& viewport)override{
        return finish(draw_event(4,type,fvf,vertices,{},flags,viewport));
    }
    std::uint32_t draw_indexed(std::uint32_t type,std::uint32_t fvf,std::span<const r::PolygonSubmissionVertex> vertices,
        std::span<const std::uint16_t> indices,std::uint32_t flags,const r::ViewportState& viewport)override{
        return finish(draw_event(5,type,fvf,vertices,indices,flags,viewport));
    }
};
}
