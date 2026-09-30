#include "source_scene_reset_witness.hpp"
#include "sgn/source_scene_reset_prefix.hpp"
#include "sgn/race_overlay_textures.hpp"
#include <algorithm>
#include <atomic>
#include <cstddef>
#include <iostream>
#include <thread>

using namespace sgn;
using namespace sgn::retail;
using namespace source_scene_reset_receiving;
namespace {
std::size_t checks{},cases{},missing_owner{},retained_owner{};
void check(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
struct Device final:TextureReplacementDevice {
    Image destination;std::uint32_t width{};bool released{};
    mutable unsigned width_calls{};unsigned uploads{};
    std::uint16_t destination_width(std::uint32_t slot)const override{
        if(released||slot!=15)throw FormatError("reset touched unavailable receiving destination");
        if(width>0xffffu)throw FormatError("receiving width exceeds the source16-bit field");
        ++width_calls;return static_cast<std::uint16_t>(width);
    }
    void upload_to_destination(std::uint32_t slot,const Image& image)override{
        if(released||slot!=15)throw FormatError("reset uploaded to unavailable receiving destination");
        ++uploads;destination=image;
    }
};
struct OverlaySnapshot {
    std::array<Image,14> images;
    std::array<const Image*,14> objects;
    std::array<const std::uint8_t*,14> pixels;
    std::array<std::optional<std::uint32_t>,255> completed;
    Image destination;unsigned widths{},uploads{};
    OverlaySnapshot(const RaceOverlayTextures& bank,const Device& device){
        for(unsigned i=0;i<14;++i){const auto& image=bank.source_image(i+16);images[i]=image;objects[i]=&image;pixels[i]=image.rgba.data();}
        for(unsigned i=0;i<255;++i)completed[i]=bank.completed_source(i);
        destination=device.destination;widths=device.width_calls;uploads=device.uploads;
    }
    void verify(const RaceOverlayTextures& bank,const Device& device)const{
        for(unsigned i=0;i<14;++i){const auto& image=bank.source_image(i+16);
            check(&image==objects[i]&&image.rgba.data()==pixels[i]&&image.width==images[i].width&&
                image.height==images[i].height&&image.rgba==images[i].rgba,"reset replaced source image bytes/storage");}
        for(unsigned i=0;i<255;++i)check(bank.completed_source(i)==completed[i],"reset changed completed-source history");
        check(device.width_calls==widths&&device.uploads==uploads&&device.destination.width==destination.width&&
            device.destination.height==destination.height&&device.destination.rgba==destination.rgba,"reset touched destination pixels/device");
    }
};
void no_backend(RaceSessionPlatform& platform){
    const auto h=platform.host_readiness();
    check(!h.assets&&!h.audio_device&&!h.audio_stream&&!h.window_input,"reset prepared a host backend");
    check(!platform.source_allocations().initialized()&&platform.source_allocations().live_count()==0,"reset began source allocations");
    bool absent=false;try{(void)platform.source_audio_owner();}catch(const FormatError& e){
        absent=std::string_view(e.what())=="source audio owner has not been constructed";
    }
    check(absent,"reset constructed a source audio owner");
}
void seed(RaceSessionState& s,unsigned variation){
    s.frontend_launch={0x10203040u+variation,91,23,0x76543210u,{}};
    for(unsigned i=0;i<4;++i)s.frontend_launch.input_bindings[i]={i+2,i+7};
    for(unsigned i=0;i<6;++i){
        auto& p=s.vehicles.physics[i];auto& race=s.vehicles.race[i];auto& contact=s.vehicles.contact[i];
        s.vehicles.parameters[i].fuel_capacity=100.0f+static_cast<float>(variation*6+i);
        p.fuel=-7.0f;p.energy=-9.0f;p.energy_capacity=200.0f+static_cast<float>(variation*6+i);
        p.position={13.0f+static_cast<float>(i),-11,19,2};p.velocity={17,31,47};p.wheels[0].speed=81;
        race.active=(i+variation)%3;race.lap_index=17;race.automatic_drive=19;
        contact.impact_count.value=77;contact.shock=23.0f;contact.com_2628=0x12345678u;
        s.cars[i].source_word00=0x7a000000u+i;s.cars[i].source_word04=0x7b000000u+i;s.cars[i].model=0x7c000000u+i;
        s.cars[i].settings={11,12,13,14,15,16};s.cars[i].name.fill('Q');
        s.selection[i].model=101+i;s.selection[i].name.fill('Z');
    }
    for(unsigned i=0;i<8;++i){auto& m=s.vehicles.models[i];
        m.model=static_cast<std::size_t>(0x1234567800000000ull+i);m.shadow=static_cast<std::size_t>(0x8765432100000000ull+i);
        m.position={2,3,5,7};m.scale={11,13,17,19};m.template_tail={23,29,31,37};m.field_2708=41;
        s.vehicles.presentation[i].field_3134=0x58000000u+i;
    }
    s.gravity.factor=9;s.gravity.inverse_factor=8;s.gravity.magnitude=7;s.gravity.inverse_magnitude=6;
    s.gravity.vector={5,4,3};s.gravity.unit={2,3,4,9.25f};
    // This source POD bank contains integer, float and character fields only.
    //0x3f bytes represent finite floats and preserve nonzero canaries everywhere.
    static_assert(sizeof(VehicleScorePopup)==0x138&&std::is_trivially_copyable_v<VehicleScorePopup>);
    std::memset(s.score_popups.popups.data(),0x3f,sizeof(s.score_popups.popups));
    s.score_popups.sound_class=111;s.score_popups.sound_id=112;s.score_popups.sound_volume=113;
    using Particle=contact::effects::EffectParticle;
    static_assert(offsetof(Particle,source_undefined)==0xd0);
    for(unsigned i=0;i<64;++i){auto& particle=s.effect_particles.particles[i];
        std::memset(&particle,0x3f,offsetof(Particle,source_undefined));
        particle.active=(i+variation)%4;
        for(unsigned j=0;j<7;++j)particle.source_undefined[j]=((i+j+variation)%2)!=0;
    }
    s.effect_particles.active_signal=0x12345678u;
    s.ghosts.reference_completed=0xabcdef01u;
    for(unsigned i=0;i<16;++i)s.finish.order[i]=i%6;s.finish.count=11;
    s.impact_particles.emitted=17;s.impact_particles.cursor=19;s.impact_particles.throttle=23;s.impact_particles.rate_selector=29;
    std::memset(s.impact_particles.particles.data(),0x3f,sizeof(s.impact_particles.particles));
    s.impact_particles.particles[0].reserved.fill(0xcafebabeu);
    s.impact_draw_list.count=13;for(unsigned i=0;i<32;++i)s.impact_draw_list.slots[i]=31-i;
    s.outer.loop_exit_word_612948=variation==1?1u:0x81234560u+variation;
    constexpr std::array<std::uint32_t,8> pair_words{0,1,2,10,11,0x7fffffffu,0x80000000u,0xffffffffu};
    s.outer.source_word_612974=pair_words[variation%8];
    s.outer.source_word_612978=pair_words[(variation/8)%8];
    s.outer.source_word_612980=pair_words[(variation/8)%8];
    s.outer.render_counter=pair_words[(variation+2*(variation/8))%8];
    s.outer.source_word_612988=pair_words[variation%8];
    s.outer.source_word_61298c=pair_words[variation%8];s.outer.source_word_612994=pair_words[(variation+1)%8];
    s.outer.source_word_612998=pair_words[(variation+2)%8];s.outer.source_word_6129a0=pair_words[(variation+3)%8];
    s.outer.source_word_612be4=pair_words[(variation+4)%8];s.outer.source_word_61b62c=pair_words[(variation+5)%8];
    s.demo_ticks=51;s.hud_block=53;s.normal_finish.field_612bf0=59;s.ui.loading_tile_base=61;
    s.automatic.initialized=3;s.automatic.route_distances={11,13,17};s.automatic.target={19,23,29};
    s.automatic.target_opaque=0xa5a5a5a5u;s.finish.best_total_ticks=71;s.finish.best_total_score=73;
    s.selected_hud=31;s.ui.selected_material=37;s.frame_pause=3;s.ui.second_pause=5;s.normal_finish.confirm_pressed=7;
    s.tick_clock={0x11111111u,0x22222222u,0x33333333u};s.scene_mode=2;s.options.selection_phase=2;
    s.normal_finish.menu_requested=0;s.pause_flag=0;
}
void run(const std::filesystem::path& root){
    auto platform=std::make_shared<RaceSessionPlatform>(SourceColdConstructionOnly{},root,0.0f);
    check(platform->state().outer.loop_exit_word_612948==0,"612948 did not begin from proven PE zero-fill");
    check(platform->state().outer.source_word_612974==0&&platform->state().outer.source_word_612978==0,
        "612974/612978 did not begin from proven PE zero-fill");
    platform->state().outer.source_word_612974=0x80000000u;platform->state().outer.source_word_612978=0xffffffffu;
    check(platform->state().outer.source_word_612980==0&&platform->state().outer.render_counter==0&&
        platform->state().outer.source_word_612988==0,"612980/84/88 did not begin from proven PE zero-fill");
    platform->state().outer.source_word_612980=0x81234567u;platform->state().outer.render_counter=0xffffffffu;
    platform->state().outer.source_word_612988=0x80000000u;
    platform->state().outer.loop_exit_word_612948=0xa5a5a5a5u;
    check(platform->state().outer.source_word_61298c==0&&platform->state().outer.source_word_612994==0&&platform->state().outer.source_word_612998==0&&
        platform->state().outer.source_word_6129a0==0&&platform->state().outer.source_word_612be4==0&&platform->state().outer.source_word_61b62c==0,"six raw words PE initial zeros");
    platform->state().outer.source_word_61298c=0x80000000u;platform->state().outer.source_word_612994=0xffffffffu;
    platform->state().outer.source_word_612998=2;platform->state().outer.source_word_6129a0=3;
    platform->state().outer.source_word_612be4=4;platform->state().outer.source_word_61b62c=5;
    RaceSession session(SourceColdAttachmentOnly{},platform);auto& s=session.state();
    check(s.outer.loop_exit_word_612948==0xa5a5a5a5u,"Session attachment reset persistent process-loop word");
    check(s.outer.source_word_612974==0x80000000u&&s.outer.source_word_612978==0xffffffffu,
        "Session attachment reset or normalized the persistent source pair");
    check(s.outer.source_word_612980==0x81234567u&&s.outer.render_counter==0xffffffffu&&
        s.outer.source_word_612988==0x80000000u,"Session attachment reset or normalized persistent612980/84/88");
    constexpr std::array<std::pair<std::uint32_t,bool>,8> loop_cases{{
        {0,false},{1,true},{2,false},{255,false},{0x7fffffffu,false},
        {0x80000000u,false},{0xfffffffeu,false},{0xffffffffu,false}}};
    for(const auto& [word,expected]:loop_cases){
        s.outer.loop_exit_word_612948=word;
        check(s.outer.source_loop_exit_requested_44eb45()==expected&&s.outer.loop_exit_word_612948==word,
            "44EB45 predicate must test exactly1 and never mutate the source word");
    }
    check(&s==&platform->state()&&session.shared_platform()==platform,"reset uses another Session world");
    check(s.outer.source_word_61298c==0x80000000u&&s.outer.source_word_612994==0xffffffffu&&s.outer.source_word_612998==2&&
        s.outer.source_word_6129a0==3&&s.outer.source_word_612be4==4&&s.outer.source_word_61b62c==5,"six raw words must survive Session attachment");
    check(s.impact_draw_list.cursor_is_null()&&std::all_of(s.impact_draw_list.slots.begin(),s.impact_draw_list.slots.end(),
        [](auto v){return v==0xffffffffu;}),"PE NULL list/cursor must differ from normalized zero");
    const SplineSet splines(read_file(root/"meshdata/track1.PMD"));const auto spline=splines.find(1);check(spline.has_value(),"actual spline1");
    const std::vector<std::uint8_t> spline_before(spline->storage.begin(),spline->storage.end());
    const AssetStore assets(root);Device device,other_device;
    auto other_overlay_platform=std::make_shared<RaceSessionPlatform>(SourceColdConstructionOnly{},root,0.0f);
    RaceOverlayTextures overlay(assets,device,platform),other_overlay(assets,other_device,other_overlay_platform);
    other_device.width=other_overlay.source_image(25).width;other_overlay.replace_source_texture(15,25);
    const OverlaySnapshot other_images(other_overlay,other_device);
    RaceSessionPlatform other(SourceColdConstructionOnly{},root,0.0f);SceneResetWitness other_state(other.state());
    const auto other_before=other_state.observed_bytes();no_backend(*platform);
    // Frozen retained-owner corpus only. The32 removed missing-owner inputs
    // now have a separate no-resource world-reset test; no old oracle is edited.
    for(unsigned i=0;i<64;++i){
        if(((i+i/8)%2)==0)continue;
        seed(s,i);s.grid.start_spline=spline;s.automatic.routes=std::array{*spline,*spline,*spline};device.released=false;const auto source=16u+(i%12u);
        device.width=overlay.source_image(source).width;overlay.replace_source_texture(15,source);
        check(overlay.selected_overlay_texture()==source,"receiving source texture failed to initialize");
        const OverlaySnapshot images(overlay,device);device.released=true;
        const bool missing=((i+i/8)%2)==0; //both branches include first-word0 with second-word nonzero
        const SceneResetWitness expected(s,!missing);const auto clock=bytes(s.tick_clock);
        const auto result=reset_source_scene_445fa0_prefix(*platform);
        checks+=expected.verify();++cases;
        if(missing){++missing_owner;
            check(result.status==SourceSceneResetStatus::overlay_owner_required&&result.blocked_address==0x44600f&&
                result.missing_source_word==0x61b634&&overlay.selected_overlay_texture()==source,"missing overlay owner ran its source store");
        }else{++retained_owner;
            check(result.status==SourceSceneResetStatus::callee_required&&result.blocked_address==0x4460cb&&
                result.missing_source_word==0&&overlay.selected_overlay_texture()==0,"wrong first unmapped scalar boundary");
        }
        check(bytes(s.tick_clock)==clock&&s.scene_mode==2&&s.options.selection_phase==2&&s.normal_finish.menu_requested==0&&s.pause_flag==0,
            "prefix ran later clock reset or changed caller scene/audio words");
        using Codec=contact::effects::ImpactParticleDrawList;
        check(Codec::encode_cursor(s.impact_draw_list.count)==(missing?0x61b5a4u+13u*4u:0u),"NULL cursor encoded as listbase");
        for(unsigned j=0;j<32;++j)check(Codec::encode_slot(s.impact_draw_list.slots[j])==(missing?0x612da4u+(31u-j)*0x440u:0u),"NULL slot encoded as particle0");
        images.verify(overlay,device);check(other_state.observed_bytes()==other_before,"reset changed another platform");
    }
    check(std::equal(spline_before.begin(),spline_before.end(),spline->storage.begin(),spline->storage.end()),"reset mutated borrowed spline storage");
    no_backend(*platform);other_images.verify(other_overlay,other_device);
    check(other_overlay.selected_overlay_texture()==25,"reset changed another overlay owner");
    seed(s,99);const SceneResetWitness guarded(s);const auto before=guarded.observed_bytes();const auto clock=bytes(s.tick_clock);
    const auto selected=overlay.selected_overlay_texture();const OverlaySnapshot images(overlay,device);
    std::atomic<unsigned> refused{0};std::thread foreign([&]{
        try{(void)reset_source_scene_445fa0_prefix(*platform);}catch(const std::logic_error&){++refused;}
        try{(void)reset_source_scene_445fa0_prefix(*platform);}catch(const std::logic_error&){++refused;}
    });foreign.join();
    check(refused==2&&guarded.observed_bytes()==before&&bytes(s.tick_clock)==clock&&overlay.selected_overlay_texture()==selected,
        "foreign thread mutated prefix before owner-thread rejection");images.verify(overlay,device);no_backend(*platform);
}
}
int main(int argc,char** argv)try{
    check(argc==2,"expected NEW retail root");run(argv[1]);
    std::cout<<"SCENE_RESET_PREFIX_PASS authored_cases="<<cases<<" missing_owner="<<missing_owner<<" retained_owner="<<retained_owner
        <<" checks="<<checks<<" original_runs=0 native_backend_runs=0 frontier=4460CB\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
