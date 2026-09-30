#include "source_scene_reset_witness.hpp"
#include "sgn/source_scene_reset_composition.hpp"
#include "sgn/source_outer_scheduler.hpp"
#include "sgn/race_session_scene.hpp"
#include "sgn/race_session_music.hpp"
#include "sgn/race_overlay_textures.hpp"
#include "sgn/source_allocation_bank.hpp"
#include <iostream>
#include <thread>

namespace {
using namespace sgn;
using namespace sgn::retail;
using source_scene_reset_receiving::SceneResetWitness;
using source_scene_reset_receiving::bytes;
unsigned checks{},reads{},two_reset_cases{},fault_cases{},initialize_cases{},wrong_threads{},missing_cases{},unhandled_cases{};
void check(bool value,const char* reason){++checks;if(!value)throw std::runtime_error(reason);}
using Words=std::array<std::uint32_t,6>;
Words clock_words(const RaceSessionState& s){return {s.tick_clock.scheduled,s.tick_clock.observed,s.tick_clock.increment,
    s.source_scheduler.frequency,s.source_scheduler.fixed_mode_a,s.source_scheduler.fixed_mode_b};}
constexpr Words initial_clock{0x11111111u,0x22222222u,0x33333333u,0x44444444u,0x55555555u,0x66666666u};
using Values=std::array<std::uint64_t,4>;
constexpr std::array<Values,3> values{{
    {0x12345678ffffffffull,0x2345678900000000ull,0x3456789a80000000ull,0x456789ab00000007ull},
    {0x1111111100000064ull,0x2222222200000003ull,0x33333333fffffffeull,0x444444447fffffffull},
    {0xffffffff00000000ull,0x0000000080000000ull,0xabcdef0100000000ull,0x12345678ffffffffull}
}};
Words after_reads(Values sample,unsigned successful){
    auto result=initial_clock;
    for(unsigned i=0;i<successful;++i)result[i%2]=static_cast<std::uint32_t>(sample[i]);
    return result;
}
struct Device final:TextureReplacementDevice {
    std::uint16_t width{};unsigned uploads{},width_reads{};bool unavailable{};
    std::uint16_t destination_width(std::uint32_t slot) const override{
        if(unavailable||slot!=15u)throw FormatError("composition touched released texture device");
        ++const_cast<Device*>(this)->width_reads;return width;
    }
    void upload_to_destination(std::uint32_t slot,const Image&) override{
        if(unavailable||slot!=15u)throw FormatError("composition uploaded to released texture device");++uploads;
    }
};
void seed(RaceSessionState& s,const SplineView& spline){
    s.frontend_launch.catalog_count=91;
    for(unsigned i=0;i<8;++i){
        s.vehicles.models[i].model=static_cast<std::size_t>(0x1234567800000000ull+i);
        s.vehicles.models[i].shadow=static_cast<std::size_t>(0x8765432100000000ull+i);
        s.vehicles.models[i].field_2708=41+i;s.vehicles.presentation[i].field_3134=43+i;
    }
    for(unsigned i=0;i<6;++i){
        s.vehicles.race[i].active=i%3;s.vehicles.race[i].lap_index=17+i;
        s.vehicles.parameters[i].fuel_capacity=100.0f+static_cast<float>(i);
        s.vehicles.physics[i].fuel=-7;s.vehicles.physics[i].energy=-9;
        s.vehicles.physics[i].energy_capacity=200.0f+static_cast<float>(i);
        s.vehicles.contact[i].impact_count.value=77;s.vehicles.contact[i].shock=23;
        s.cars[i].source_word00=79+i;s.cars[i].name.fill('Q');s.selection[i].model=83+i;
    }
    s.gravity.factor=9;s.gravity.inverse_factor=8;s.gravity.magnitude=7;s.gravity.inverse_magnitude=6;
    s.gravity.vector={5,4,3};s.gravity.unit={2,3,4,9.25f};
    std::memset(s.score_popups.popups.data(),0x3f,sizeof(s.score_popups.popups));
    s.score_popups.sound_class=111;s.score_popups.sound_id=112;s.score_popups.sound_volume=113;
    for(auto& p:s.effect_particles.particles){p.active=1;p.position.x=9;}
    s.effect_particles.active_signal=127;s.ghosts.reference_completed=131;
    for(unsigned i=0;i<16;++i)s.finish.order[i]=i%6;s.finish.count=11;
    s.finish.best_total_ticks=71;s.finish.best_total_score=73;
    s.impact_particles.emitted=17;s.impact_particles.cursor=19;s.impact_particles.throttle=23;s.impact_particles.rate_selector=29;
    std::memset(s.impact_particles.particles.data(),0x3f,sizeof(s.impact_particles.particles));
    s.impact_draw_list.count=13;for(unsigned i=0;i<32;++i)s.impact_draw_list.slots[i]=31-i;
    s.outer.loop_exit_word_612948=0x80000001u;s.outer.source_word_612974=0;s.outer.source_word_612978=0xffffffffu;
    s.outer.source_word_612980=3;s.outer.render_counter=5;s.outer.source_word_612988=7;s.outer.source_word_61298c=11;
    s.outer.source_word_612994=13;s.outer.source_word_612998=17;s.outer.source_word_6129a0=19;
    s.outer.source_word_612be4=23;s.outer.source_word_61b62c=29;
    s.demo_ticks=51;s.hud_block=53;s.normal_finish.field_612bf0=59;s.ui.loading_tile_base=61;
    s.grid.start_spline=spline;s.automatic.routes=std::array{spline,spline,spline};s.automatic.initialized=3;
    s.automatic.route_distances={11,13,17};s.automatic.target_opaque=0xa5a5a5a5u;
    s.selected_hud=31;s.ui.selected_material=37;s.frame_pause=3;s.ui.second_pause=5;s.normal_finish.confirm_pressed=7;
    s.rail.spline=spline;s.rail.tube_glass_texture=41;s.rail.source_word_4d5ca0=0xabcdef01u;
    s.reset_globals={0x98765432u,0x12345678u,0xffffffffu};
    s.tick_clock={initial_clock[0],initial_clock[1],initial_clock[2]};
    s.source_scheduler.frequency=initial_clock[3];s.source_scheduler.fixed_mode_a=initial_clock[4];s.source_scheduler.fixed_mode_b=initial_clock[5];
    s.scene_mode=2;s.options.selection_phase=2;s.normal_finish.menu_requested=7;s.pause_flag=29;
}
struct Interrupted {};
struct Counter final:FrontendPerformanceCounter {
    const RaceSessionState& state;const SceneResetWitness& prefix;const SceneResetWitness& complete;
    const RaceOverlayTextures& overlay;Values sample;int failure;unsigned calls{};unsigned frequency_calls{};
    Counter(const RaceSessionState& s,const SceneResetWitness& p,const SceneResetWitness& c,
        const RaceOverlayTextures& o,Values v,int f):state(s),prefix(p),complete(c),overlay(o),sample(v),failure(f){}
    std::uint64_t counter() override{
        check(calls<4u,"more than four counter requests");
        check(clock_words(state)==after_reads(sample,calls),"counter acquired before prior source store or wrong retained world");
        checks+=static_cast<unsigned>((calls<2u?prefix:complete).verify());
        check(overlay.selected_overlay_texture()==0u,"clock acquired before overlay/prefix reset");
        const auto ordinal=calls++;++reads;
        if(failure==static_cast<int>(ordinal))throw Interrupted{};
        return sample[ordinal];
    }
    std::uint64_t frequency() override{++frequency_calls;throw std::runtime_error("reset requested frequency");}
};
struct NextChild {SceneOperation operation;};
struct Services final:SceneTransitionServices {
    RaceSessionPlatform& platform;Counter& counter;
    bool audio_prefix{};unsigned late_resets{};std::vector<SceneOperation> entered;
    Services(RaceSessionPlatform& p,Counter& c,bool audio=false):platform(p),counter(c),audio_prefix(audio){}
    void perform(SceneOperation operation) override{
        entered.push_back(operation);
        if(perform_source_scene_reset_445fa0(operation,platform,counter))return;
        if(audio_prefix&&operation==SceneOperation::stop_music){
            check(stop_source_music_45f9d0(platform).status==SourceMusicStopStatus::completed,"unexpected active music boundary");return;
        }
        if(audio_prefix&&operation==SceneOperation::stop_sound_instances){
            check(stop_source_samples_460dd0(platform).status==SourceSampleStopStatus::completed,"unexpected active sample boundary");return;
        }
        if(audio_prefix&&operation==SceneOperation::reset_sound_output){reset_source_sound_output_460c90(platform);return;}
        throw NextChild{operation}; //receiving refuses every unadmitted child
    }
    void reset_tick_clock() override{++late_resets;reset_platform_source_clock_442100(platform,counter);}
    void select_audio_environment(std::uint32_t) override{throw std::runtime_error("unrequested environment call");}
    void initialize_menu_viewport(std::uint32_t) override{throw std::runtime_error("unadmitted menu viewport");}
};
void no_backend(RaceSessionPlatform& p){
    const auto ready=p.host_readiness();check(!ready.assets&&!ready.audio_device&&!ready.audio_stream&&!ready.window_input,"native owner created");
    check(!p.source_allocations().initialized()&&p.source_allocations().live_count()==0,"continued to allocation begin");
}
}

int main(int argc,char** argv)try{
    check(argc==2,"supply NEW retail root");
    const AssetStore assets(argv[1]);const SplineSet splines(read_file(std::filesystem::path(argv[1])/"meshdata/track1.PMD"));
    const auto route=splines.find(1u);check(route.has_value(),"missing source spline1");
    const Bytes spline_before(route->storage.begin(),route->storage.end());
    auto platform=std::make_shared<RaceSessionPlatform>(SourceColdConstructionOnly{},argv[1],0.0f);
    auto alias=std::shared_ptr<RaceSessionState>(platform,&platform->state());auto& s=*alias;
    Device device;RaceOverlayTextures overlay(assets,device,platform);
    std::array<Image,14> images;std::array<const std::uint8_t*,14> image_storage;
    for(unsigned i=0;i<14;++i){images[i]=overlay.source_image(16+i);image_storage[i]=overlay.source_image(16+i).rgba.data();}
    const auto setup=[&]{seed(s,*route);device.unavailable=false;device.width=static_cast<std::uint16_t>(overlay.source_image(25).width);
        overlay.replace_source_texture(15,25);device.unavailable=true;};
    const auto retained=[&](unsigned uploads,unsigned widths){
        check(device.uploads==uploads&&device.width_reads==widths,"reset used texture device");
        check(s.reset_globals.transition_active==0x98765432u&&s.reset_globals.special_value==0x12345678u&&
            s.reset_globals.special_timer==0xffffffffu,"reset changed adjacent rail globals");
        check(alias.get()==&platform->state()&&!alias.owner_before(platform)&&!platform.owner_before(alias),"changed retained owner/alias");
        for(unsigned i=0;i<14;++i){const auto& image=overlay.source_image(16+i);
            check(image.rgba.data()==image_storage[i]&&image.rgba==images[i].rgba,"reset changed surviving image storage");}
        check(std::equal(spline_before.begin(),spline_before.end(),route->storage.begin(),route->storage.end()),"reset changed borrowed PMD");
        no_backend(*platform);
    };
    // Explicitly separate child entrypoints. No unsupported middle children
    // of442030 are synthesized or executed by this receiving sequence.
    for(const auto sample:values)for(const int failure:{-1,0,1,2,3}){
        setup();const auto uploads=device.uploads,widths=device.width_reads;
        const SceneResetWitness prefix(s,true),complete(s,true,true);
        Counter counter(s,prefix,complete,overlay,sample,failure);Services services(*platform,counter);
        bool interrupted=false;
        try{
            services.perform(SceneOperation::reset_scene_state);
            check(counter.calls==2u&&clock_words(s)==after_reads(sample,2),"first reset reused/skipped a counter read");
            checks+=static_cast<unsigned>(complete.verify());
            services.reset_tick_clock(); //distinct4420D8 contract; calls3 and4
        }catch(const Interrupted&){interrupted=true;}
        check(interrupted==(failure>=0),"wrong interruption outcome");
        const unsigned succeeded=failure<0?4u:static_cast<unsigned>(failure);
        check(counter.calls==(failure<0?4u:static_cast<unsigned>(failure+1))&&counter.frequency_calls==0u,"counter retry/reuse/extra request");
        check(clock_words(s)==after_reads(sample,succeeded),"wrong first-store or second-fault clock state");
        checks+=static_cast<unsigned>((failure>=0&&failure<2?prefix:complete).verify());
        check(services.entered==std::vector{SceneOperation::reset_scene_state}&&services.late_resets==(failure<0||failure>=2?1u:0u),"wrong child return/late entry");
        retained(uploads,widths);++two_reset_cases;if(failure>=0)++fault_cases;
    }
    // Actual442030 dispatch and actual already-admitted audio prefix. The
    // first unsupported child42EA80 is observed, rejected and never retried.
    for(const int failure:{-1,0,1}){
        setup();const auto uploads=device.uploads,widths=device.width_reads;
        const SceneResetWitness prefix(s,true),complete(s,true,true);
        Counter counter(s,prefix,complete,overlay,values[0],failure);Services services(*platform,counter,true);
        auto world=scene_transition_world(s);SceneLifecycleState lifecycle{};bool stopped=false;
        try{initialize_scene(world,lifecycle,services);}
        catch(const NextChild& next){check(failure<0&&next.operation==SceneOperation::begin_allocations,"wrong next scene child");stopped=true;}
        catch(const Interrupted&){check(failure>=0,"unexpected clock interruption");stopped=true;}
        check(stopped&&s.pause_flag==0u&&services.late_resets==0u,"442030 continued past boundary or skipped pause store");
        auto expected=std::vector{SceneOperation::stop_music,SceneOperation::stop_sound_instances,
            SceneOperation::reset_sound_output,SceneOperation::reset_scene_state};
        if(failure<0)expected.push_back(SceneOperation::begin_allocations);
        check(services.entered==expected,"admitted reset did not return exactly to the next child");
        check(counter.calls==(failure<0?2u:static_cast<unsigned>(failure+1))&&counter.frequency_calls==0u,"442030 consumed a later reset or retried");
        check(clock_words(s)==after_reads(values[0],failure<0?2u:static_cast<unsigned>(failure)),"442030 failed-read state");
        checks+=static_cast<unsigned>((failure<0?complete:prefix).verify());
        check(s.audio_level.output_b==255u&&s.audio_level.output_b_copy==255u,"prior460C90 stores missing");
        retained(uploads,widths);++initialize_cases;
    }
    // Two retired null-overlay boundary cases remain in the frozen baseline.
    // Their new source expectation is tested by overlay_world_word_tests.
    setup();const SceneResetWitness prefix(s,true),complete(s,true,true);
    Counter counter(s,prefix,complete,overlay,values[0],-1);
    const auto unchanged=prefix.observed_bytes();const auto clock=clock_words(s);
    for(const auto op:{SceneOperation::begin_allocations,SceneOperation::load_scene_resources}){
        check(!perform_source_scene_reset_445fa0(op,*platform,counter),"helper swallowed another child");
        check(prefix.observed_bytes()==unchanged&&clock_words(s)==clock&&counter.calls==0u,"unhandled operation wrote state");++unhandled_cases;
    }
    for(const bool late:{false,true}){
        std::exception_ptr error;std::thread worker([&]{try{
            if(late)reset_platform_source_clock_442100(*platform,counter);
            else (void)perform_source_scene_reset_445fa0(SceneOperation::reset_scene_state,*platform,counter);
        }catch(...){error=std::current_exception();}});worker.join();
        check(bool(error),"foreign thread entered reset");bool rejected=false;
        try{std::rethrow_exception(error);}catch(const std::logic_error&){rejected=true;}
        check(rejected&&prefix.observed_bytes()==unchanged&&clock_words(s)==clock&&counter.calls==0u&&
            overlay.selected_overlay_texture()==25u,"foreign thread performed partial source work");++wrong_threads;
    }
    no_backend(*platform);
    std::cout<<"{\"passed\":true,\"two_reset_cases\":"<<two_reset_cases<<",\"counter_fault_cases\":"<<fault_cases
        <<",\"initialize_prefix_cases\":"<<initialize_cases<<",\"missing_owner_cases\":"<<missing_cases
        <<",\"wrong_thread_cases\":"<<wrong_threads<<",\"unhandled_operation_cases\":"<<unhandled_cases
        <<",\"counter_reads\":"<<reads<<",\"checks\":"<<checks
        <<",\"next_unadmitted_child\":\"42EA80\",\"full_442030_qualified\":false,\"native_QPC_calls\":0,\"GPU_runs\":0}\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"reset composition: "<<e.what()<<'\n';return 1;}
catch(...){std::cerr<<"unexpected source boundary in receiving\n";return 1;}
