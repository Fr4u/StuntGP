// Seventeen bounded actual Session/capture/release groups. Execution by Main only.
// The surface failure is an explicit OS/renderer boundary, not full draw success.
#include "fixture_com.hpp"
#include "capture_probe.hpp"
#include "warm_release_checks.hpp"
#include "../source_warm_allocation_witness.hpp"
#include "../source_warm_environment_witness.hpp"
#include "../source_warm_continuation_witness.hpp"
#include <thread>
#include <exception>
#include "sgn/source_outer_scheduler.hpp"
#include "sgn/source_input_cold_init.hpp"
#include "sgn/menu_directinput_startup.hpp"
#include "sgn/menu_keyboard_acquisition.hpp"
#include "sgn/race_session.hpp"
#include "sgn/race_session_pause_restart.hpp"
#include "sgn/race_session_world_tick.hpp"
#include "sgn/session_outer_source_bindings.hpp"
#include "sgn/session_world_materials.hpp"
#include "sgn/material_d3d12.hpp"
#include "sgn/renderer.hpp"
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <string_view>

namespace {
using namespace sgn;
using namespace sgn::retail;
using namespace source_outer_receiving;
using fixture::require;
constexpr auto surface_error="source432A60 surface operation failed; legacy error termination is unsupported";
constexpr auto cadence_error="outer source frame duplicated, reentered or poisoned";
constexpr auto capture_error="source outer capture is already active or poisoned";
constexpr auto session_error="source outer binding has another session/world/platform";
constexpr auto input_error="window input and Session must share the same world, control block and HWND";
constexpr auto phase_error="DirectInput acquisition requested outside the initialized source lifetime";
constexpr PolygonSubmissionCaptureLimits limits{{{20000,32u*1024u*1024u},30000},16};
using Bytes=std::vector<unsigned char>;
template<class T>void append(Bytes& bytes,const T& value){
    const auto* first=reinterpret_cast<const unsigned char*>(&value);bytes.insert(bytes.end(),first,first+sizeof(value));
}
// Same object before/after only. Includes inline owners and scratch payloads;
// does not claim recursively hashing every resource heap or padding semantics.
Bytes image_of(const RaceSessionState& s,const FrontendState& f,const PolygonSubmissionState& scratch){
    Bytes b;append(b,s);append(b,f.source);append(b,scratch);
    for(const auto& v:scratch.alpha_vertices)append(b,v);
    for(const auto& v:scratch.indexed_vertices)append(b,v);
    for(const auto& v:scratch.indices)append(b,v);
    return b;
}
template<class F>void rejects(F action,const char* expected){
    try{action();}catch(const FormatError& e){
        require(std::string_view(e.what())==expected,"wrong rejection type/reason");return;
    }
    throw std::runtime_error(std::string("expected rejection: ")+expected);
}
struct Window {
    HWND handle{};
    Window(HINSTANCE instance,const wchar_t* type):handle(CreateWindowExW(0,type,L"StuntGP NEW source outer receiving",
        WS_OVERLAPPEDWINDOW,0,0,1280,720,nullptr,nullptr,instance,nullptr)){require(handle!=nullptr,"real hidden HWND");}
    ~Window(){if(handle)DestroyWindow(handle);}
    Window(const Window&)=delete;Window& operator=(const Window&)=delete;
};
RaceSessionLaunch launch(HWND window,bool spatial=false){
    RaceSessionLaunch result;result.mode=SessionMode::normal_race;result.master_gain=0;
    result.sound_percent=spatial?100u:0u;result.music_percent=0;result.window=reinterpret_cast<std::uint64_t>(window);return result;
}
struct Counter final:FrontendPerformanceCounter {
    std::uint64_t value{100};unsigned reads{},frequencies{};
    warm_release_receiving::ResetContinuationWitness* warm_reset{};
    std::uint64_t counter() override{++reads;return warm_reset?warm_reset->counter(reads,value):value;}
    std::uint64_t frequency() override{if(warm_reset)throw std::runtime_error("warm reset queried frequency");++frequencies;return 600;}
};
struct SurfaceFailure final:LoadingFrameDevice {
    unsigned calls{};
    std::uint32_t surface_operation_432a60(std::uint32_t a,std::uint32_t b) override{
        require(a==0&&b==1,"actual432A60 arguments");++calls;return 1;
    }
    std::uint32_t begin_scene() override{throw std::runtime_error("unexpected external BeginScene");}
    std::uint32_t end_scene() override{throw std::runtime_error("unexpected external EndScene");}
    std::uint32_t finalize_423430(std::uint32_t) override{throw std::runtime_error("unexpected surface finalize");}
};
// Unavailable child guard, never a successful zero/empty game implementation.
// Every case must stop before any getter. Real full frame requires Camera's
// actual Session outer resources and a successful qualified surface receiver.
struct UnavailableDrawResources final:SessionOuterFrameSources {
    unsigned calls{};
    [[noreturn]] void unavailable(){++calls;throw std::runtime_error("full draw resource child was unexpectedly reached");}
    SessionWorldFrameResources borrow_world_resources() override{unavailable();}
    RaceHudSubmission borrow_countdown() override{unavailable();}
    PauseAgeFadeWorld borrow_age_fade() override{unavailable();}
    LoadingFrameWorld borrow_loading_frame() override{unavailable();}
    DemoTailWords borrow_demo_words() override{unavailable();}
};
struct StageObservation {unsigned before{},after{};};
void before_stage(void* p,std::uint32_t) noexcept{++static_cast<StageObservation*>(p)->before;}
void after_stage(void* p,std::uint32_t) noexcept{++static_cast<StageObservation*>(p)->after;}
WorldMaterialLoadingContext loading_for(const RaceSession& session,const SourceGraphicsRoots& roots){
    WorldMaterialLoadingContext c;c.source_graphics_roots=&roots;c.scene=session.state().scene_mode;
    const auto& p=session.state().level.track_texture;const auto end=std::find(p.begin(),p.end(),'\0');
    require(end!=p.end(),"actual track texture prefix");c.track_prefix={p.begin(),end};
    c.quality=0;c.maximum_dimension=4096;return c; // Explicit cold receiving profile, not full42E6E0.
}
struct Run {
    RaceSession& session;FrontendState& frontend;SessionWindowInput& input;
    SessionWorldMaterials& materials;MaterialTransparencyStorage& transparency;
    PolygonSubmissionState& scratch;PolygonDeviceContext& device;MaterialD3D12& backend;
    SessionOuterCapture& capture;SurfaceFailure& surfaces;UnavailableDrawResources sources;
    SessionPauseRestartSource restart;Counter counter;SourceOuterTrace trace;
    StageObservation stages;SessionWorldTickAudit audit;
    std::uint32_t depth_capability{1}; // Explicit modern receiver capability, not recovered initializer.
    Run(RaceSession& s,FrontendState& f,SessionWindowInput& i,SessionWorldMaterials& m,MaterialTransparencyStorage& t,
        PolygonSubmissionState& p,PolygonDeviceContext& d,
        MaterialD3D12& b,SessionOuterCapture& c,SurfaceFailure& surf):
        session(s),frontend(f),input(i),materials(m),transparency(t),scratch(p),device(d),backend(b),capture(c),surfaces(surf),restart(s){
        audit.context=&stages;audit.before_stage=before_stage;audit.after_stage=after_stage;
    }
    SessionOuterCaptureRequest request(std::uint64_t serial=71){
        auto& s=session.state();return {restart,sources,session_outer_device_words(s),
            session_outer_polygon_capabilities(s,SelectedDeviceDepthBorrow(depth_capability)),serial,0xabcdef,limits};
    }
    SourceRaceOuterContext context(){return {session,frontend,counter,input,nullptr,trace,&audit};}
    void seed(){
        auto& s=session.state();require(s.scene_mode==1&&s.hud_block==0,"actual local non-demo race");
        require(s.controls.controllers[0].active==1,"actual primary controller");
        require(s.source_scheduler.fixed_mode_a==0&&s.source_scheduler.fixed_mode_b==0,"actual variable clock initializers");
        initialize_session_source_clock_434840(session,counter);
        require(counter.reads==2&&counter.frequencies==1&&s.tick_clock.increment==10,"actual434840 clock initialization");
        counter.reads=0;counter.frequencies=0;
        // Explicit bounded receiving stimuli in the real owners. No source
        // initialization or transition is claimed for these seed writes.
        s.simulation_tick=41;s.options.selection_phase=s.scene_mode;s.frame_pause=0;s.controls.current=0;
        auto& camera=s.controls.controllers[0].camera;
        camera.drift={11,12,13,71};camera.angular_acceleration={21,22,23,72};
        s.audio_level.output_b=8;s.audio_level.output_b_copy=20;
        auto keys=menu_keyboard_bytes(s.startup);std::fill(keys.begin(),keys.end(),std::uint8_t{1});
        require(std::all_of(s.frontend_launch.input_bindings.begin(),s.frontend_launch.input_bindings.end(),
            [](const auto& binding){return binding.vehicle==0xffff;}),"actual45EC30 unbound physical slots");
    }
    void entered(bool iteration,bool draw){
        std::vector<SourceOuterBoundary> expected{SourceOuterBoundary::numeric,SourceOuterBoundary::camera_reset,
            SourceOuterBoundary::input_pair,SourceOuterBoundary::sample_clock};
        if(iteration)expected.push_back(SourceOuterBoundary::entered_iteration);
        expected.push_back(SourceOuterBoundary::audio_pair);expected.push_back(SourceOuterBoundary::scene_transition);
        if(draw)expected.push_back(SourceOuterBoundary::draw);
        require(trace.entered==expected,"actual outer call order/count");
    }
    void updated(std::uint32_t tick,std::uint32_t scheduled,bool sound_reset=false){
        auto& s=session.state();require(s.simulation_tick==tick&&s.tick_clock.scheduled==scheduled,"actual source tick/schedule");
        require(counter.reads==(sound_reset?3u:1u)&&counter.frequencies==0&&keyboard_calls==1,"source counter order and once-only input acquisition");
        const auto& c=s.controls.controllers[0].camera;
        require(c.drift.x==0&&c.drift.y==0&&c.drift.z==0&&c.drift.w==71&&
            c.angular_acceleration.x==0&&c.angular_acceleration.y==0&&c.angular_acceleration.z==0&&c.angular_acceleration.w==72,
            "actual4023F0 xyz reset and retained W");
        const auto keys=menu_keyboard_bytes(s.startup);
        require(std::all_of(keys.begin(),keys.end(),[](auto x){return x==0;}),"actual current keyboard publication");
        const auto* prior=reinterpret_cast<const unsigned char*>(s.startup.fields_62dcf0.data());
        require(std::all_of(prior,prior+256,[](auto x){return x==1;}),"actual previous keyboard publication");
        // Actual462280 honors its initialized guard. Silent receiving may use
        // that legitimate early return; no fake audio service is installed.
        require(s.audio_level.output_b==(sound_reset?255u:(s.audio_driver.initialized?12u:8u)),"actual audio tail source branch");
        require(stages.before==0&&stages.after==0&&sources.calls==0,"unexpected world stage or unresolved draw child");
    }
    template<class F>void reject_before_mutation(F action,const char* reason){
        const auto before=image_of(session.state(),frontend,scratch);const auto d=device.snapshot();
        const auto entries=trace.entered;const auto missing=trace.missing_scene_operations;
        const auto reads=counter.reads,freq=counter.frequencies,keys=keyboard_calls,surface=surfaces.calls;
        const auto source_calls=sources.calls;const auto probes=capture_begin_count;
        const auto epoch=backend.resource_epoch();const auto calls=fixture::active->log.calls;
        rejects(action,reason);
        require(image_of(session.state(),frontend,scratch)==before&&same_polygon_snapshot(d,device.snapshot()),"admission wrote Session/frontend/scratch/device");
        require(trace.entered==entries&&trace.missing_scene_operations==missing&&counter.reads==reads&&counter.frequencies==freq&&
            keyboard_calls==keys&&surfaces.calls==surface&&sources.calls==source_calls&&capture_begin_count==probes&&
            backend.resource_epoch()==epoch&&fixture::active->log.calls==calls,"admission entered source or OS/capture/resource work");
    }
    void warm_stop(std::uint64_t serial=71,bool disable_spatial=false){
        auto& s=session.state();s.options.selection_phase=2;s.overlays[0].active=0;s.normal_finish.menu_requested=1;
        const warm_release_receiving::BankWitness bank(backend);
        const warm_release_receiving::AllocationWitness allocations(*session.shared_platform());
        s.outer.render_counter=0x81234567u; //receiving canary: observe reset versus earlier audio boundary
        const warm_release_receiving::EnvironmentWitness environment(session,disable_spatial);
        warm_release_receiving::ResetContinuationWitness continuation(session,frontend,allocations,environment.sound_reset_completed());
        counter.warm_reset=&continuation;
        const auto retained=warm_release_receiving::cpu_image(s,materials.state(),transparency,&capture.textures(),scratch,environment.expected_selected_overlay());
        const auto device_before=device.snapshot();const auto probes=capture_begin_count;
        const auto renders=s.outer.render_counter;auto ctx=context();const auto out=capture.advance_and_capture_source_frame(ctx,request(serial));
        bank.released(backend);continuation.finished();counter.warm_reset=nullptr;
        require(retained==warm_release_receiving::cpu_image(s,materials.state(),transparency,&capture.textures(),scratch)&&
            same_polygon_snapshot(device_before,device.snapshot())&&capture_begin_count==probes,
            "warm stop lost CPU/font/overlay/effect owners or opened a capture");
        require(out.update.status==SourceOuterStatus::scene_child_required&&out.update.blocked_address==environment.blocked_address()&&
            out.update.entered_iterations==0&&!out.frame,"actual release/environment prefix returned wrong typed stop");
        environment.finished(trace.missing_scene_operations);
        require(s.outer.render_counter==(environment.sound_reset_completed()?0u:renders)&&surfaces.calls==0&&!device.capturing(),"warm reset counter or draw/capture boundary mismatch");

        entered(false,false);updated(41,environment.sound_reset_completed()?117u:100u,environment.sound_reset_completed());
        reject_before_mutation([&]{auto next=context();(void)capture.advance_and_capture_source_frame(next,request(serial+1));},environment.retry_reason());
    }
};

// Direct child checks bracket431810 itself, before any scheduler prefix.
void exercise_release(std::string_view group,RaceSession& session,FrontendState& frontend,
    SessionWorldMaterials& materials,PolygonSubmissionState& scratch,PolygonDeviceContext& device,
    MaterialD3D12& backend,MaterialTransparencyStorage& transparency,Renderer& renderer,SurfaceFailure& surfaces,
    const std::filesystem::path& root,HWND hwnd,const SourceGraphicsRoots& roots){
    namespace warm=warm_release_receiving;
    constexpr auto lease_error="material replacement owner was released, changed, or has an invalid slot";
    constexpr auto idle_error="source image release requires a healthy closed idle capture owner";
    const auto& state=materials.state(); // Borrow while world ticket is valid; owner outlives every check.
    auto cpu=[&](const RaceOverlayTextures* overlays){return warm::cpu_image(session.state(),state,transparency,overlays,scratch);};
    auto source=[&]{return image_of(session.state(),frontend,scratch);};
    auto platform=[&]{return std::pair{renderer.stats().presents,renderer.stats().uploaded_texture_bytes};};
    if(group=="release_foreign_backend"){
        // Both banks actually load the same retail resources; their token
        // sequences restart independently. Equal counts/tokens are intentional.
        RaceSession other(root,launch(hwnd));FrontendState other_front(other.shared_state(),other.shared_platform());SessionWorldMaterials other_materials(other);
        auto other_loading=loading_for(other,roots);MaterialTransparencyStorage other_transparency;
        MaterialD3D12 other_backend(renderer,other_transparency,false,1);other_backend.reset_transparency_430690();
        other_materials.load_with_fonts(other_loading,other_backend);other_materials.publish_count_42e772();
        warm::BankWitness first(backend),second(other_backend);
        require(first.count==second.count&&first.tokens==second.tokens,"foreign banks do not collide as required");
        require(first.bank->entries.front().resource.Get()!=second.bank->entries.front().resource.Get(),"foreign banks share a GPU object");
        const auto other_cpu=warm::cpu_image(other.state(),other_materials.state(),other_transparency,nullptr,scratch);
        const auto other_source=image_of(other.state(),other_front,scratch);
        {
            SessionOuterCapture wrong(session,materials,scratch,device,other_backend,session.assets(),surfaces);
            const auto before=cpu(&wrong.textures()),full=source();const auto device_before=device.snapshot();const auto stats=platform();
            rejects([&]{wrong.release_image_resources_431810();},"Session material release has another loading backend");
            require(cpu(&wrong.textures())==before&&source()==full&&same_polygon_snapshot(device_before,device.snapshot())&&platform()==stats,
                "foreign backend release changed source/device/platform");
            first.unchanged(backend);second.unchanged(other_backend);
        }
        require(other_cpu==warm::cpu_image(other.state(),other_materials.state(),other_transparency,nullptr,scratch)&&
            other_source==image_of(other.state(),other_front,scratch),"foreign release changed the other Session owners");
        SessionOuterCapture correct(session,materials,scratch,device,backend,session.assets(),surfaces);
        const auto before=cpu(&correct.textures()),full=source();const auto device_before=device.snapshot();const auto stats=platform();
        correct.release_image_resources_431810();first.released(backend);second.unchanged(other_backend);
        require(cpu(&correct.textures())==before&&source()==full&&same_polygon_snapshot(device_before,device.snapshot())&&platform()==stats,
            "valid release after identity refusal changed CPU owners");
        require(other_cpu==warm::cpu_image(other.state(),other_materials.state(),other_transparency,nullptr,scratch)&&
            other_source==image_of(other.state(),other_front,scratch),"valid release changed the other Session owners");
        return;
    }
    if(group=="release_capture_guards"||group=="release_generation"){
        auto& physical=materials.replacement_storage(session);PolygonSubmissionCapture capture(device,backend,physical);
        warm::BankWitness bank(backend);
        auto refused=[&](const char* reason){
            const auto before=cpu(nullptr),full=source();const auto device_before=device.snapshot();const auto stats=platform();
            const auto status=device.capture_status();
            rejects([&]{capture.release_image_resources_431810();},reason);
            require(cpu(nullptr)==before&&source()==full&&same_polygon_snapshot(device_before,device.snapshot())&&platform()==stats,
                "refused direct release changed CPU/device/platform");
            const auto after=device.capture_status();require(status.has_value()==after.has_value(),"release removed foreign/own capture");
            if(status)require(status->poisoned==after->poisoned&&status->commands==after->commands&&status->events==after->events&&
                status->payload_bytes==after->payload_bytes&&status->refused_calls==after->refused_calls&&
                status->unlogged_refusals==after->unlogged_refusals,"release changed capture status");
            bank.unchanged(backend);
        };
        if(group=="release_generation"){
            const auto generation=physical.generation();physical.reset_loading_42c480();
            require(physical.generation()==generation+1u&&physical.empty(),"actual material generation reset");
            refused(lease_error);return;
        }
        capture.begin_source_frame(9001,limits);refused(idle_error);
        const auto own=capture.seal_source_frame();require(own&&own->source_tick==9001&&!capture.poisoned(),"refused release destroyed own healthy capture");
        device.begin_capture(9002,bank.bank->epoch,limits.total);refused(idle_error);
        const auto foreign=device.seal_capture();require(foreign&&foreign->polygon_commands->source_tick==9002,"refused release destroyed foreign capture");
        require(device.begin_scene()==0,"actual open scene stimulus");refused(idle_error);
        require(device.end_scene()==0,"open scene survived release refusal");
        capture.begin_source_frame(9003,limits);
        require(device.set_texture_stage_state(2,1,3)!=0,"actual rejected device selector did not poison capture");
        rejects([&]{(void)capture.seal_source_frame();},"refused source device operation poisons submission");
        capture.discard_failed_source_frame();require(capture.poisoned()&&!device.capturing(),"actual failure cleanup not retained");
        refused("polygon submission capture is poisoned or reentrant");return;
    }
    SessionOuterCapture capture(session,materials,scratch,device,backend,session.assets(),surfaces);
    if(group=="release_idle_shared"||group=="release_idle_separate"){
        // Actual overlay replacement supplies nonzero selected/completed state
        // and a populated next-free physical record BEFORE release snapshots.
        capture.textures().replace_source_texture(15,27);
        require(capture.textures().selected_overlay_texture()==27u&&capture.textures().completed_source(15)==27u,
            "actual overlay retention witness not seeded");
        warm::BankWitness bank(backend);bank.aliases(state,group=="release_idle_separate");
        const auto before=cpu(&capture.textures()),full=source();const auto device_before=device.snapshot();const auto stats=platform();
        std::exception_ptr thread_error;
        std::thread worker([&]{try{capture.release_image_resources_431810();}catch(...){thread_error=std::current_exception();}});worker.join();
        require(thread_error!=nullptr,"wrong-thread release was admitted");
        rejects([&]{std::rethrow_exception(thread_error);},"Session materials belong to their creating thread");
        bank.unchanged(backend);
        require(cpu(&capture.textures())==before&&source()==full&&same_polygon_snapshot(device_before,device.snapshot())&&platform()==stats,
            "wrong-thread release mutated owners");
        capture.release_image_resources_431810();bank.released(backend);
        require(cpu(&capture.textures())==before&&source()==full&&same_polygon_snapshot(device_before,device.snapshot())&&platform()==stats,
            "actual431810 changed retained source material/CPU/font/overlay/effect state");
        rejects([&]{capture.release_image_resources_431810();},lease_error);bank.released(backend);
        require(cpu(&capture.textures())==before&&source()==full&&same_polygon_snapshot(device_before,device.snapshot())&&platform()==stats,
            "repeat stale release changed owners or advanced epoch twice");
        return;
    }
    warm::BankWitness bank(backend);
    const char* reason{};
    if(group=="release_pending"){
        backend.begin_pending_record(0);reason="material replacement requires a completed live resource bank";
    }else if(group=="release_stale_world"){
        detach_world_polygon_level(session.state().polygon_frame);reason="world polygon level is detached or replaced";
    }else if(group=="release_suspended"){
        ImmutablePolygonSubmission no_display;backend.suspend_renderer_for_recreation(no_display);
        require(!backend.bound_to_renderer(renderer),"actual backend suspension did not occur");
        reason="cannot publish bindings from a failed or suspended material backend";
    }else throw std::runtime_error("unknown direct release group");
    const auto before=cpu(&capture.textures()),full=source();const auto device_before=device.snapshot();const auto stats=platform();
    rejects([&]{capture.release_image_resources_431810();},reason);
    require(cpu(&capture.textures())==before&&source()==full&&same_polygon_snapshot(device_before,device.snapshot())&&platform()==stats,
        "invalid lifetime release mutated source/device/platform");
    if(group=="release_suspended"){
        require(backend.resource_entries()==bank.count&&backend.resource_epoch()==bank.bank->epoch&&!backend.bound_to_renderer(renderer),
            "refused suspended release changed epoch/count/binding");
        rejects([&]{(void)backend.bindings_snapshot();},reason);bank.retained();
    }else bank.unchanged(backend);
}

void exercise(Run& r,std::string_view name,const std::filesystem::path& root,HWND primary,HWND alternate){
    auto& s=r.session.state();
    if(name=="no_catchup"||name=="paused_iteration"||name=="warm_active_overlay"){
        const bool paused=name=="paused_iteration";
        if(name=="warm_active_overlay"){s.options.selection_phase=2;s.overlays[0].active=1;s.normal_finish.menu_requested=1;}
        const warm_release_receiving::BankWitness retained_bank(r.backend);
        if(paused){
            s.frame_pause=1;s.pause.age=37;s.trial_start.phase_ticks=1;s.startup.start_delay_ticks=5;
            set_overlay_transition(s.overlays[0],2,10,1,60,0,100,1);s.overlays[0].age=2;
            set_hud_countdown(s.hud_countdown,0,s.selected_hud,3);r.counter.value=110;
        }
        const auto renders=s.outer.render_counter;const auto epoch=r.backend.resource_epoch();auto context=r.context();
        rejects([&]{(void)r.capture.advance_and_capture_source_frame(context,r.request());},surface_error);
        r.entered(paused,true);r.updated(paused?42u:41u,paused?110u:100u);retained_bank.unchanged(r.backend);
        require(r.trace.missing_scene_operations.empty(),"guarded transition incorrectly entered a missing child");
        if(name=="warm_active_overlay")require(s.options.selection_phase==2u&&s.overlays[0].active==1u&&s.normal_finish.menu_requested==1u,
            "active overlay warm guard changed source target/pending/overlay");
        require(capture_begin_count==1&&!capture_probe_overflow&&capture_begins[0].tick==s.simulation_tick&&
            capture_begins[0].tick!=r.request().simulation_tick&&capture_begins[0].epoch==epoch,"capture used stale request tick or wrong epoch");
        require(s.outer.render_counter==renders+1&&r.surfaces.calls==1&&!r.device.capturing(),"actual draw prefix / failed capture cleanup");
        if(paused)require(s.frame_pause==1&&s.pause.raw_activity==0&&s.pause.age==38&&s.trial_start.phase_ticks==0&&
            s.startup.field_60f2c0==0&&s.overlays[0].initial_value==20&&s.overlays[0].duration==0&&
            s.overlays[0].age==2&&s.hud_countdown[0].ticks==2&&s.startup.start_delay_ticks==4,
            "actual paused4423D0/five suffix children did not advance");
        r.reject_before_mutation([&]{auto next=r.context();(void)r.capture.advance_and_capture_source_frame(next,r.request(72));},capture_error);
    }else if(name=="warm_stop"||name=="warm_environment_active"||name=="warm_environment_disabled"){
        if(name=="warm_environment_active")require(s.audio_driver.initialized==1&&s.audio_initialization.spatial_enabled==1,"active environment requires actual source audio startup");
        r.warm_stop(71,name=="warm_environment_disabled");require(capture_begin_count==0,"warm stop opened capture");
    }else if(name=="duplicate"){
        // Existing real capture API seeds the SAME cadence with an explicit
        // scene!=1 stimulus. No fake transition callback changes scene.
        s.scene_mode=2;const auto renders=s.outer.render_counter;
        const auto prior=r.capture.capture_source_frame(r.request());
        require(prior.source.kind==SessionOuterFrameKind::not_race&&!prior.submission&&prior.accepted_outer_serial==71&&
            s.outer.render_counter==renders&&r.surfaces.calls==0&&r.sources.calls==0&&!r.device.capturing(),"actual not_race cadence seed");
        require(capture_begin_count==1&&capture_begins[0].tick==0xabcdef,"existing capture seed metadata");
        s.scene_mode=1;
        r.reject_before_mutation([&]{auto c=r.context();(void)r.capture.advance_and_capture_source_frame(c,r.request());},cadence_error);
        // Higher serial still reaches real source: duplicate refusal itself
        // did not poison. The deliberate missing warm child then does poison.
        r.warm_stop(72);
    }else if(name=="foreign_session"){
        RaceSession other(root,launch(primary));FrontendState other_front(other.shared_state(),other.shared_platform());
        const auto foreign=image_of(other.state(),other_front,r.scratch);
        r.reject_before_mutation([&]{SourceRaceOuterContext c{other,other_front,r.counter,r.input,nullptr,r.trace,&r.audit};
            (void)r.capture.advance_and_capture_source_frame(c,r.request());},session_error);
        require(image_of(other.state(),other_front,r.scratch)==foreign,"foreign Session was mutated");
        r.warm_stop();
    }else if(name=="uninitialized_input"){
        r.reject_before_mutation([&]{auto c=r.context();(void)r.capture.advance_and_capture_source_frame(c,r.request());},phase_error);
        require(r.input.initialize_4249b0()==1,"actual input startup after rejected admission");r.warm_stop();
    }else if(name=="foreign_input"){
        // Same real Session/platform/control block, but its owner was attached
        // to another actual HWND. require_session must fail BEFORE phase check.
        require(alternate!=primary,"receiving HWND identities not distinct");
        r.reject_before_mutation([&]{auto c=r.context();(void)r.capture.advance_and_capture_source_frame(c,r.request());},input_error);
        require(capture_begin_count==0&&keyboard_calls==0,"foreign HWND entered source");
    }else if(name=="foreign_device_capture"){
        // A different coordinator already owns this actual underlying device.
        r.device.begin_capture(9001,9002,limits.total);const auto status=r.device.capture_status();
        require(status&&!status->poisoned,"foreign underlying capture setup");
        r.reject_before_mutation([&]{auto c=r.context();(void)r.capture.advance_and_capture_source_frame(c,r.request());},capture_error);
        const auto after=r.device.capture_status();
        require(r.device.capturing()&&after&&!after->poisoned&&after->commands==status->commands&&
            after->events==status->events&&after->payload_bytes==status->payload_bytes&&
            after->refused_calls==status->refused_calls&&after->unlogged_refusals==status->unlogged_refusals,
            "rejected outer poisoned/discarded another owner's capture");
        const auto retained=r.device.seal_capture();
        require(retained&&retained->polygon_commands->source_tick==9001&&retained->polygon_commands->resource_epoch==9002,
            "foreign underlying capture lost identity or could not seal");
        r.warm_stop(); // Same serial proves the guard did not poison outer cadence.
    }else throw std::runtime_error("unknown receiving group");
}
}
int main(int argc,char** argv)try{
    require(argc==3,"expected imported retail root and receiving group");const std::string_view group=argv[2];
    WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"SGN-Source-Outer-Session";
    require(RegisterClassW(&wc)!=0,"real window class");Window primary(wc.hInstance,wc.lpszClassName),alternate(wc.hInstance,wc.lpszClassName);
    fixture::Scenario os;fixture::active=&os;fixture::receiving_window=primary.handle;
    Renderer renderer(primary.handle,{true,true,false,false,false,1});renderer.resize(1280,720);
    require(renderer.stats().samples==1,"WARP single sample");
    {
        RaceSession session(argv[1],launch(primary.handle,group=="warm_environment_active"||group=="warm_environment_disabled"));FrontendState frontend(session.shared_state(),session.shared_platform());
        initialize_session_source_physical_input_45ec30(session);
        const bool wrong_window=group=="foreign_input";const bool cold=group=="uninitialized_input"||wrong_window;
        SessionWindowInput input(session.shared_platform(),session.shared_state(),
            {reinterpret_cast<std::uintptr_t>(wc.hInstance),reinterpret_cast<std::uintptr_t>(wrong_window?alternate.handle:primary.handle)});
        if(!cold)require(input.initialize_4249b0()==1,"actual4249B0 with explicit zero enumerated COM devices");
        {
            SessionWorldMaterials materials(session);const auto roots=source_graphics_roots_from_pe();auto loading=loading_for(session,roots);
            MaterialTransparencyStorage transparency;MaterialD3D12 backend(renderer,transparency,group=="release_idle_separate",1);
            backend.reset_transparency_430690();materials.load_with_fonts(loading,backend);materials.publish_count_42e772();
            require(materials.validate_fonts_for(session).count==2,"actual material and two-font loading");
            PolygonSubmissionState scratch;scratch.alpha_vertices.resize(4);
            PolygonDeviceContext device{PolygonDeviceState{}};SurfaceFailure surfaces;
            if(group.starts_with("release_")){
                exercise_release(group,session,frontend,materials,scratch,device,backend,transparency,renderer,surfaces,
                    argv[1],primary.handle,roots);
                require(!device.capturing()&&!capture_probe_overflow&&surfaces.calls==0&&keyboard_calls==0,
                    "direct release entered surface/input work or left capture active");
            }else{
                SessionOuterCapture capture(session,materials,scratch,device,backend,session.assets(),surfaces);
                const auto presents=renderer.stats().presents,uploads=renderer.stats().uploaded_texture_bytes;
                Run run(session,frontend,input,materials,transparency,scratch,device,backend,capture,surfaces);run.seed();
                exercise(run,group,argv[1],primary.handle,alternate.handle);
                require(!device.capturing()&&!capture_probe_overflow&&run.sources.calls==0&&
                    renderer.stats().presents==presents&&renderer.stats().uploaded_texture_bytes==uploads,
                    "unexpected surviving capture, draw child, present or texture upload");
            }
        } // capture/overlay/material and device owners precede Session shutdown.
        if(!wrong_window)input.shutdown_424930();
    } // real Session/platform finalization while COM fixture and HWNDs live.
    os.clean();fixture::active=nullptr;
    renderer.finish();renderer.collect_diagnostics();renderer.check_health();
    require(renderer.stats().debug_active&&renderer.stats().debug_errors==0,"GPU debug warning/error");
    std::cout<<"passed=1 group="<<group<<" actual_session=1 actual_material_fonts=2 surface_policy=explicit_failure"
        <<" full_draw=0 new_presents=0 game_callbacks_substituted=0 debug_warnings_errors=0"
        <<" keyboard_calls="<<keyboard_calls<<" observed_capture_begins="<<capture_begin_count<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
