// Five new typed/backend/provider transitions. Execution belongs to Main only.
#include "backend_receiving_access.hpp"
#include "warm_release_checks.hpp"
#include "../source_warm_allocation_witness.hpp"
#include "../source_warm_environment_witness.hpp"
#include "../source_warm_continuation_witness.hpp"
#include "fixture_com.hpp"
#include "capture_probe.hpp" // Existing native keyboard boundary's counters only.
#include "sgn/race_session.hpp"
#include "sgn/race_session_iteration.hpp"
#include "sgn/race_session_pause_restart.hpp"
#include "sgn/source_input_cold_init.hpp"
#include "sgn/session_effect_resources.hpp"
#include "sgn/session_world_materials.hpp"
#include "sgn/material_d3d12.hpp"
#include "sgn/renderer.hpp"
#include <algorithm>
#include <bit>
#include <filesystem>
#include <iostream>
#include <string_view>

namespace sgn::retail {unsigned verify_overlay_world_native_bindings(SessionOuterFrameContext&,SelectedDeviceDepthBorrow);}

namespace {
using namespace sgn;using namespace sgn::retail;using fixture::require;
using Access=TypedOuterReceivingAccess;
constexpr PolygonSubmissionCaptureLimits limits{{{20000,32u*1024u*1024u},30000},16};
constexpr auto world_error="Session outer world resource producer is missing";
template<class F>void rejects(F action,const char* expected){
    try{action();}catch(const FormatError& e){require(std::string_view(e.what())==expected,"wrong receiving rejection");return;}
    throw std::runtime_error(std::string("expected rejection: ")+expected);
}
using SourceBytes=std::vector<unsigned char>;
template<class T>void append(SourceBytes& out,const T& value){
    const auto* p=reinterpret_cast<const unsigned char*>(&value);out.insert(out.end(),p,p+sizeof(value));
}
SourceBytes source_image(const RaceSessionState& s,const PolygonSubmissionState& scratch){
    // Same objects before/after only; not a recursive resource-heap proof.
    SourceBytes out;append(out,s);append(out,scratch);
    for(const auto& x:scratch.alpha_vertices)append(out,x);
    for(const auto& x:scratch.indexed_vertices)append(out,x);
    for(const auto& x:scratch.indices)append(out,x);return out;
}
struct Window{HWND handle{};~Window(){if(handle)DestroyWindow(handle);}};
RaceSessionLaunch launch(HWND hwnd,bool spatial=false){RaceSessionLaunch l;l.mode=SessionMode::normal_race;
    l.master_gain=0;l.sound_percent=spatial?100u:0u;l.music_percent=0;l.window=reinterpret_cast<std::uint64_t>(hwnd);return l;}
WorldMaterialLoadingContext loading_for(const RaceSession& session,const SourceGraphicsRoots& roots){
    WorldMaterialLoadingContext c;c.source_graphics_roots=&roots;c.scene=session.state().scene_mode;
    const auto& p=session.state().level.track_texture;const auto end=std::find(p.begin(),p.end(),'\0');
    require(end!=p.end(),"actual track texture prefix");c.track_prefix={p.begin(),end};c.quality=0;c.maximum_dimension=4096;return c;
}
std::array<std::uint64_t,5> image_words(const Renderer& renderer){
    const auto image=renderer.polygon_surface_roles();
    return {image.active?1u:0u,image.front_image,image.back_image,image.width,image.height};
}
struct Counter final:FrontendPerformanceCounter {
    std::uint64_t value{100};unsigned reads{};
    warm_release_receiving::ResetContinuationWitness* warm_reset{};
    std::uint64_t counter() override{++reads;return warm_reset?warm_reset->counter(reads,value):value;}
    std::uint64_t frequency() override{if(warm_reset)throw std::runtime_error("warm reset queried frequency");return 600;}
};
PolygonDeviceState platform_seed(std::uint32_t token){
    // Explicit analytic PLATFORM fixture from existing light_gamma_receiving.
    // These are not recovered game initializers or a successful source frame.
    PolygonDeviceState state;
    for(auto [i,v]:std::initializer_list<std::pair<unsigned,unsigned>>{{4,1},{7,1},{8,3},{9,2},{14,1},{15,1},{19,2},{20,1},{22,1},{23,7},{24,1},{25,7},{27,0},{28,0},{29,0}})state.render[i]=v;
    for(unsigned stage=0;stage<2;++stage)
        for(auto [i,v]:std::initializer_list<std::pair<unsigned,unsigned>>{{1,stage?1u:3u},{2,2},{3,0},{4,3},{5,2},{6,0},{11,stage},{13,3},{14,3},{16,1},{17,1},{18,0}})state.stage[stage][i]=v;
    state.render[41]=0;state.texture[0]=token;state.texture[1]=0;return state;
}
ImmutablePolygonSubmission platform_submission(PolygonDeviceContext& device,MaterialD3D12& backend,PhysicalMaterialStorage& materials,bool second){
    // This temporary real coordinator is destroyed BEFORE constructing the
    // application's sole coordinator. Its bank/CPU commands are immutable.
    const std::uint64_t tick=second?7002u:7001u;
    PolygonSubmissionCapture capture(device,backend,materials);capture.begin_source_frame(tick,limits);
    try{
        require(device.begin_scene()==0,"analytic platform BeginScene");
        const float left=second?208.0f:16.0f;const std::uint32_t color=second?0xffc93167u:0xff7145c9u;
        std::array<PolygonSubmissionVertex,4> vertices{};unsigned i=0;
        for(const auto [x,y]:std::array<std::array<float,2>,4>{{{left,16},{left+144,16},{left+144,120},{left,120}}})
            vertices[i++].words={std::bit_cast<std::uint32_t>(x),std::bit_cast<std::uint32_t>(y),0x3f000000,0x3f800000,color,0,0,0,0,0};
        ViewportState viewport;viewport.full_width=1280;viewport.full_height=720;
        require(device.draw(6,0x2c4,vertices,0x1c,viewport)==0&&device.end_scene()==0,"analytic platform quad and EndScene");
        auto frame=capture.seal_source_frame();
        require(frame&&frame->commands==1&&frame->source_tick==tick&&!frame->segments.empty(),"real immutable nonempty platform submission");
        return frame;
    }catch(...){capture.discard_failed_source_frame();throw;}
}
struct Run {
    RaceSession& session;FrontendState& frontend;SessionWindowInput& input;SessionWorldMaterials& materials;
    MaterialTransparencyStorage& transparency;
    PolygonSubmissionState& scratch;PolygonDeviceContext& device;MaterialD3D12& material_backend;
    Renderer& renderer;RaceApplicationPolygonBackend& app;
    SessionPauseRestartSource restart;SessionWorldFrameResources current;std::uint32_t depth{1};
    Counter counter;SourceOuterTrace trace;
    Run(RaceSession& s,FrontendState& f,SessionWindowInput& i,SessionWorldMaterials& m,MaterialTransparencyStorage& t,PolygonSubmissionState& p,
        PolygonDeviceContext& d,MaterialD3D12& b,Renderer& r,RaceApplicationPolygonBackend& a):
        session(s),frontend(f),input(i),materials(m),transparency(t),scratch(p),device(d),material_backend(b),renderer(r),app(a),restart(s){
        publish_session_effect_resources(session,current);current.gamma=&app.gamma_device();current.submission_budget=desktop_polygon_submission_budget;
    }
    SessionOuterWorldResourceBorrow world(SessionOuterGammaBinding kind=SessionOuterGammaBinding::source_device_present){return {session,current,kind};}
    SessionOuterBoundCaptureRequest request(std::uint64_t serial=81){return {restart,SelectedDeviceDepthBorrow(depth),world(),serial,0xabcdef,limits};}
    void clock(){initialize_session_source_clock_434840(session,counter);counter.reads=0;session.state().simulation_tick=41;
        session.state().options.selection_phase=session.state().scene_mode;session.state().controls.current=0;}
    SourceRaceOuterContext context(){return {session,frontend,counter,input,nullptr,trace,nullptr};}
    template<class F>void read_only(F action){
        const auto before=source_image(session.state(),scratch);const auto cpu=device.snapshot();
        const auto epoch=material_backend.resource_epoch(),uploads=renderer.stats().uploaded_texture_bytes,presents=renderer.stats().presents;
        const auto gamma=Access::current_gamma(app);const auto ramps=Access::ramps(app);const auto recipes=Access::submissions(app);const auto images=image_words(renderer);
        const auto applies=Access::applies(app);const auto keys=source_outer_receiving::keyboard_calls;
        action();
        require(before==source_image(session.state(),scratch)&&same_polygon_snapshot(cpu,device.snapshot())&&epoch==material_backend.resource_epoch()&&
            uploads==renderer.stats().uploaded_texture_bytes&&presents==renderer.stats().presents&&gamma==Access::current_gamma(app)&&
            ramps==Access::ramps(app)&&recipes==Access::submissions(app)&&images==image_words(renderer)&&applies==Access::applies(app)&&
            keys==source_outer_receiving::keyboard_calls,"provider getter changed source/platform owners");
    }
};
void check_hud_aliases(const RaceHudSubmission& h,Run& r){
    auto& s=r.session.state();const auto physical=r.materials.validate_for(r.session).names.materials.active_materials();
    require(&h.hud.controls==&s.controls&&h.hud.countdown.data()==s.hud_countdown.data()&&&h.hud.selected_hud==&s.selected_hud&&
        &h.vehicles==&s.vehicles&&&h.clock==&s.clock&&&h.records==&s.records&&&h.ranking==&s.ranking&&
        h.cars.data()==s.cars.data()&&&h.game_text==&s.game_text&&&h.split_setting==&s.split_setting&&&h.frame_pause==&s.frame_pause,
        "HUD source owners copied or foreign");
    require(&h.overlay_textures==&r.app.textures()&&&h.stars==&s.ui.stars&&h.descriptor.data()==s.ui.descriptor.data()&&
        h.vertices.data()==r.scratch.alpha_vertices.data()&&&h.rendering.fonts==&s.ui.fonts&&h.rendering.fonts.count==2&&
        &h.rendering.selected_material==&s.ui.selected_material&&&h.rendering.specular_enabled==&r.scratch.specular_enabled&&
        h.rendering.materials.data()==physical.data()&&h.rendering.materials.size()==physical.size()&&
        &h.rendering.quality==&s.view_quality.second&&&h.rendering.legacy_driver_restriction==&s.view_quality.legacy_driver_restriction&&
        &h.rendering.viewport==&s.viewport,"HUD material/font/scratch/texture identity");
    require(&h.audio.state==&s.audio_driver&&&h.audio.device==&r.session.audio_device()&&&h.audio.level==&s.audio_level&&
        &h.audio.controls==&s.controls&&&h.audio.split_setting==&s.split_setting,"HUD retained actual AudioDriver references");
}
void services_identity(Run& r,const std::filesystem::path& root,HWND hwnd){
    auto& s=r.session.state();auto* const gamma=&r.app.gamma_device();
    r.read_only([&]{
        SessionOuterFrameServices sources(r.session,r.materials,r.scratch,r.app.textures(),SelectedDeviceDepthBorrow(r.depth),r.world(),gamma);
        SessionOuterFrameContext frame{r.session,r.materials,sources.capabilities(),r.scratch,r.device,r.app.textures(),r.restart,
            sources,sources.device_words(),Access::surface_device(r.app)};
        require(verify_overlay_world_native_bindings(frame,SelectedDeviceDepthBorrow(r.depth))==25,"overlay world native guard count");
    });
    std::cout<<"overlay_world_guard_checks=25 actual_context=1 foreign_device_calls=0\n";
    r.current.gamma=nullptr;r.current.submission_budget.packet_visits=17;
    SessionOuterFrameServices services(r.session,r.materials,r.scratch,r.app.textures(),SelectedDeviceDepthBorrow(r.depth),r.world(),gamma);
    r.read_only([&]{
        auto hud=services.borrow_countdown();check_hud_aliases(hud,r);auto* const audio=&hud.audio;
        const auto again=services.borrow_countdown();require(&again.audio==audio,"AudioDriver temporary escaped provider");
        const auto fade=services.borrow_age_fade();
        require(fade.descriptor.data()==s.ui.descriptor.data()&&fade.vertices.data()==r.scratch.alpha_vertices.data()&&
            &fade.selected_material==&s.ui.selected_material&&&fade.viewport==&s.viewport&&&fade.frame_pause==&s.frame_pause&&&fade.age==&s.pause.age,
            "fade sources not actual Session owners");
        const auto loading=services.borrow_loading_frame();const auto words=services.device_words();const auto capabilities=services.capabilities();
        require(&loading.submission==&r.scratch&&&loading.depth_capability==&r.depth&&&loading.fog_a==&s.outer.device.fog_a&&
            &loading.fog_b==&s.outer.device.fog_b&&&loading.scene_active==&s.outer.device.scene_active&&
            &loading.finalized==&s.outer.device.finalized&&&loading.render_counter==&s.outer.render_counter&&
            loading.quads.overlays.data()==s.overlays.data()&&&loading.quads.controls==&s.controls&&
            &words.render_counter==&s.outer.render_counter&&&capabilities.depth==&r.depth&&&capabilities.multitexture==&s.outer.device.flag_592074,
            "loading/capability words not actual persistent aliases");
        const auto demo=services.borrow_demo_words();require(&demo.age==&s.outer.demo.age&&&demo.offset==&s.outer.demo.offset&&
            &demo.initialized==&s.outer.demo.initialized,"demo identity");
        rejects([&]{(void)services.borrow_world_resources();},"Session outer gamma receiver is missing");
    });
    // Actual source update, not a callback that rewrites a resource fixture.
    // This directly tests the provider across a real child. The full typed
    // application's missing-resource case separately reaches this getter
    // after its real Flip and BeginScene; full world drawing remains unqualified.
    r.clock();s.frame_pause=1;s.pause.age=37;s.pause.raw_activity=0;s.trial_start.phase_ticks=1;s.startup.start_delay_ticks=5;
    set_overlay_transition(s.overlays[0],2,10,1,60,0,100,1);s.overlays[0].age=2;
    set_hud_countdown(s.hud_countdown,0,s.selected_hud,3);
    advance_session_iteration(r.session,r.app.textures(),borrow_session_effect_animation_frames(r.session));
    require(s.simulation_tick==42&&s.pause.age==38&&s.hud_countdown[0].ticks==2&&s.startup.start_delay_ticks==4,"actual paused source update before fresh getter");
    // Explicit host republishing BETWEEN source children, never inside a source
    // callback. Effect pointers originate from actual persistent Session owners.
    publish_session_effect_resources(r.session,r.current);r.current.gamma=gamma;r.current.submission_budget.packet_visits=29;
    require(r.scratch.alpha_vertices.size()==8,"receiving UI scratch extent before replacement");
    std::vector<PolygonSubmissionVertex> replacement(19);std::copy(r.scratch.alpha_vertices.begin(),r.scratch.alpha_vertices.end(),replacement.begin());
    r.scratch.alpha_vertices.swap(replacement);s.ui.selected_material=15;r.depth=2;
    r.read_only([&]{
        const auto now=services.borrow_world_resources();require(now.gamma==gamma&&now.submission_budget.packet_visits==29&&
            now.effect_animations.data()==r.current.effect_animations.data()&&now.effect_animations.size()==r.current.effect_animations.size()&&
            now.adjacent_angles.data()==r.current.adjacent_angles.data()&&now.effect_colors==&s.effect_resources.colors.borrow_for(s.sprite_packets),
            "world getter retained constructor snapshot or wrong actual effect owner");
        auto hud=services.borrow_countdown();check_hud_aliases(hud,r);require(hud.hud.countdown[0].ticks==2&&hud.rendering.selected_material==15&&
            services.borrow_loading_frame().depth_capability==2,"fresh post-update HUD/material/scratch/depth views");
    });
    LightGammaD3D12 foreign_gamma;
    r.current.gamma=&foreign_gamma;r.read_only([&]{rejects([&]{(void)services.borrow_world_resources();},"Session outer gamma receiver differs from its capture backend");});
    r.current.gamma=gamma;
    r.read_only([&]{
        SessionOuterFrameServices missing(r.session,r.materials,r.scratch,r.app.textures(),SelectedDeviceDepthBorrow(r.depth),std::nullopt,gamma);
        rejects([&]{(void)missing.borrow_world_resources();},"Session outer world resource producer is missing");
        SessionOuterFrameServices unresolved(r.session,r.materials,r.scratch,r.app.textures(),SelectedDeviceDepthBorrow(r.depth),r.world(SessionOuterGammaBinding::unresolved),gamma);
        rejects([&]{(void)unresolved.borrow_world_resources();},"Session outer gamma device availability is unresolved");
    });
    r.current.gamma=nullptr;
    r.read_only([&]{
        SessionOuterFrameServices absent(r.session,r.materials,r.scratch,r.app.textures(),SelectedDeviceDepthBorrow(r.depth),r.world(SessionOuterGammaBinding::source_device_absent),gamma);
        rejects([&]{(void)absent.borrow_world_resources();},"Session outer gamma absence differs from its capture backend");
    });
    r.current.gamma=gamma;
    RaceSession other(root,launch(hwnd));SessionWorldMaterials foreign_materials(other);
    r.read_only([&]{
        SessionOuterFrameServices wrong_world(r.session,r.materials,r.scratch,r.app.textures(),SelectedDeviceDepthBorrow(r.depth),
            SessionOuterWorldResourceBorrow{other,r.current,SessionOuterGammaBinding::source_device_present},gamma);
        rejects([&]{(void)wrong_world.borrow_world_resources();},"Session outer world resources belong to a different Session");
        SessionOuterFrameServices wrong_materials(r.session,foreign_materials,r.scratch,r.app.textures(),SelectedDeviceDepthBorrow(r.depth),r.world(),gamma);
        rejects([&]{(void)wrong_materials.borrow_countdown();},"Session materials belong to a different world");
    });
    require(!r.app.displayed()&&!r.app.draw(r.renderer)&&!r.renderer.polygon_surface_roles().active&&r.trace.entered.empty(),"provider test manufactured a displayed game frame");
}
void font_guard(Run& r){
    (void)r.materials.validate_for(r.session); //Actual loaded/published materials, intentionally no font initializer.
    require(r.session.state().ui.fonts.count==0,"missing-font stimulus not genuine");
    SessionOuterFrameServices services(r.session,r.materials,r.scratch,r.app.textures(),SelectedDeviceDepthBorrow(r.depth),r.world(),&r.app.gamma_device());
    r.read_only([&]{
        rejects([&]{(void)services.borrow_countdown();},"Session UI requires completed font initialization before publication");
        const auto world=services.borrow_world_resources();require(world.gamma==&r.app.gamma_device(),"world getter incorrectly requires fonts");
        (void)services.borrow_demo_words();
    });
    require(!r.app.displayed()&&!r.app.draw(r.renderer)&&!r.renderer.polygon_surface_roles().active,"font guard issued frame work");
}
void retained_pair(Run& r,std::string_view group,std::array<ImmutablePolygonSubmission,2> seeded){
    const bool warm=group=="typed_stop"||group=="typed_environment_active"||group=="typed_environment_disabled";
    auto& s=r.session.state();auto* const gamma=&r.app.gamma_device();
    require(seeded[0]&&seeded[1]&&seeded[0]!=seeded[1]&&seeded[0]->source_tick==7001&&seeded[1]->source_tick==7002,
        "two distinct actual platform captures required");
    const auto seed_presents=r.renderer.stats().presents,seed_updates=r.renderer.stats().live_updates;
    set_light_gamma(s.light_fade,3.0f,gamma);const auto front_ramp=Access::current_gamma(r.app);
    Access::seed_completed_platform_submission(r.app,std::move(seeded[0]));
    require(!r.app.displayed()&&r.renderer.polygon_surface_roles().back_image&&!r.renderer.polygon_surface_roles().front_image,
        "first platform seed was displayed before actual Flip");
    Access::flip_first_platform_seed(r.app);
    set_light_gamma(s.light_fade,4.0f,gamma);const auto back_ramp=Access::current_gamma(r.app);
    Access::seed_completed_platform_submission(r.app,std::move(seeded[1]));
    const auto retained=Access::submissions(r.app);const auto retained_ramps=Access::ramps(r.app);
    const auto retained_images=image_words(r.renderer);
    require(retained[0]&&retained[1]&&retained[0]!=retained[1]&&retained[0]->commands==1&&retained[1]->commands==1&&
        retained_ramps[0]==front_ramp&&retained_ramps[1]==back_ramp&&front_ramp!=back_ramp&&retained_images[0]==1&&
        retained_images[1]&&retained_images[2]&&retained_images[1]!=retained_images[2]&&
        r.renderer.stats().live_updates==seed_updates+2&&r.renderer.stats().presents==seed_presents,
        "seed did not produce two actual GPU images and distinct paired gamma ramps without Present");
    set_light_gamma(s.light_fade,2.0f,gamma);const auto current_gamma=Access::current_gamma(r.app);const auto applies=Access::applies(r.app);
    require(current_gamma!=front_ramp&&current_gamma!=back_ramp&&s.light_fade.last_gamma==2.0f,"current gamma witness must differ from both retained ramps");
    const auto presents=r.renderer.stats().presents,uploads=r.renderer.stats().uploaded_texture_bytes,epoch=r.material_backend.resource_epoch();
    const auto updates=r.renderer.stats().live_updates;
    const warm_release_receiving::BankWitness bank(r.material_backend);
    std::array<std::vector<warm_release_receiving::Bytes>,2> frame_banks;
    for(std::size_t role=0;role<2;++role)for(const auto& segment:retained[role]->segments){
        require(segment.textures&&same_polygon_bank(*segment.textures,*bank.bank),"retained role lacks actual pre-release GPU bank");
        frame_banks[role].push_back(warm_release_receiving::bank_image(*segment.textures));
    }
    if(group=="typed_null"){
        // Genuine not_race from explicit scene seed; no transition callback.
        s.scene_mode=2;auto request=r.request();request.world_resources=std::nullopt;
        const auto before=r.device.snapshot();const auto renders=s.outer.render_counter;
        const auto out=r.app.capture_source_frame(request);
        require(out.source.kind==SessionOuterFrameKind::not_race&&!out.submission&&image_words(r.renderer)==retained_images&&
            r.trace.entered.empty()&&same_polygon_snapshot(before,r.device.snapshot())&&s.outer.render_counter==renders,
            "typed null result changed roles or bypassed source guard/lazy resources");
    }else{
        r.clock();s.frame_pause=0;auto context=r.context();unsigned expected_counter_reads=1;
        if(warm){
            const warm_release_receiving::AllocationWitness allocations(*r.session.shared_platform());
            s.options.selection_phase=2;s.overlays[0].active=0;s.normal_finish.menu_requested=1;
            if(group=="typed_environment_active")require(s.audio_driver.initialized==1&&s.audio_initialization.spatial_enabled==1,"active environment requires actual source audio startup");
            s.outer.render_counter=0x92345678u; //receiving canary: source reset must become observable
            const warm_release_receiving::EnvironmentWitness environment(r.session,group=="typed_environment_disabled");
            expected_counter_reads=environment.sound_reset_completed()?3u:1u;
            warm_release_receiving::ResetContinuationWitness continuation(r.session,r.frontend,allocations,environment.sound_reset_completed());
            r.counter.warm_reset=&continuation;
            const auto retained_cpu=warm_release_receiving::cpu_image(s,r.materials.state(),r.transparency,&r.app.textures(),r.scratch,environment.expected_selected_overlay());
            const auto device_before=r.device.snapshot();const auto renders=s.outer.render_counter;
            const auto out=r.app.advance_and_capture_source_frame(context,r.request());
            require(out.update.status==SourceOuterStatus::scene_child_required&&out.update.blocked_address==environment.blocked_address()&&
                out.update.entered_iterations==0&&!out.frame&&image_words(r.renderer)==retained_images,
                "typed wrapper did not propagate actual first warm stop");
            environment.finished(r.trace.missing_scene_operations);
            bank.released(r.material_backend);continuation.finished();r.counter.warm_reset=nullptr;
            require(retained_cpu==warm_release_receiving::cpu_image(s,r.materials.state(),r.transparency,&r.app.textures(),r.scratch)&&
                same_polygon_snapshot(device_before,r.device.snapshot())&&s.outer.render_counter==(environment.sound_reset_completed()?0u:renders)&&s.options.selection_phase==2u,
                "typed warm release changed source owners, target or draw state");
            const auto missing=r.trace.missing_scene_operations;
            r.read_only([&]{auto next=r.context();rejects([&]{(void)r.app.advance_and_capture_source_frame(next,r.request(82));},
                environment.retry_reason());});
            require(r.trace.missing_scene_operations==missing,"poisoned typed retry added source operations");

        }else if(group=="typed_exception"){
            // Public typed API stimulus, not a surface HRESULT replacement:
            //4427C9 actual Flip ->4427CE BeginScene ->4427EC/442640 view1 ->
            //actual borrow_world_resources rejects the missing producer.
            // Actual RaceSession constructor already initialized full dimensions
            // through initialize_viewport; this test adds no cold initializer.
            s.viewport.cropped=0;s.split_setting=0;
            require(s.outer.device.scene_active==0&&!r.device.snapshot().scene_open&&
                s.viewport.full_width>0&&s.viewport.full_height>0,"exception stimulus needs actual closed scene and valid viewport");
            const auto renders=s.outer.render_counter,finalized=s.outer.device.finalized;
            const auto before=r.device.snapshot();auto request=r.request();request.world_resources=std::nullopt;
            rejects([&]{(void)r.app.advance_and_capture_source_frame(context,request);},world_error);
            const auto after=r.device.snapshot();const auto last_call=r.device.last_call();
            require(s.outer.render_counter==renders+1&&s.outer.device.scene_active==1&&after.scene_open&&
                after.operation_sequence==before.operation_sequence+1&&same_polygon_state(before.polygon,after.polygon)&&before.light==after.light&&
                last_call&&last_call->operation==PolygonDeviceOperation::begin_scene&&last_call->hresult==0&&
                s.outer.device.finalized==finalized&&s.viewport.applied_selector==1,
                "typed exception did not retain exact reached BeginScene/viewport prefix before missing resources");
            require(!r.device.capturing(),"failed typed source capture was not discarded");
            r.read_only([&]{auto next=r.context();rejects([&]{(void)r.app.advance_and_capture_source_frame(next,r.request(82));},
                "source outer capture is already active or poisoned");});
            // No EndScene, source word reset or image swap-back is introduced.
        }else throw std::runtime_error("unknown retained-pair group");
        const std::vector<SourceOuterBoundary> prefix{SourceOuterBoundary::numeric,SourceOuterBoundary::camera_reset,
            SourceOuterBoundary::input_pair,SourceOuterBoundary::sample_clock,SourceOuterBoundary::audio_pair,SourceOuterBoundary::scene_transition};
        auto expected=prefix;if(group=="typed_exception")expected.push_back(SourceOuterBoundary::draw);
        require(r.trace.entered==expected&&r.counter.reads==expected_counter_reads&&source_outer_receiving::keyboard_calls==1&&s.simulation_tick==41,
            "typed update wrapper skipped or duplicated actual source work");
    }
    const std::size_t front=group=="typed_exception"?1u:0u,back=1u-front;
    const auto actual=Access::submissions(r.app);const auto actual_ramps=Access::ramps(r.app);const auto actual_images=image_words(r.renderer);
    require(actual[0]==retained[front]&&actual[1]==retained[back]&&r.app.displayed()==retained[front]&&
        actual_ramps[0]==retained_ramps[front]&&actual_ramps[1]==retained_ramps[back]&&actual_images[0]==1&&
        actual_images[1]==retained_images[front+1]&&actual_images[2]==retained_images[back+1]&&
        actual_images[3]==retained_images[3]&&actual_images[4]==retained_images[4]&&
        Access::current_gamma(r.app)==current_gamma&&Access::applies(r.app)==applies&&s.light_fade.ramp==current_gamma&&&r.app.gamma_device()==gamma,
        "typed result has mismatched CPU/GPU roles, delayed Flip, or rolled-back current gamma");
    require(!r.device.capturing()&&r.renderer.stats().presents==presents&&r.renderer.stats().uploaded_texture_bytes==uploads&&
        r.renderer.stats().live_updates==updates&&r.material_backend.resource_epoch()==epoch+(warm?1u:0u),
        "typed non-displayable result leaked capture, rasterization, upload or Present");
    if(warm)bank.released(r.material_backend);else bank.unchanged(r.material_backend);
    for(std::size_t role=0;role<2;++role){
        require(retained[role]->segments.size()==frame_banks[role].size(),"retained role segment count changed");
        for(std::size_t i=0;i<frame_banks[role].size();++i)
            require(warm_release_receiving::bank_image(*retained[role]->segments[i].textures)==frame_banks[role][i],
                "immutable role bank lost GPU/pixel owners");
        (void)validate_polygon_submission(*retained[role]);
    }
}
}
int main(int argc,char** argv)try{
    require(argc==3,"expected imported retail root and typed group");const std::string_view group=argv[2];
    const bool no_fonts=group=="font_guard",identity=group=="services_identity";
    require(identity||no_fonts||group=="typed_null"||group=="typed_stop"||group=="typed_environment_active"||group=="typed_environment_disabled"||group=="typed_exception","unknown typed group");
    WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"SGN-Typed-Outer-Receiving";
    require(RegisterClassW(&wc)!=0,"real receiving window class");
    Window window{CreateWindowExW(0,wc.lpszClassName,L"StuntGP NEW typed outer receiving",WS_OVERLAPPEDWINDOW,
        0,0,1280,720,nullptr,nullptr,wc.hInstance,nullptr)};require(window.handle!=nullptr,"actual hidden HWND");
    fixture::Scenario os;fixture::active=&os;fixture::receiving_window=window.handle;
    Renderer renderer(window.handle,{true,true,false,false,false,1});renderer.resize(1280,720);
    require(renderer.stats().samples==1,"real WARP single sample");
    {
        RaceSession session(argv[1],launch(window.handle,group=="typed_environment_active"||group=="typed_environment_disabled"));FrontendState frontend(session.shared_state(),session.shared_platform());
        initialize_session_source_physical_input_45ec30(session);SessionWindowInput input(session,reinterpret_cast<std::uintptr_t>(wc.hInstance));
        require(input.initialize_4249b0()==1,"actual4249B0 external zero-device enumeration");
        {
            SessionWorldMaterials materials(session);const auto roots=source_graphics_roots_from_pe();auto loading=loading_for(session,roots);
            MaterialTransparencyStorage transparency;MaterialD3D12 backend(renderer,transparency,false,1);backend.reset_transparency_430690();
            if(no_fonts)materials.load(loading,backend);else materials.load_with_fonts(loading,backend);
            materials.publish_count_42e772();if(!no_fonts)require(materials.validate_fonts_for(session).count==2,"actual two-font load");
            PolygonSubmissionState scratch;prepare_polygon_submission_ui_capacity(scratch,desktop_polygon_submission_budget);
            PolygonDeviceContext device(platform_seed(backend.resource_primary_token(15)));
            std::array<ImmutablePolygonSubmission,2> seeded;
            if(!identity&&!no_fonts){
                seeded[0]=platform_submission(device,backend,materials.replacement_storage(session),false);
                seeded[1]=platform_submission(device,backend,materials.replacement_storage(session),true);
            }
            RaceApplicationPolygonBackend app(session,materials,scratch,device,backend,renderer,session.assets());
            Run run(session,frontend,input,materials,transparency,scratch,device,backend,renderer,app);
            const auto presents=renderer.stats().presents;
            if(identity)services_identity(run,argv[1],window.handle);else if(no_fonts)font_guard(run);else retained_pair(run,group,std::move(seeded));
            require(renderer.stats().presents==presents&&!device.capturing(),"typed receiving presented or left capture open");
        }
        input.shutdown_424930();
    }
    os.clean();fixture::active=nullptr;renderer.finish();renderer.collect_diagnostics();renderer.check_health();
    require(renderer.stats().debug_active&&renderer.stats().debug_errors==0,"typed receiving GPU debug warning/error");
    std::cout<<"passed=1 group="<<group<<" actual_application_owner=1 typed_game_full_draw=0 Present=0"
        <<" game_body_replacements=0 actual_surface_adapter=1 platform_seeded_submissions="<<(!identity&&!no_fonts?2:0)
        <<" fonts_loaded="<<(no_fonts?0:2)<<" debug_warnings_errors=0"
        <<" exception_after_real_Flip_and_Begin="<<(group=="typed_exception"?1:0)<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
