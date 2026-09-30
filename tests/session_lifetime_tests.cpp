#include "sgn/race_session.hpp"
#include "sgn/race_session_music.hpp"
#include "sgn/retail_frontend_state.hpp"
#include "sgn/retail_world_polygon_producer.hpp"
#include "sgn/retail_camera_frame_session.hpp"
#include "sgn/contact_rail.hpp"
#include <atomic>
#include <bit>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <thread>
#include <windows.h>

using namespace sgn;using namespace sgn::retail;
namespace {
const RaceSessionState* terminating_world{};
AudioStreamState terminating_stream;
void expected_termination() noexcept {
    //No message pump is active on either thread. Verify the rejected close
    //failed before source streaming writes, while the retained owner is alive.
    if(terminating_world&&std::memcmp(&terminating_world->audio_stream,&terminating_stream,sizeof(terminating_stream))!=0)
        std::_Exit(74);
    std::_Exit(73);
}
void require(bool ok,const char* message){if(!ok)throw FormatError(message);}
void pump(unsigned milliseconds){
    const auto end=std::chrono::steady_clock::now()+std::chrono::milliseconds(milliseconds);
    do{MSG message{};while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }while(std::chrono::steady_clock::now()<end);
}
unsigned callback_windows(){
    unsigned count=0;HWND previous{};
    while((previous=FindWindowExW(HWND_MESSAGE,previous,L"StuntGPNewMusicCallbacks",nullptr))!=nullptr){
        DWORD process{};GetWindowThreadProcessId(previous,&process);if(process==GetCurrentProcessId())++count;}
    return count;
}
void publish_actual_model_borrows(RaceSession& session){
    auto& frame=session.state().polygon_frame;const auto track=session.scene_resources().model_handle(3);
    require(track.has_value(),"real level lacks a track model");
    auto& model=session.models().at(*track);const auto* spatial=model.source_spatial();
    require(spatial&&!spatial->polygon_indices.empty()&&!spatial->vertex_indices.empty(),"real level lacks authored spatial lists");
    frame.spatial[0].records[0]={spatial->polygon_indices,spatial->vertex_indices};frame.spatial[0].count=1;
    // Actual qualified429D50 publishes live PMD header/coordinate/corner views.
    // This lifetime fixture does not claim camera geometry or a rendered frame.
    reset_world_polygon_groups(frame); // Explicit source42B100 before the fixture emitter.
    begin_world_polygon_group(frame,0);
    const auto source=world_polygon_part_source(model,model.source_lod_index(0),0);
    emit_world_polygons_unclipped(source,frame.allocations->vertices,{0,0,10000,0},frame.packets[0],frame.packets[1]);
    finish_world_polygon_group(frame,0);publish_world_polygon_totals(frame);
    require(frame.packets[0].count()+frame.packets[1].count()>0,"real polygon emitter published no borrowed packets");
    const auto lists=world_polygon_submission_lists(frame);
    bool found=false;for(const auto list:lists)for(const auto& packet:list)if(packet.header){
        require(packet.attributes.bytes().data()==source.attributes.bytes().data(),"packet copied authored attribute ownership");
        require(source.headers.find(packet.header).has_value(),"packet header is not actual ModelResource storage");
        (void)packet.attributes.value(0);found=true;}
    require(found,"published borrowed packets are absent from source submission views");
}
void death_case(const std::filesystem::path& root,bool release_session){
    std::set_terminate(expected_termination);
    RaceSessionLaunch launch;launch.master_gain=0;
    auto session=std::make_unique<RaceSession>(root,launch);
    auto world=session->shared_state();auto platform=session->shared_platform();
    if(release_session){
        start_audio_music(audio_stream_world(*session),world->music,2,1);
        require(world->audio_stream.allocated!=0,"foreign-session test needs a real stream");
        terminating_world=world.get();std::memcpy(&terminating_stream,&world->audio_stream,sizeof(terminating_stream));
        // The installed Windows UCRT keeps its terminate handler in per-thread
        //data; install the test observer on the actual failing thread as well.
        std::thread worker([old=std::move(session)]()mutable{std::set_terminate(expected_termination);old.reset();});worker.join();
    }else{
        session.reset();world.reset();
        std::thread worker([last=std::move(platform)]()mutable{std::set_terminate(expected_termination);last.reset();});worker.join();
    }
    throw FormatError("foreign-thread finalization escaped the required guard");
}
struct ThrowingStartupObserver final:StartupObserver {
    bool before{},had_spline{};
    std::shared_ptr<RaceSessionState> world;
    std::shared_ptr<RaceSessionPlatform> platform;
    explicit ThrowingStartupObserver(bool early):before(early){}
    void capture_and_throw(const RaceSession& session){
        world=session.shared_state();platform=session.shared_platform();
        had_spline=world->grid.start_spline.has_value();
        throw FormatError("injected startup observer failure");
    }
    void before_race_initialization(const RaceSession& session)override{if(before)capture_and_throw(session);}
    void after_race_initialization(const RaceSession& session,const RaceInitializationObservation&)override{if(!before)capture_and_throw(session);}
};
unsigned failed_startup_cases(const std::filesystem::path& root){
    unsigned checks=0;const auto check=[&](bool ok,const char* message){require(ok,message);++checks;};
    for(const bool early:{true,false}){
        const auto baseline=callback_windows();ThrowingStartupObserver observer(early);
        RaceSessionLaunch launch;launch.master_gain=0;launch.startup_observer=&observer;bool failed=false;
        try{RaceSession session(root,launch);}catch(const FormatError& error){failed=std::string_view(error.what())=="injected startup observer failure";}
        check(failed&&observer.world&&observer.platform,"startup observer did not retain the failing constructor's actual owners");
        check(observer.had_spline,"constructor failure did not occur after actual spline binding");
        auto& world=*observer.world;auto& platform=*observer.platform;platform.require_owner_thread();
        check(!world.grid.start_spline&&!world.rail.spline&&!world.automatic.routes,"constructor unwind retained released spline storage");
        for(const auto& camera:world.camera_presentation)check(!camera.spline,"constructor unwind retained a camera spline");
        bool detached=false;try{(void)world_polygon_submission_lists(world.polygon_frame);}catch(const FormatError&){detached=true;}
        check(detached,"failed constructor left a usable model lease");
        check(!world.audio_stream.allocated&&platform.audio_stream_platform().live_files()==0&&platform.audio_stream_platform().live_timers()==0,
            "constructor unwind left native music resources running");
        //40D500 selects a track but has not opened it yet at this hook. Verify
        //the surviving real catalog/device by opening it after the failure.
        start_audio_music(audio_stream_world(platform),world.music,2,1);pump(80);
        check(world.audio_stream.allocated&&platform.audio_device().inspect(world.audio_maintenance.music.buffer.borrowed()).samples_played>0,
            "retained platform cannot consume real music after constructor failure");
        close_audio_stream(world.audio_stream,world.audio_maintenance.music,platform.audio_device(),platform.audio_stream_platform());
        AudioStreamState closed;std::memcpy(&closed,&world.audio_stream,sizeof(closed));
        close_audio_stream(world.audio_stream,world.audio_maintenance.music,platform.audio_device(),platform.audio_stream_platform());
        close_audio_stream(world.audio_stream,world.audio_maintenance.music,platform.audio_device(),platform.audio_stream_platform());
        check(std::memcmp(&closed,&world.audio_stream,sizeof(closed))==0,"repeated cleanup changed an already closed source stream");
        std::weak_ptr<RaceSessionState> weak=observer.world;observer.world.reset();observer.platform.reset();
        pump(20);check(weak.expired()&&callback_windows()==baseline,"failed constructor leaked the final platform owner");
    }
    return checks;
}
unsigned model_descriptor_binding_cases(const std::filesystem::path& root){
    unsigned checks=0;const auto check=[&](bool ok,const char* message){require(ok,message);++checks;};
    const auto baseline=callback_windows();RaceSessionLaunch launch;launch.master_gain=0;launch.mode=SessionMode::normal_race;
    auto session=std::make_unique<RaceSession>(root,launch);auto world=session->shared_state();
    for(unsigned i=0;i<world->sprite_packets.indices.size();++i)
        check(world->sprite_packets.indices[i]==i,"normal-race startup missed original434090 sprite index sequence");
    std::array<const std::uint32_t*,6> owners{};
    const auto word=[](std::span<const std::uint8_t> raw,std::size_t offset){
        require(offset<=raw.size()&&raw.size()-offset>=4,"descriptor test PMD range");
        std::uint32_t value{};std::memcpy(&value,raw.data()+offset,4);return value;
    };
    for(unsigned slot=0;slot<6;++slot){
        const auto handle=session->models().resolve(world->cars[slot].source_word00);
        check(handle&&world->vehicles.models[slot].model==handle,"normal race car descriptor lacks its actual physical-slot model");
        auto& model=session->models().at(*handle);const ModelResource& readonly=model;
        owners[slot]=&model.current_detail_level();
        check(owners[slot]==&world->selected_lod[slot]&&&readonly.current_detail_level()==owners[slot],"car descriptor5C duplicated the canonical Session LOD");
        const auto raw=read_file(root/"meshdata"/(std::string(model.source_name())+".PMD"));
        const auto descriptor=std::size_t{328}+word(raw,0x4c);const auto imported=word(raw,descriptor+0x5c);
        check(world->selected_lod[slot]==imported,"initial car LOD was not transferred from its actual PMD descriptor");
        const auto retained=world->selected_lod[slot];world->selected_lod[slot]=3;
        check(model.current_detail_level()==3,"model missed a write from the Session LOD owner");
        model.current_detail_level()=1;check(world->selected_lod[slot]==1,"Session missed a write from the model descriptor");
        model.bind_current_detail_level(world->selected_lod[slot]);
        check(model.current_detail_level()==1&&&model.current_detail_level()==owners[slot],"same-owner binding reset a live LOD");
        std::uint32_t foreign=0xa1362748u;bool rejected=false;
        try{model.bind_current_detail_level(foreign);}catch(const FormatError&){rejected=true;}
        check(rejected&&foreign==0xa1362748u&&model.current_detail_level()==1,"cross-owner LOD rebind was not atomic");
        world->selected_lod[slot]=retained;
    }
    const auto track=session->scene_resources().model_handle(3);check(track.has_value(),"descriptor test lacks actual track");
    auto& track_model=session->models().at(*track);auto& pose=world->model_draws.template_pose;
    check(track_model.source_outer_transform()==&pose,"track descriptor0C does not borrow sole4864E8");
    check(world->polygon_frame.low_quality_initialized==0,"level initialization missed source57B41C reset");
    const auto before_pose=pose;pose.position.x=173;pose.tail[3]=0x31a9c6e2u;
    check(track_model.source_outer_transform()->position.x==173&&track_model.source_outer_transform()->tail[3]==0x31a9c6e2u,"track template was snapshotted instead of borrowed");
    pose=before_pose;
    auto camera_world=camera_frame_world(*session);
    check(&camera_world.template_pose==&pose,"camera frame does not borrow the sole live4864E8 owner");
    check(world->camera_bank.count>0,"camera binding test lacks actual authored retail records");
    auto& camera=world->controls.controllers[0].camera;auto& lifecycle=world->camera_lifecycle[0];
    const auto saved_camera=camera;const auto saved_lifecycle=lifecycle;
    const auto observed=world->vehicles.camera_snapshot(0);
    // The invocation view predates both writes. Repeated source record apply
    // must observe the same owner, including opaque W and tail identities.
    for(unsigned pass=0;pass<2;++pass){
        const auto value=static_cast<float>(pass);
        pose.scale={2.0f+value,3.0f+value,4.0f+value};pose.scale_w=0x7fc12345u+pass;
        pose.tail={0x12340000u+pass,0x43210000u+pass,0x56780000u+pass,0x87650000u+pass};
        camera_request_record(lifecycle,0,1);
        camera_apply_record(camera,lifecycle,observed,world->camera_bank,camera_world.template_pose,world->shared_model_identity);
        check(lifecycle.scale.x==pose.scale.x&&lifecycle.scale.y==pose.scale.y&&lifecycle.scale.z==pose.scale.z
            &&std::bit_cast<std::uint32_t>(lifecycle.scale.w)==pose.scale_w,"record apply missed live template scale/W");
        check(lifecycle.transform_word_48==pose.tail[0]&&lifecycle.transform_word==pose.tail[3],"record apply missed live template tail words");
        check(camera.world_to_camera_reference==pose.tail[1]&&camera.camera_to_world_reference==pose.tail[2],"record apply replaced template matrix identities");
    }
    camera=saved_camera;lifecycle=saved_lifecycle;pose=before_pose;
    // New resource registration must preserve all earlier deque-backed model
    // references and the six Session-owned current LOD words.
    const auto first=session->models().resolve(world->cars[0].source_word00);
    const auto name=std::string(session->models().at(*first).source_name());
    (void)session->models().load(name,std::string_view(world->level.track_texture.data()));
    for(unsigned slot=0;slot<6;++slot){const auto handle=session->models().resolve(world->cars[slot].source_word00);
        check(&session->models().at(*handle).current_detail_level()==owners[slot],"resource growth moved a canonical car LOD reference");}
    check(track_model.source_outer_transform()==&pose,"resource growth replaced the live track template owner");
    const auto lods=world->selected_lod;session.reset();
    check(world->selected_lod==lods&&std::memcmp(&world->model_draws.template_pose,&before_pose,sizeof(before_pose))==0,"model destruction mutated surviving world descriptor owners");
    world.reset();pump(20);check(callback_windows()==baseline,"descriptor binding test leaked its native platform");
    return checks;
}
}
int main(int argc,char** argv)try{
    if(argc<2||argc>3)return 2;
    if(argc==3){death_case(argv[1],std::string_view(argv[2])=="--foreign-session");return 3;}
    const auto baseline=callback_windows();unsigned checks=0,cancelled_callbacks=0;
    checks+=model_descriptor_binding_cases(argv[1]);
    checks+=failed_startup_cases(argv[1]);
    const auto check=[&](bool ok,const char* message){require(ok,message);++checks;};
    RaceSessionLaunch launch;launch.master_gain=0;
    auto session=std::make_unique<RaceSession>(argv[1],launch);
    auto alias=session->shared_state();auto platform=session->shared_platform();
    std::weak_ptr<RaceSessionState> weak_world=alias;std::weak_ptr<RaceSessionPlatform> weak_platform=platform;
    auto frontend=std::make_unique<FrontendState>(alias,platform);
    check(&frontend->world()==&session->state()&&&platform->state()==alias.get(),"world copied while attaching frontend");
    check(!alias.owner_before(platform)&&!platform.owner_before(alias),"world alias uses a different control block");
    check(callback_windows()==baseline+1,"one persistent platform must own one callback window");
    std::atomic<bool> owner_guard{};
    std::thread wrong_thread([&]{try{platform->require_owner_thread();}catch(const std::logic_error&){owner_guard=true;}});wrong_thread.join();
    check(owner_guard,"normal owner-thread violation was not rejected");
    bool wrong_alias=false;
    auto unrelated_world=std::make_unique<RaceSessionState>();
    try{FrontendState invalid(std::shared_ptr<RaceSessionState>{platform,unrelated_world.get()},platform);}
    catch(const FormatError&){wrong_alias=true;}check(wrong_alias,"same-controlblock wrong world pointer accepted");
    bool wrong_owner=false;
    auto other_control=std::shared_ptr<RaceSessionState>(alias.get(),[](RaceSessionState*){});
    try{FrontendState invalid(other_control,platform);}catch(const FormatError&){wrong_owner=true;}
    check(wrong_owner,"same pointer with unrelated controlblock accepted");other_control.reset();unrelated_world.reset();
    auto& world=*alias;auto& stream=platform->audio_stream_platform();auto& device=platform->audio_device();
    for(unsigned i=0;i<world.sprite_packets.indices.size();++i)
        check(world.sprite_packets.indices[i]==i,"time-trial startup missed original434090 sprite index sequence");
    const auto sprite_startup_indices=world.sprite_packets.indices;
    check(world.track_part_animations&&world.track_part_animations->records().size()==32,"Session did not retain all32 source animation slots");
    const auto* animation_owner=world.track_part_animations->records().data();
    check(world.grid.start_spline.has_value(),"real Session did not retain its starting-grid spline");
    const DecodedTrackSplines track_splines{session->splines(),session->raw_texture_labels(),session->track_pmd()}; //R18
    auto automatic_cars=world.vehicles.automatic_views();initialize_ai_automatic(track_splines,automatic_cars,world.automatic);
    check(bool(world.automatic.routes),"actual AI initialization did not bind its three routes");
    initialize_rail(world.rail,session->splines(),session->raw_texture_labels());
    check(world.rail.spline.has_value(),"actual rail initialization did not bind its authored spline");
    const auto ai_initialized=world.automatic.initialized;const auto ai_distances=world.automatic.route_distances;
    const auto* frame_allocation=world.polygon_frame.allocations.get();
    check(frame_allocation!=nullptr,"Session did not allocate the actual polygon frame owner");
    publish_actual_model_borrows(*session);const auto frame_ticket=capture_world_polygon_level(world.polygon_frame);
    const auto frame_totals=world.polygon_frame.totals;const auto spatial_count=world.polygon_frame.spatial[0].count;
    auto rejects=[&](auto action,const char* reason){bool rejected=false;try{action();}catch(const FormatError&){rejected=true;}check(rejected,reason);};
    rejects([&]{attach_world_polygon_level(world.polygon_frame);},"double level attachment was accepted");
    std::atomic<bool> borrow_thread_guard{};
    std::thread foreign_borrow([&]{try{validate_world_polygon_level(frame_ticket);}catch(const FormatError&){borrow_thread_guard=true;}});foreign_borrow.join();
    check(borrow_thread_guard,"foreign-thread model borrow was accepted");
    const auto catalog_buffer=world.audio_driver.samples[1].buffer;
    const auto before_camera=world.controls.controllers[0].camera.position;
    bool had_spline=false;for(const auto& camera:world.camera_presentation)had_spline|=camera.spline.has_value();
    check(had_spline,"real Session did not bind a level camera spline");
    start_audio_music(audio_stream_world(*session),world.music,2,1);pump(450);stream.check_health();
    check(world.audio_stream.allocated&&stream.live_files()==1&&stream.live_timers()==1,"actual source stream did not start");
    check(device.inspect(world.audio_maintenance.music.buffer.borrowed()).samples_played>0,"source stream was not consumed by native mixer");
    stream.start_open_worker([&]{++cancelled_callbacks;world.random.state=0;});
    session.reset();
    check(!weak_world.expired()&&!weak_platform.expired()&&&frontend->world()==alias.get(),"Session destruction killed retained world/platform");
    check(world.track_part_animations->records().data()==animation_owner,"level destruction replaced the persistent animation state owner");
    check(world.sprite_packets.indices==sprite_startup_indices,"level destruction changed persistent sprite indices");
    check(world.polygon_frame.allocations.get()==frame_allocation,"Session detach replaced the retained polygon frame owner");
    check(world.polygon_frame.totals==frame_totals&&world.polygon_frame.spatial[0].count==spatial_count,"host detach changed source packet or spatial counts");
    rejects([&]{validate_world_polygon_level(frame_ticket);},"detached packet ticket remained valid");
    rejects([&]{(void)world_polygon_submission_lists(world.polygon_frame);},"detached submission exposed released model references");
    rejects([&]{(void)world_polygon_clip_lists(world.polygon_frame);},"detached clip views exposed released model references");
    rejects([&]{(void)world_polygon_remaining_vertices(world.polygon_frame);},"detached projection view was accepted");
    // Replace source allocation before new host attachment. This is a lease
    //generation test, not a reconstructed race/level reattachment.
    allocate_world_polygon_frame(world.polygon_frame);attach_world_polygon_level(world.polygon_frame);
    const auto replacement_ticket=capture_world_polygon_level(world.polygon_frame);
    validate_world_polygon_level(world.polygon_frame,replacement_ticket);
    rejects([&]{validate_world_polygon_level(frame_ticket);},"old model ticket became valid after a new attachment");
    WorldPolygonFrame different_frame;allocate_world_polygon_frame(different_frame);attach_world_polygon_level(different_frame);
    rejects([&]{validate_world_polygon_level(different_frame,replacement_ticket);},"ticket from another frame was accepted");
    check(!world.audio_stream.allocated&&stream.live_files()==0&&stream.live_timers()==0,"legacy Session destruction leaked live music");
    for(const auto& camera:world.camera_presentation)check(!camera.spline,"per-level spline view survived destruction of its storage");
    check(!world.grid.start_spline&&!world.rail.spline&&!world.automatic.routes,"retained grid/rail/AI spline views survived level destruction");
    check(world.automatic.initialized==ai_initialized&&world.automatic.route_distances==ai_distances,"host spline invalidation changed source AI numeric state");
    check(std::memcmp(&before_camera,&world.controls.controllers[0].camera.position,sizeof(before_camera))==0,"host detach changed retail camera position");
    unsigned status{};check(device.status(catalog_buffer,status)==0,"retained catalog audio identity died with the level");
    pump(30);check(cancelled_callbacks==0,"queued callback ran after Session detach");
    // Source music can use the retained platform without fabricating a second
    // Session or reinitializing the catalog. This is not a race-reattach test.
    start_audio_music(audio_stream_world(*platform),world.music,21,1);pump(450);stream.check_health();
    check(world.audio_stream.allocated&&stream.live_files()==1&&stream.live_timers()==1,"retained platform failed to reopen real source music");
    const auto samples=device.inspect(world.audio_maintenance.music.buffer.borrowed()).samples_played;
    check(samples>0,"retained platform did not consume reopened music");
    frontend.reset();platform.reset();
    check(!weak_platform.expired()&&!weak_world.expired(),"last world alias did not retain the audio owner");
    stream.start_open_worker([&]{++cancelled_callbacks;});
    const auto timer=stream.start_timer(1,1,[&]{++cancelled_callbacks;},1);
    check(timer!=0,"test callback timer was not created");
    // Await the real queue precondition. A fixed sleep is not evidence that
    // Windows has generated a due timer. Do not remove or dispatch callbacks.
    MSG pending{};bool has_pending_timer=false;unsigned pending_timer_probes=0;
    const auto timer_deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    do{
        ++pending_timer_probes;
        has_pending_timer=PeekMessageW(&pending,nullptr,WM_TIMER,WM_TIMER,PM_NOREMOVE)!=0&&pending.message==WM_TIMER;
        if(!has_pending_timer)std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }while(!has_pending_timer&&std::chrono::steady_clock::now()<timer_deadline);
    check(has_pending_timer,"final teardown lacks an actually pending callback");
    DWORD pending_process{};std::array<wchar_t,64> pending_class{};
    check(GetWindowThreadProcessId(pending.hwnd,&pending_process)==GetCurrentThreadId()&&pending_process==GetCurrentProcessId()
        &&GetClassNameW(pending.hwnd,pending_class.data(),static_cast<int>(pending_class.size()))>0
        &&std::wstring_view(pending_class.data())==L"StuntGPNewMusicCallbacks"
        &&(pending.wParam==timer||pending.wParam==world.audio_stream.timer),"pending timer does not belong to the retained music platform");
    check(cancelled_callbacks==0,"callback ran while awaiting final teardown precondition");
    alias.reset(); //final owner-thread close while the source stream is active
    check(weak_platform.expired()&&weak_world.expired(),"world/platform lifetime cycle leaked the final owner");
    rejects([&]{validate_world_polygon_level(replacement_ticket);},"weak ticket outlived the destroyed world owner");
    pump(30);check(cancelled_callbacks==0,"queued callback ran after final owner destruction");
    check(callback_windows()==baseline,"final platform release left a native callback window");
    std::cout<<"{\"passed\":true,\"checks\":"<<checks<<",\"reopened_samples_consumed\":"<<samples
        <<",\"pending_timer_probes\":"<<pending_timer_probes<<",\"retained_world_copies\":0,\"cancelled_callbacks_executed\":"<<cancelled_callbacks
        <<",\"callback_windows_after\":"<<callback_windows()<<",\"constructor_failure_cases\":2,\"canonical_car_lod_bindings\":6,\"live_track_template_binding\":true,\"startup_sprite_indices_checked\":4096,\"camera_live_template_applications\":2,\"actual_model_packet_and_spatial_borrows\":true,\"lease_replacement_tested\":true,\"source_race_reattach_tested\":false}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
