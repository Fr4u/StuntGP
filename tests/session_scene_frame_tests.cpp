// New-project receiving derivative; source test SHA256 6c17f5c5c5bb63c56536cde42dfee0ca8c1728ac237d5e481f3289ac1a91d0fa
#include "sgn/session_scene_frame.hpp"
#include "sgn/material_d3d12.hpp"
#include "sgn/polygon_device_context.hpp"
#include "sgn/retail_camera_frame_session.hpp"
#include "sgn/retail_screen_polygon_submission.hpp"
#include "sgn/retail_screen_polygons.hpp"
#include "sgn/material_texture_replacement.hpp"
#include "sgn/playable_driver.hpp"
#include "session_world_frame_archive.hpp"
#include <algorithm>
#include <bit>
#include <fstream>
#include <iostream>
#include <thread>

using namespace sgn;using namespace sgn::retail;
namespace {
void require(bool condition,const char* message){if(!condition)throw FormatError(message);}
template<class F>void rejects(F&& action,const char* message){bool rejected=false;try{action();}catch(const FormatError&){rejected=true;}require(rejected,message);}
struct Window {HWND handle{};~Window(){if(handle)DestroyWindow(handle);}};
// Negative-control receiver: never supplies fabricated successful uploads.
struct RejectBackend final:MaterialTextureBackend {
    unsigned calls{};
    [[noreturn]] void fail(){++calls;throw FormatError("deliberate material backend failure");}
    void release_all() override{fail();}
    std::uint8_t& current_transparency() override{fail();}
    void begin_pending_record(std::uint32_t) override{fail();}
    CompletedMaterialUpload upload(PreparedMaterialPixels) override{fail();}
    CompletedMaterialUpload generate(std::uint32_t) override{fail();}
};
struct FailFourthUpload final:MaterialTextureBackend {
    MaterialD3D12& backend;unsigned attempts{};
    explicit FailFourthUpload(MaterialD3D12& value):backend(value){}
    void release_all() override{backend.release_all();}
    std::uint8_t& current_transparency() override{return backend.current_transparency();}
    void begin_pending_record(std::uint32_t motion) override{backend.begin_pending_record(motion);}
    void before(){if(++attempts==4)throw FormatError("deliberate fourth material upload failure");}
    CompletedMaterialUpload upload(PreparedMaterialPixels pixels) override{before();return backend.upload(pixels);}
    CompletedMaterialUpload generate(std::uint32_t dimension) override{before();return backend.generate(dimension);}
};
WorldMaterialLoadingContext context_for(const RaceSession& session,const SourceGraphicsRoots& roots){
    WorldMaterialLoadingContext context;context.source_graphics_roots=&roots;context.scene=session.state().scene_mode;
    const auto& text=session.state().level.track_texture;const auto end=std::find(text.begin(),text.end(),'\0');
    require(end!=text.end(),"Session track texture missing terminator");context.track_prefix={text.begin(),end};
    context.quality=0;context.maximum_dimension=4096;
    // Explicit receiving fixture: retain all actual PC sizes up to4096; no
    // loading GUI. This is not the reconstructed default256/whole42E6E0.
    return context;
}
PolygonDeviceState modern_seed(){
    PolygonDeviceState state;
    // New platform profile. Full42FA80 writes every consumed render state.
    // The remaining TSS entries are explicit modern defaults, not measured
    //2001 driver semantics. One advertised stage avoids unresolved stage1
    //COLOROP-active/ALPHAOP-disabled legacy behavior.
    for(unsigned stage=0;stage<2;++stage){
        for(auto [index,value]:std::initializer_list<std::pair<unsigned,unsigned>>{
            {1,1},{2,2},{3,1},{4,1},{5,2},{6,1},{11,stage},{13,1},{14,1},{16,2},{17,2},{18,0}})
            state.stage[stage][index]=value;
        state.texture[stage]=0;
    }
    return state;
}
struct DeviceOwners {
    const std::uint32_t e4{1},e8{},f0{2},depth{1};
    const std::uint16_t texture_units{1},blend_stages{1};
    std::uint32_t flag68{},flag6c{},flag70{},multitexture{},flag78{},flagf0{};
    DeviceSetupDevice* device;DeviceSetupSurface* surface{};
    explicit DeviceOwners(PolygonDeviceContext& context):device(&context){
        require(initialize_retail_device({{e4,e8,f0,texture_units,blend_stages,device},
            {flag68,flag6c,flag70,multitexture,flag78,flagf0},device,surface})==1,"source42FA80 failed");
    }
};
void healthy(Renderer& renderer){renderer.finish();renderer.collect_diagnostics();renderer.check_health();
    for(const auto& item:renderer.diagnostics())std::cerr<<item.message<<'\n';
    require(renderer.stats().debug_active&&renderer.stats().debug_errors==0,"Session GPU debug warning/error");
}
session_archive::Frame save_capture(const CapturedPolygonFrame& frame,const PolygonDeviceState& initial,const std::filesystem::path& path){
    const auto bytes=session_archive::encode(frame,initial);
    {std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));require(bool(out),"Session capture archive write failed");}
    auto replay=session_archive::decode(read_file(path));require(session_archive::encode(replay.commands,replay.initial)==bytes,"Session archive roundtrip changed semantics");return replay;
}
}
int main(int argc,char** argv)try{
    const auto source_roots=source_graphics_roots_from_pe();
    require(argc==4||argc==5,"expected --warp|--hardware retail root and fresh output [--samples-1|--samples-4|--diagnostic-1x|--diagnostic-4x]");
    const std::string_view mode=argc==5?argv[4]:"--samples-4";
    const bool diagnostic=mode=="--diagnostic-1x"||mode=="--diagnostic-4x";
    const bool single_sample=mode=="--samples-1"||mode=="--diagnostic-1x";
    require(diagnostic||mode=="--samples-1"||mode=="--samples-4","unknown sample mode");
    const bool warp=std::string_view(argv[1])=="--warp";require(warp||std::string_view(argv[1])=="--hardware","unknown adapter");
    const auto output=std::filesystem::absolute(argv[3]);require(!std::filesystem::exists(output),"Session output already exists");std::filesystem::create_directories(output);
    WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"SGN-Session-Scene-Receiving";
    require(RegisterClassW(&wc)!=0,"Session window registration");Window window{CreateWindowExW(0,wc.lpszClassName,L"StuntGP NEW source scene receiving",WS_OVERLAPPEDWINDOW,0,0,1280,720,nullptr,nullptr,wc.hInstance,nullptr)};
    require(window.handle!=nullptr,"Session hidden window creation");Renderer renderer(window.handle,{true,warp,false,false,true,single_sample?1u:4u});
    std::cout<<"actual_samples="<<renderer.stats().samples<<'\n';
    require(renderer.stats().samples==(single_sample?1u:4u),"GPU did not satisfy requested sample count");
    RaceSessionLaunch launch;launch.master_gain=0;launch.sound_percent=launch.music_percent=0;launch.window=reinterpret_cast<std::uint64_t>(window.handle);
    unsigned guards{},frames{},status_frames{},draws{},materials_total{},screen_records{},source_frames{},empty_source_frames{};std::uint64_t payload{},replays{};
    const std::array levels{"TRACK1","TRACK7","TRACK11","TRACK24"};
    const std::array widths{1280,1600,1920,2560},heights{720,900,1080,1080};
    for(unsigned group=0;group<levels.size();++group){
        launch.level_name=levels[group];launch.model=group+1;launch.configuration=group+1;launch.width=widths[group];launch.height=heights[group];renderer.resize(widths[group],heights[group]);
        auto session=std::make_unique<RaceSession>(argv[2],launch);auto& s=session->state();
        auto material_owner=std::make_unique<SessionWorldMaterials>(*session);SessionWorldMaterials twin(*session);auto loading=context_for(*session,source_roots);
        RejectBackend refuse;
        rejects([&]{(void)material_owner->validate_for(*session);},"unloaded material owner accepted");++guards;
        rejects([&]{(void)material_owner->validate_fonts_for(*session);},"unloaded Session fonts accepted");++guards;
        auto wrong=loading;wrong.scene=0;rejects([&]{material_owner->load(wrong,refuse);},"wrong scene accepted");require(refuse.calls==0,"wrong scene reached backend");++guards;
        wrong=loading;wrong.track_prefix+="-invalid";rejects([&]{material_owner->load(wrong,refuse);},"wrong prefix accepted");require(refuse.calls==0,"wrong prefix reached backend");++guards;
        MaterialTransparencyStorage transparency;MaterialD3D12 backend(renderer,transparency,false,1+group*1000);backend.reset_transparency_430690();
        material_owner->load_with_fonts(loading,backend);materials_total+=backend.resource_entries();
        require(s.ui.fonts.count==2&&backend.resource_entries()==material_owner->state().names.materials.size()&&
            backend.resource_entries()==material_owner->state().names.physical_upload_count+4,"Session ordinary/font/GPU counts differ");
        require(material_owner->state().reload.font_cursor==2,"Session font reload records not in actual shared owner");
        rejects([&]{material_owner->load_with_fonts(loading,backend);},"repeated Session font entry accepted");++guards;
        rejects([&]{(void)material_owner->validate_for(*session);},"unpublished materials accepted");++guards;
        rejects([&]{(void)material_owner->validate_fonts_for(*session);},"unpublished Session fonts accepted");++guards;
        material_owner->publish_count_42e772();(void)material_owner->validate_fonts_for(*session);
        rejects([&]{twin.load(loading,refuse);},"second wrapper repeated cold relocation");require(refuse.calls==0,"second wrapper reached backend");++guards;
        bool foreign_rejected=false;std::thread foreign([&]{try{(void)material_owner->state();}catch(const FormatError&){foreign_rejected=true;}});foreign.join();require(foreign_rejected,"foreign material access accepted");++guards;
        {RaceSession other(argv[2],launch);rejects([&]{(void)material_owner->validate_for(other);},"world A material owner accepted for B");++guards;}
        MaterialTextureReplacement replacement(backend,material_owner->replacement_storage(*session));
        RaceOverlayTextures overlays(session->assets(),replacement,session->shared_platform());PlayableDriver driver(*session,overlays,{});
        PolygonDeviceContext device(modern_seed());DeviceOwners device_owners(device);
        PolygonSubmissionState scratch;scratch.depth_scale_bits=0x43000000; //pinned coldPE477CCC=128.0f
        contact::effects::EffectSpriteColorAudit color_audit;
        const float adjacent_word=std::bit_cast<float>(0x0048c5d8u);
        const std::array<contact::effects::EffectAngleSample,1> adjacent{{{10240,adjacent_word}}};
        SessionWorldFrameResources resources{{},adjacent,&color_audit,nullptr,{},desktop_polygon_submission_budget};
        ImmutablePolygonDeviceFrame first;std::shared_ptr<const PolygonGpuTextures> first_bank;
        {RaceSession other(argv[2],launch);const auto before_rng=other.state().random.state,before_stamp=other.state().polygon_frame.frame_stamp;
            const auto calls=device.snapshot().operation_sequence;
            rejects([&]{prepare_session_scene_frame(other,*material_owner,{device_owners.depth,device_owners.multitexture},scratch,device,resources,overlays);},"cross-world frame entry accepted");++guards;
            rejects([&]{submit_session_world_polygons(other,*material_owner,{device_owners.depth,device_owners.multitexture},scratch,device,desktop_polygon_submission_budget);},"cross-world submission entry accepted");++guards;
            require(before_rng==other.state().random.state&&before_stamp==other.state().polygon_frame.frame_stamp&&calls==device.snapshot().operation_sequence&&scratch.indexed_vertices.empty(),"cross-world rejection mutated source/sink/scratch");
        }
        unsigned manual_steps{};bool saw_startup{},saw_control_after_startup{};
        for(unsigned call=0;call<1800;++call){
            PlayableInput input;input.throttle=true;
            if(s.clock.phase==1)saw_startup=true;
            const auto player=s.controls.controllers[0].vehicle;const auto& race=s.vehicles.race[player];
            if(s.clock.phase==2&&race.control_enabled==1&&race.automatic_drive==0){
                ++manual_steps;input.turbo=manual_steps%600<120;input.right=manual_steps%600>=150&&manual_steps%600<180;
                if(saw_startup)saw_control_after_startup=true;
            }
            auto world=camera_frame_world(*session);camera_frame_begin_outer(world);driver.tick(input);
            // One real driver step and one composed4425D0 per accepted source
            //step. This receiving tests native composition and GPU replay;
            //full original-parent equivalence has its own source gate.
            const auto bank=backend.bindings_snapshot();
            device.begin_capture(s.simulation_tick,bank->epoch,{{20000,32u*1024u*1024u},30000});require(device.begin_scene()==0,"capture BeginScene");
            const auto stamp=s.polygon_frame.frame_stamp;
            const auto scene_result=prepare_session_scene_frame(*session,*material_owner,{device_owners.depth,device_owners.multitexture},scratch,device,resources,overlays);
            require(scene_result.kind==SessionSceneFrameKind::normal&&scene_result.camera.has_value(),"Session normal parent selected status");
            const auto& snapshot=*scene_result.camera;
            require(snapshot.controller()==s.controls.current&&s.polygon_frame.frame_stamp==((stamp+1u)&0xffffu),"Session frame camera/stamp mismatch");
            require(scratch.alpha_vertices.size()>=8&&s.ui.selected_material==0xffffffffu,"Session border/shared cache incomplete");
            screen_records+=scene_result.screen_records;
            require(s.screen_polygon_count==0,"source screen submission did not reset count");
            require(device.end_scene()==0,"capture EndScene");const auto captured=device.seal_capture();
            require(!captured->polygon_commands->commands.empty(),"Session world produced no commands");
            const auto frame_draws=std::count_if(captured->polygon_commands->commands.begin(),captured->polygon_commands->commands.end(),[](const auto& command){return std::holds_alternative<CapturedPolygonDraw>(command);});
            if(frame_draws==0)++empty_source_frames;draws+=static_cast<unsigned>(frame_draws);payload+=captured->polygon_commands->payload_bytes;++source_frames;
            if(!first&&frame_draws>0){first=captured;first_bank=bank;}
            if(call!=0&&call!=179&&call!=359&&call!=719&&call!=1799)continue;
            const auto fingerprint=session_archive::hash(*captured->polygon_commands,captured->initial.polygon);const auto rng=s.random.state,post_stamp=s.polygon_frame.frame_stamp,tick=s.simulation_tick;
            const auto ui_descriptor=s.ui.descriptor;
            const auto ui_stars=s.ui.stars.phase;
            const auto ui_font=s.ui.fonts.current,ui_cache=s.ui.selected_material;
            for(unsigned replay=0;replay<2;++replay){renderer.draw_polygons(*captured->polygon_commands,captured->initial.polygon,bank);++replays;}
            renderer.capture(output/(std::string(levels[group])+"-"+std::to_string(call)+".bmp"));
            const auto archived=save_capture(*captured->polygon_commands,captured->initial.polygon,output/(std::string(levels[group])+"-"+std::to_string(call)+".commands"));
            renderer.draw_polygons(archived.commands,archived.initial,bank);++replays;
            const auto second_path=output/(std::string(levels[group])+"-"+std::to_string(call)+"-archived.bmp");renderer.capture(second_path);
            if(diagnostic&&call==179){
                for(unsigned repeat=0;repeat<24;++repeat){
                    if(repeat%2)renderer.draw_polygons(archived.commands,archived.initial,bank);
                    else renderer.draw_polygons(*captured->polygon_commands,captured->initial.polygon,bank);
                    renderer.capture(output/("repeat-"+std::to_string(repeat)+".bmp"));healthy(renderer);
                }
                std::cout<<"diagnostic_only=1 samples="<<renderer.stats().samples<<" repeated_frames=24 no_full_receiving_claim\n";return 0;
            }
            require(read_file(second_path)==read_file(output/(std::string(levels[group])+"-"+std::to_string(call)+".bmp")),"archived frame GPU pixels differ from live capture");
            require(fingerprint==session_archive::hash(*captured->polygon_commands,captured->initial.polygon)&&rng==s.random.state&&post_stamp==s.polygon_frame.frame_stamp&&tick==s.simulation_tick,"GPU replay changed source frame/clock/RNG");
            require(ui_descriptor==s.ui.descriptor&&ui_stars==s.ui.stars.phase&&ui_font==s.ui.fonts.current&&ui_cache==s.ui.selected_material,"GPU replay changed source UI");
            std::cout<<levels[group]<<" call="<<call<<" tick="<<tick<<" phase="<<s.clock.phase<<" manual_steps="<<manual_steps<<" draws="<<frame_draws<<" screen_records="<<screen_records<<" camera="<<snapshot.camera().position.x<<','<<snapshot.camera().position.y<<','<<snapshot.camera().position.z<<'\n'<<std::flush;
            healthy(renderer);++frames;
        }
        require(first!=nullptr&&manual_steps>0&&saw_control_after_startup,"source startup did not reach a rendered controllable race");
        //Real loaded Session/font/material owners on both source status tails.
        //Their input edits are explicit fixtures; no additional driver tick.
        const auto saved_block=s.hud_block;const auto saved_current=s.controls.current;
        auto& status_control=s.controls.controllers[0];const auto saved_active=status_control.active;
        auto& status_word=s.vehicles.startup.at(status_control.vehicle).fields_768c[5];const auto saved_status=status_word;
        const std::vector<SourceScreenPolygon> saved_queue(s.screen_polygons.begin(),s.screen_polygons.end());
        const auto saved_queued=s.screen_polygon_count;require(saved_queued==0,"status fixture expects completed normal submission");
        SourceScreenPolygon queued_polygon{};queued_polygon.texture=0;queued_polygon.alpha=255;queued_polygon.vertex_count=4;
        for(unsigned i=0;i<4;++i){auto& v=queued_polygon.vertices[i];
            v.x=s.viewport.clip_screen[0]+((i==1||i==2)?24.0f:8.0f);
            v.y=s.viewport.clip_screen[2]+(i>=2?24.0f:8.0f);v.z=0.5f;
            v.retained_w=0x3f800000;v.u=(i==1||i==2)?1.0f:0.0f;v.v=i>=2?1.0f:0.0f;v.color=0xffffffff;
        }
        auto screen_access=lock_screen_polygon_owner(session->shared_platform());
        require(append_screen_polygon({live_screen_polygon_reader(screen_access),screen_access.count_word()},queued_polygon,s.viewport)&&s.screen_polygon_count==1,"actual425830 did not append status fixture");
        const std::vector<SourceScreenPolygon> retained_queue(s.screen_polygons.begin(),s.screen_polygons.end());
        s.hud_block=1;s.controls.current=0;status_control.active=0;
        for(unsigned status=1;status<=2;++status){
            status_word=status;const auto stamp=s.polygon_frame.frame_stamp,queued=s.screen_polygon_count,rng=s.random.state,tick=s.simulation_tick;
            const auto alpha=scratch.alpha_vertices;const auto descriptor=s.ui.descriptor;const auto phases=s.ui.stars.phase;
            const auto bank=backend.bindings_snapshot();
            device.begin_capture(tick,bank->epoch,{{20000,32u*1024u*1024u},30000});require(device.begin_scene()==0,"status BeginScene");
            const auto result=prepare_session_scene_frame(*session,*material_owner,{device_owners.depth,device_owners.multitexture},scratch,device,resources,overlays);
            require(!result.camera&&result.screen_records==0&&result.kind==(status==1?SessionSceneFrameKind::race_finished:SessionSceneFrameKind::no_player),"status tail dispatch differs");
            require(stamp==s.polygon_frame.frame_stamp&&queued==s.screen_polygon_count&&rng==s.random.state&&tick==s.simulation_tick,"status ran normal world or cleared screen queue");
            require(queued==1&&std::memcmp(retained_queue.data(),s.screen_polygons.data(),retained_queue.size()*sizeof(SourceScreenPolygon))==0,"status changed nonempty screen queue bytes");
            require(descriptor==s.ui.descriptor&&phases==s.ui.stars.phase&&alpha.size()==scratch.alpha_vertices.size()&&
                std::memcmp(alpha.data(),scratch.alpha_vertices.data(),alpha.size()*sizeof(PolygonSubmissionVertex))==0,"status changed shared HUD/alpha scratch");
            require(device.end_scene()==0,"status EndScene");const auto captured=device.seal_capture();
            const auto count=std::count_if(captured->polygon_commands->commands.begin(),captured->polygon_commands->commands.end(),[](const auto& command){return std::holds_alternative<CapturedPolygonDraw>(command);});
            require(count==(status==1?37:20),"status text emitted unexpected draw count");
            const auto stem=std::string(levels[group])+"-status-"+std::to_string(status);
            renderer.draw_polygons(*captured->polygon_commands,captured->initial.polygon,bank);++replays;renderer.capture(output/(stem+".bmp"));
            const auto restored=save_capture(*captured->polygon_commands,captured->initial.polygon,output/(stem+".commands"));
            renderer.draw_polygons(restored.commands,restored.initial,bank);++replays;renderer.capture(output/(stem+"-archived.bmp"));
            require(read_file(output/(stem+".bmp"))==read_file(output/(stem+"-archived.bmp")),"status archived GPU replay differs");
            healthy(renderer);++status_frames;
        }
        //Remove only the explicit caller fixture after both retained status calls.
        std::copy(saved_queue.begin(),saved_queue.end(),s.screen_polygons.begin());s.screen_polygon_count=saved_queued;
        status_word=saved_status;status_control.active=saved_active;s.hud_block=saved_block;s.controls.current=saved_current;
        const auto first_hash=session_archive::hash(*first->polygon_commands,first->initial.polygon);renderer.draw_polygons(*first->polygon_commands,first->initial.polygon,first_bank);++replays;healthy(renderer);
        require(session_archive::hash(*first->polygon_commands,first->initial.polygon)==first_hash,"retained capture changed after later source frames");
        for(unsigned corruption=0;corruption<4;++corruption){auto altered=*first->polygon_commands;auto initial=first->initial.polygon;
            auto draw=std::find_if(altered.commands.begin(),altered.commands.end(),[](const auto& c){return std::holds_alternative<CapturedPolygonDraw>(c);});
            auto& d=std::get<CapturedPolygonDraw>(*draw);
            if(corruption==0)++d.viewport.full_width;else if(corruption==1)d.indexed=!d.indexed;else if(corruption==2)++altered.resource_epoch;else initial.render[41]=1;
            require(session_archive::hash(altered,initial)!=first_hash,"semantic capture corruption escaped fingerprint");++guards;
        }
        if(group==0){
            const auto before=scratch;const auto calls=device.snapshot().operation_sequence;const auto rng=s.random.state,stamp=s.polygon_frame.frame_stamp;
            const auto* indexed=scratch.indexed_vertices.data();const auto* alpha=scratch.alpha_vertices.data();const auto* indices=scratch.indices.data();
            rejects([&]{submit_session_world_polygons(*session,*material_owner,{device_owners.depth,device_owners.multitexture},scratch,device,{0,0,0});},"zero submission budget accepted");++guards;
            const auto same=[]<class T>(const std::vector<T>& a,const std::vector<T>& b){return a.size()==b.size()&&(a.empty()||std::memcmp(a.data(),b.data(),a.size()*sizeof(T))==0);};
            require(same(before.indexed_vertices,scratch.indexed_vertices)&&same(before.alpha_vertices,scratch.alpha_vertices)&&same(before.indices,scratch.indices)&&indexed==scratch.indexed_vertices.data()&&alpha==scratch.alpha_vertices.data()&&indices==scratch.indices.data()&&
                before.depth_scale_bits==scratch.depth_scale_bits&&before.specular_enabled==scratch.specular_enabled&&before.depth_enabled==scratch.depth_enabled&&before.depth_write==scratch.depth_write&&before.depth_other==scratch.depth_other&&calls==device.snapshot().operation_sequence&&rng==s.random.state&&stamp==s.polygon_frame.frame_stamp,"capacity failure changed scratch/source/sink");
            rejects([&]{prepare_session_scene_frame(*session,*material_owner,{device_owners.depth,device_owners.multitexture},scratch,device,resources,overlays);},"failed submission world retried");++guards;
            attach_world_polygon_level(s.polygon_frame);
        }
        if(group==1){
            const auto sky=session->scene_resources().model_handle(2);require(sky.has_value(),"binding guard sky absent");
            const std::array request{LevelAssetRequest{std::string(session->models().at(*sky).source_name()),2}};
            (void)session->scene_resources().bind_models(request,s.selection,s.cars,loading.track_prefix);
            const auto calls=device.snapshot().operation_sequence;const auto rng=s.random.state,stamp=s.polygon_frame.frame_stamp;
            rejects([&]{(void)material_owner->validate_for(*session);},"changed model binding accepted");++guards;
            rejects([&]{prepare_session_scene_frame(*session,*material_owner,{device_owners.depth,device_owners.multitexture},scratch,device,resources,overlays);},"frame accepted changed model binding");++guards;
            rejects([&]{submit_session_world_polygons(*session,*material_owner,{device_owners.depth,device_owners.multitexture},scratch,device,desktop_polygon_submission_budget);},"submission accepted changed model binding");++guards;
            require(calls==device.snapshot().operation_sequence&&rng==s.random.state&&stamp==s.polygon_frame.frame_stamp,"binding rejection changed source/sink");
        }
        material_owner.reset();SessionWorldMaterials recreated(*session);rejects([&]{recreated.load(loading,refuse);},"destroyed wrapper reset cold reservation");require(refuse.calls==0,"recreated wrapper reached backend");++guards;
        detach_world_polygon_level(s.polygon_frame);attach_world_polygon_level(s.polygon_frame);
        SessionWorldMaterials reattached(*session);rejects([&]{reattached.load(loading,refuse);},"reattach reset cold reservation");require(refuse.calls==0,"reattached wrapper reached backend");++guards;
        allocate_world_polygon_frame(s.polygon_frame);attach_world_polygon_level(s.polygon_frame);
        auto expired=std::make_unique<SessionWorldMaterials>(*session);rejects([&]{expired->load(loading,refuse);},"frame reallocation reset cold reservation");++guards;
        const auto retained_platform=session->shared_platform();session.reset();rejects([&]{(void)expired->state();},"retained platform kept destroyed level valid");++guards;
        renderer.draw_polygons(*first->polygon_commands,first->initial.polygon,first_bank);++replays;healthy(renderer);
        std::cout<<levels[group]<<" real Session/material/source capture/GPU passed\n";
    }
    {RaceSession session(argv[2],launch);SessionWorldMaterials owner(session);auto loading=context_for(session,source_roots);RejectBackend backend;
        rejects([&]{owner.load(loading,backend);},"deliberate source backend failure accepted");require(backend.calls==1,"failed load ran beyond backend failure");++guards;
        rejects([&]{(void)owner.state();},"failed material world remained usable");++guards;
        attach_world_polygon_level(session.state().polygon_frame);SessionWorldMaterials retry(session);rejects([&]{retry.load(loading,backend);},"failed reservation reset by reattach");require(backend.calls==1,"failed reservation reached backend twice");++guards;
    }
    {RaceSession session(argv[2],launch);SessionWorldMaterials owner(session);const auto& partial=owner.state();auto loading=context_for(session,source_roots);
        MaterialTransparencyStorage transparency;MaterialD3D12 backend(renderer,transparency,false,10001);backend.reset_transparency_430690();FailFourthUpload failure(backend);
        rejects([&]{owner.load(loading,failure);},"mid-load material failure accepted");require(failure.attempts==4&&partial.names.materials.size()==3&&backend.resource_entries()==3,"mid-load failure lost completed source/GPU prefix");++guards;
        rejects([&]{(void)owner.state();},"mid-load failed owner remained valid");++guards;
        attach_world_polygon_level(session.state().polygon_frame);SessionWorldMaterials retry(session);RejectBackend refuse;rejects([&]{retry.load(loading,refuse);},"mid-load failure reservation reset");require(refuse.calls==0,"mid-load retry reached backend");++guards;
        healthy(renderer);
    }
    {RaceSession session(argv[2],launch);SessionWorldMaterials owner(session);auto loading=context_for(session,source_roots);
        MaterialTransparencyStorage transparency;MaterialD3D12 backend(renderer,transparency,false,11001);backend.reset_transparency_430690();
        owner.load(loading,backend);owner.publish_count_42e772();(void)owner.validate_for(session);
        rejects([&]{(void)owner.validate_fonts_for(session);},"material-only published Session falsely has initialized fonts");++guards;
        healthy(renderer);
    }
    {RaceSession session(argv[2],launch);SessionWorldMaterials owner(session);const auto& partial=owner.state();auto loading=context_for(session,source_roots);
        MaterialTransparencyStorage transparency;MaterialD3D12 backend(renderer,transparency,false,12001);backend.reset_transparency_430690();
        //Explicit host-invalid retained font state: real world uploads finish,
        //then the actual font initializer rejects its incoming glyph index.
        session.state().ui.fonts.glyph_map[32]=255;
        rejects([&]{owner.load_with_fonts(loading,backend);},"font failure accepted");++guards;
        require(partial.names.physical_upload_count>0&&partial.names.materials.size()==partial.names.physical_upload_count&&
            backend.resource_entries()==partial.names.materials.size()&&partial.reload.font_cursor==0,"font failure lost ordinary completed prefix");
        rejects([&]{(void)owner.state();},"font failure left Session owner usable");++guards;
        rejects([&]{(void)owner.validate_fonts_for(session);},"failed fonts validated");++guards;
        attach_world_polygon_level(session.state().polygon_frame);SessionWorldMaterials retry(session);RejectBackend refuse;
        rejects([&]{retry.load(loading,refuse);},"font failure released cold reservation");require(refuse.calls==0,"font failure retry reached backend");++guards;
        healthy(renderer);
    }
    require(screen_records>0,"no actual Session producer exercised the screen queue");
    healthy(renderer);std::cout<<"passed=1 adapter="<<(warp?"WARP":"hardware")<<" sessions=4 frames="<<frames<<" status_frames="<<status_frames<<" source_frames="<<source_frames<<" empty_source_frames="<<empty_source_frames<<" replay_presents="<<replays<<" draws="<<draws<<" materials="<<materials_total<<" payload_bytes="<<payload<<" screen_records="<<screen_records<<" guards="<<guards<<" debug_warnings_errors=0\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
