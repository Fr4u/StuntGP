#include "sgn/polygon_device_context.hpp"
#include <limits>
#include <algorithm>
#include <new>
#include <utility>

namespace sgn {
struct PolygonDeviceContext::Impl {
    struct Capture {
        PolygonDeviceSnapshot initial;
        PolygonCommandCapture commands;
        PolygonDeviceCaptureLimits limits;
        std::vector<PolygonDeviceCall> events;
        bool poisoned{};
        std::uint64_t refused{};
        std::uint64_t unlogged{};
        std::optional<PolygonDeviceCall> first_refusal,last_refusal;
        Capture(PolygonDeviceSnapshot s,std::uint64_t tick,std::uint64_t epoch,PolygonDeviceCaptureLimits l):
            initial(std::move(s)),commands(tick,epoch,l.polygon),limits(l){
            if(l.events==0)throw FormatError("device capture requires an event budget");
            events.reserve(l.events);
        }
    };
    PolygonDeviceIdentityRef identity=std::make_shared<const PolygonDeviceIdentity>();
    PolygonDeviceSnapshot state;
    std::optional<PolygonDeviceCall> last;
    std::unique_ptr<Capture> capture;
    Impl(PolygonDeviceState polygon,PolygonDeviceLightState light):state{std::move(polygon),std::move(light),false,0}{}
    bool event_space() const noexcept{return !capture||capture->events.size()<capture->limits.events;}
    bool command_space(std::size_t bytes) const noexcept {
        if(!capture)return true;
        const auto used=capture->commands.payload_bytes();
        return event_space()&&capture->commands.command_count()<capture->limits.polygon.commands&&
            used<=capture->limits.polygon.payload_bytes&&bytes<=capture->limits.polygon.payload_bytes-used;
    }
    std::uint32_t finish(PolygonDeviceOperation op,std::array<std::uint32_t,3> args,std::uint32_t hr,
        PolygonDeviceRejection why=PolygonDeviceRejection::none,std::optional<std::size_t> command={},
        std::optional<PolygonDeviceTransform> transform={}) noexcept {
        PolygonDeviceCall call{++state.operation_sequence,op,args,hr,hr==ok,why,command,std::move(transform)};last=call;
        if(capture){
            if(hr!=ok){capture->poisoned=true;++capture->refused;if(!capture->first_refusal)capture->first_refusal=call;capture->last_refusal=call;}
            if(event_space())capture->events.push_back(call); // Capacity was reserved before capture publication.
            else ++capture->unlogged;
        }
        return hr;
    }
    template<class Record,class Apply>
    std::uint32_t command(PolygonDeviceOperation op,std::array<std::uint32_t,3> args,std::size_t bytes,Record record,Apply apply){
        if(!command_space(bytes))return finish(op,args,out_of_memory,PolygonDeviceRejection::capacity);
        std::optional<std::size_t> index;
        if(capture){
            index=capture->commands.command_count();
            try{record(capture->commands);}
            catch(const std::bad_alloc&){return finish(op,args,out_of_memory,PolygonDeviceRejection::allocation);}
            catch(const FormatError&){return finish(op,args,invalid_argument,PolygonDeviceRejection::draw_layout);}
        }
        apply();return finish(op,args,ok,PolygonDeviceRejection::none,index);
    }
    std::uint32_t draw(PolygonDeviceOperation op,std::uint32_t type,std::uint32_t fvf,
        std::span<const retail::PolygonSubmissionVertex> vertices,std::span<const std::uint16_t> indices,
        std::uint32_t flags,const retail::ViewportState& viewport,bool indexed){
        const std::array args{type,fvf,flags};
        if(!capture)return finish(op,args,invalid_order,PolygonDeviceRejection::missing_capture);
        if(!state.scene_open)return finish(op,args,invalid_order,PolygonDeviceRejection::scene_order);
        //432F60 records nonindexed fans with0..8 corners, including empty
        //and degenerate calls. Preserve that CPU payload without triangulation.
        //431830 emits8 vertices;431F10 emits2. Retain both qualified line
        //payloads. Other type2 extents remain outside the source subset.
        const bool layout=indexed
            ?type==4u&&vertices.size()>=3&&indices.size()>=3&&indices.size()%3==0
            :(type==5u&&vertices.size()>=3)||(type==6u&&vertices.size()<=8)||
             (type==2u&&(vertices.size()==2||vertices.size()==8));
        if(fvf!=0x2c4||!layout)
            return finish(op,args,invalid_argument,PolygonDeviceRejection::draw_layout);
        for(auto i:indices)if(i>=vertices.size())return finish(op,args,invalid_argument,PolygonDeviceRejection::draw_index);
        constexpr auto maximum=(std::numeric_limits<std::size_t>::max)();
        if(vertices.size()>maximum/sizeof(retail::PolygonSubmissionVertex)||indices.size()>maximum/sizeof(std::uint16_t))
            return finish(op,args,out_of_memory,PolygonDeviceRejection::capacity);
        const auto vertex_bytes=vertices.size_bytes(),index_bytes=indices.size_bytes();
        if(vertex_bytes>maximum-index_bytes)return finish(op,args,out_of_memory,PolygonDeviceRejection::capacity);
        // NEW R19: the loop above proved every index < vertices.size(); the
        // capture skips only that identical predicate (all other guards stay).
        return command(op,args,vertex_bytes+index_bytes,[&](PolygonCommandCapture& c){
            c.append_prevalidated_draw(type,fvf,vertices,indexed?indices:std::span<const std::uint16_t>{},indexed,flags,viewport);
        },[]{});
    }
};
PolygonDeviceContext::PolygonDeviceContext(PolygonDeviceState state,PolygonDeviceLightState light):impl_(std::make_unique<Impl>(std::move(state),std::move(light))){}
PolygonDeviceContext::~PolygonDeviceContext()=default;
PolygonDeviceIdentityRef PolygonDeviceContext::identity() const noexcept{return impl_->identity;}
PolygonDeviceSnapshot PolygonDeviceContext::snapshot() const noexcept{return impl_->state;}
std::optional<PolygonDeviceCall> PolygonDeviceContext::last_call() const noexcept{return impl_->last;}
bool PolygonDeviceContext::capturing() const noexcept{return bool(impl_->capture);}
std::optional<PolygonDeviceCaptureStatus> PolygonDeviceContext::capture_status() const noexcept{
    if(!impl_->capture)return {};
    const auto& c=*impl_->capture;
    return PolygonDeviceCaptureStatus{c.poisoned,c.commands.command_count(),c.commands.payload_bytes(),c.events.size(),c.refused,c.unlogged,c.first_refusal,c.last_refusal};
}
void PolygonDeviceContext::begin_capture(std::uint64_t tick,std::uint64_t epoch,PolygonDeviceCaptureLimits limits){
    if(impl_->capture)throw FormatError("device capture is already active");
    auto next=std::make_unique<Impl::Capture>(impl_->state,tick,epoch,limits);impl_->capture=std::move(next);
}
ImmutablePolygonDeviceFrame PolygonDeviceContext::seal_capture(){
    if(!impl_->capture)throw FormatError("device capture is not active");
    if(impl_->capture->poisoned)throw FormatError("device capture contains refused CPU calls; inspect and discard it before recovery");
    auto frame=std::make_shared<CapturedPolygonDeviceFrame>();auto& c=*impl_->capture;
    frame->device=impl_->identity;frame->initial=c.initial;frame->final=impl_->state;
    // Allocate the wrapper first. If the inner seal allocation fails, capture
    // remains active and its accepted prefix is available for another attempt.
    frame->polygon_commands=c.commands.seal();frame->events=std::move(c.events);
    impl_->capture.reset();return frame;
}
void PolygonDeviceContext::insert_gpu_command(std::shared_ptr<const PolygonGpuCommand> command,std::optional<PolygonGpuDrawRequest> draw){
    // NEW R19 host GPU insertion: no source event, sequence or HRESULT.
    if(!impl_->capture||!impl_->state.scene_open)throw FormatError("GPU insertion requires an active capture inside an open scene");
    impl_->capture->commands.insert_gpu_command(std::move(command),std::move(draw));
}
void PolygonDeviceContext::discard_capture(){
    if(!impl_->capture)throw FormatError("device capture is not active");
    impl_->capture.reset();
}
std::uint32_t PolygonDeviceContext::begin_scene(){
    using Op=PolygonDeviceOperation;auto& p=*impl_;
    if(p.state.scene_open)return p.finish(Op::begin_scene,{},invalid_order,PolygonDeviceRejection::scene_order);
    if(!p.event_space())return p.finish(Op::begin_scene,{},out_of_memory,PolygonDeviceRejection::capacity);
    p.state.scene_open=true;return p.finish(Op::begin_scene,{},ok);
}
std::uint32_t PolygonDeviceContext::end_scene(){
    using Op=PolygonDeviceOperation;auto& p=*impl_;
    if(!p.state.scene_open)return p.finish(Op::end_scene,{},invalid_order,PolygonDeviceRejection::scene_order);
    if(!p.event_space())return p.finish(Op::end_scene,{},out_of_memory,PolygonDeviceRejection::capacity);
    p.state.scene_open=false;return p.finish(Op::end_scene,{},ok);
}
std::uint32_t PolygonDeviceContext::set_render_state(std::uint32_t state,std::uint32_t value){
    const auto op=PolygonDeviceOperation::render_state;const std::array args{state,value,0u};auto& p=*impl_;
    if(state>=p.state.polygon.render.size())return p.finish(op,args,invalid_argument,PolygonDeviceRejection::selector);
    return p.command(op,args,0,[&](PolygonCommandCapture& c){c.set_render_state(state,value);},[&]{p.state.polygon.render[state]=value;});
}
std::uint32_t PolygonDeviceContext::set_light_state(std::uint32_t state,std::uint32_t value){
    const auto op=PolygonDeviceOperation::light_state;const std::array args{state,value,0u};auto& p=*impl_;
    if(state>=p.state.light.size())return p.finish(op,args,invalid_argument,PolygonDeviceRejection::selector);
    if(!p.event_space())return p.finish(op,args,out_of_memory,PolygonDeviceRejection::capacity);
    p.state.light[state]=value;return p.finish(op,args,ok);
}
std::uint32_t PolygonDeviceContext::set_transform_represented(std::uint32_t slot,std::span<const std::uint32_t,16> words){
    const auto op=PolygonDeviceOperation::transform;const std::array args{slot,1u,0u};auto& p=*impl_;
    if(slot<1||slot>3)return p.finish(op,args,invalid_argument,PolygonDeviceRejection::selector);
    if(!p.event_space())return p.finish(op,args,out_of_memory,PolygonDeviceRejection::capacity);
    std::array<std::uint32_t,16> coefficients;std::copy(words.begin(),words.end(),coefficients.begin());
    PolygonDeviceTransform value{coefficients};p.state.transforms[slot-1]=value;
    return p.finish(op,args,ok,PolygonDeviceRejection::none,{},value);
}
std::uint32_t PolygonDeviceContext::set_transform_uninitialized_source_stack(std::uint32_t slot){
    const auto op=PolygonDeviceOperation::transform;const std::array args{slot,2u,0x431420u};auto& p=*impl_;
    if(slot!=3)return p.finish(op,args,invalid_argument,PolygonDeviceRejection::selector);
    if(!p.event_space())return p.finish(op,args,out_of_memory,PolygonDeviceRejection::capacity);
    PolygonDeviceTransform value{PolygonSourceUninitializedTransform{}};p.state.transforms[slot-1]=value;
    return p.finish(op,args,ok,PolygonDeviceRejection::none,{},value);
}
std::uint32_t PolygonDeviceContext::set_texture(std::uint32_t stage,std::uint32_t token){
    const auto op=PolygonDeviceOperation::texture;const std::array args{stage,token,0u};auto& p=*impl_;
    if(stage>=p.state.polygon.texture.size())return p.finish(op,args,invalid_argument,PolygonDeviceRejection::selector);
    return p.command(op,args,0,[&](PolygonCommandCapture& c){c.set_texture(stage,token);},[&]{p.state.polygon.texture[stage]=token;});
}
std::uint32_t PolygonDeviceContext::set_texture_stage_state(std::uint32_t stage,std::uint32_t state,std::uint32_t value){
    const auto op=PolygonDeviceOperation::texture_stage_state;const std::array args{stage,state,value};auto& p=*impl_;
    if(stage>=p.state.polygon.stage.size()||state>=p.state.polygon.stage[stage].size())return p.finish(op,args,invalid_argument,PolygonDeviceRejection::selector);
    return p.command(op,args,0,[&](PolygonCommandCapture& c){c.set_texture_stage_state(stage,state,value);},[&]{p.state.polygon.stage[stage][state]=value;});
}
std::uint32_t PolygonDeviceContext::draw_indexed(std::uint32_t type,std::uint32_t fvf,std::span<const retail::PolygonSubmissionVertex> vertices,
    std::span<const std::uint16_t> indices,std::uint32_t flags,const retail::ViewportState& viewport){
    return impl_->draw(PolygonDeviceOperation::draw_indexed,type,fvf,vertices,indices,flags,viewport,true);
}
std::uint32_t PolygonDeviceContext::draw(std::uint32_t type,std::uint32_t fvf,std::span<const retail::PolygonSubmissionVertex> vertices,
    std::uint32_t flags,const retail::ViewportState& viewport){return impl_->draw(PolygonDeviceOperation::draw,type,fvf,vertices,{},flags,viewport,false);}
std::uint32_t PolygonDeviceSurface::set_color_key(std::uint32_t flags,std::span<const std::uint32_t,2> values){
    key_=PolygonDeviceSurfaceKey{flags,{values[0],values[1]}};return PolygonDeviceContext::ok;
}
}
