#pragma once
#include "bindings.hpp"
#include "input_wire.hpp"
#include "render_boundary.hpp"
#include "../retail_bootstrap_state.hpp"
#include "../race_initialization_state.hpp"
#include "../audio_initialize_state.hpp"
#include "../pause_meter/state.hpp"
#include <memory>
#include <tuple>

namespace pause_menu_test {
namespace bt=r::bootstrap_test;
namespace rt=r::race_initialization_test;
namespace at=sgn::test::audio_initialize;

struct FixtureUi final:r::PauseMeterLabels {
    pause_meter_wire::StateBytes carrier{}; //raw seed/capture codec only
    r::RetailFontResources fonts;
    std::uint32_t selected{},quality{},legacy{};
    r::ViewportState viewport;
    std::array<std::uint32_t,35> quad{};
    r::UiLineClipScratch clip;
    std::array<std::uint32_t,2> labels{0,1};
    std::span<const std::uint8_t> read_left_4851e0()override{return pause_meter_wire::label(labels[0]);}
    std::span<const std::uint8_t> read_right_4851e4()override{return pause_meter_wire::label(labels[1]);}
};

// All state here is fixture-owned input or the actual native output. The
// receiving process never copies an expected menu state into these owners.
class NativeFixture {
    static sgn::Bytes read(const std::filesystem::path& folder,const char* name,std::size_t size){
        auto data=sgn::read_file(folder/(std::string(name)+".bin"));
        input::require(data.size()==size,"pause seed chunk size");return data;
    }
    template<class T>static void import(T& target,std::span<const std::uint8_t> data){
        static_assert(std::is_trivially_copyable_v<T>);
        input::require(data.size()==sizeof(T),"pause seed typed extent");std::memcpy(&target,data.data(),data.size());
    }
    static std::uint32_t word(std::span<const std::uint8_t> data,std::size_t at=0){
        input::require(at<=data.size()&&data.size()-at>=4,"pause seed word extent");std::uint32_t result;std::memcpy(&result,data.data()+at,4);return result;
    }
public:
    const sgn::AssetStore& assets;
    r::ModelResources& models;
    r::VehicleStorage vehicles;
    r::SplineSet splines;
    std::array<bt::Record,8> vehicle_carrier;
    rt::Controllers controller_carrier;
    rt::Globals race_carrier;
    at::Record audio_carrier;
    r::contact::player::Controls controls;
    r::VehicleResetGlobals reset_globals;
    r::VehicleResetParameters reset_parameters;
    std::array<r::ResetVehicle,6> reset_views;
    r::VehicleResetWorld reset; //sole persistent4D5BB8 lives in reset.current
    r::VehicleGridState grid;
    r::VehicleLoadSelection load_selection;
    r::RaceClockState clock;
    r::AiAutomaticGlobals automatic;
    std::array<r::ControllerHudCountdown,4> countdown;
    std::array<r::RaceHudLayout,4> layouts;
    unsigned selected_hud{},selected_preset{};
    r::RaceHudMargins margins;
    r::CameraMatrix projection;
    r::CameraMatrix shared_model_identity{}; //one fixture owner; old corpus omits61C468
    float inverse_depth{},depth_offset{};
    r::RaceStartupCounters counters;
    r::RaceInitializationState startup;
    r::AudioMusicTrack music;
    r::AudioDriverState driver_state;
    r::AudioInitializationState audio_initialization;
    r::AudioMaintenanceState maintenance;
    r::AudioStreamState stream;
    r::LevelAudioState level;
    r::LevelOverrideOptions options;
    std::uint32_t track{},control{},scene{},music_track{},split{},game_type{},render_b{},detail{};
    std::uint32_t frame_pause{},pause_age{},pause_status{},second_pause{},audio_pause{},pending{},transition_ticks{},suppress{};
    std::array<r::LevelCarSlot,6> cars;
    std::array<r::SelectionRecord,6> selection;
    std::array<std::uint32_t,4> chosen;
    r::FrontendLaunchState frontend;
    std::array<r::ControllerOverlayTransition,4> overlays;
    r::VehicleAudioGlobals vehicle_audio_globals;
    r::VehicleAudioFrontier frontier;
    r::TimeTrialSelectionState trial;
    r::TimeTrialGhosts ghosts;
    r::ReplayBank recordings;
    r::ProgressionRecordState records;
    r::VehicleRaceFinishState finish;
    r::GameTextState game_text;
    MenuOwners menu;
    r::AudioDriver driver;
    r::AudioStartupWorld audio;
    std::array<r::VehicleAudioCar,6> audio_cars;
    r::VehicleAudioWorld vehicle_audio;
    r::AudioStreamPlaybackWorld streaming;
    FixtureUi ui;
    r::PolygonSubmissionState submission;
    r::PhysicalMaterialStorage materials;
    BorrowedDrawing drawing;
    MemoryTextureDevice texture_device;
    std::shared_ptr<r::RaceSessionPlatform> overlay_platform;
    r::RaceOverlayTextures textures;
    RecordingDrawDevice draw_device;

    r::RaceHudView hud(){return {controls,countdown,layouts,selected_hud,margins};}
    r::VehicleAudioCar audio_car(unsigned i){return {vehicles.parameters[i],vehicles.physics[i],vehicles.race[i],vehicles.contact[i],vehicles.load[i],vehicles.audio[i],i};}
    r::RaceInitializationWorld initialization(r::VehicleResetWorld& context){return {vehicles,assets,context,splines,grid,load_selection,
        clock,automatic,hud(),selected_preset,{projection,inverse_depth,depth_offset,shared_model_identity},counters,startup,audio,music};}
    r::PauseLifecycleWorld lifecycle(){return {startup.field_60f2c0,frame_pause,pause_age,pause_status,second_pause,menu.physical,
        hud(),overlays,vehicles.physics,vehicle_audio,level,{level.output_a,stream.playback_active,stream.field_e100,maintenance.music.gain_step}};}
    r::SceneTransitionWorld transition(){return {scene,options.selection_phase,pending,transition_ticks,suppress,frame_pause,controls,overlays,
        level.output_b_copy,stream.field_e100,maintenance.music.gain_step,audio_pause};}
    r::SceneRaceResultWorld results(){return {options,selection,trial,ghosts,recordings,vehicles.race,records,finish,game_type,selected_preset};}

    NativeFixture(const sgn::AssetStore& store,r::ModelResources& resources,const std::filesystem::path& seed,
        const std::filesystem::path& asset_root,r::AudioDevice& device,r::AudioInitializationPlatform& platform,
        r::AudioStreamPlaybackPlatform& stream_platform,r::AudioStreamCursorDevice& cursors,r::AudioMusicBufferProducer& music_producer):
        assets(store),models(resources),splines(sgn::read_file(asset_root/"meshdata/track1.PMD")),
        reset_views(vehicles.reset_views()),reset{reset_views,0,controls,reset_globals,reset_parameters},
        driver{driver_state,device,level,controls,split},
        audio{driver,maintenance,stream,audio_initialization,level,options,assets,platform,stream_platform,0},
        audio_cars{audio_car(0),audio_car(1),audio_car(2),audio_car(3),audio_car(4),audio_car(5)},
        vehicle_audio{audio_cars,driver,controls,vehicle_audio_globals,frontier,audio_pause},
        streaming{driver,maintenance.music,stream,audio_initialization.device,platform,cursors,stream_platform,music_producer},
        drawing{ui.fonts,ui.selected,submission,materials,ui.quality,ui.legacy,ui.viewport},
        texture_device(materials),
        overlay_platform(std::make_shared<r::RaceSessionPlatform>(r::SourceColdConstructionOnly{},asset_root,0.0f)),
        textures(assets,texture_device,overlay_platform),draw_device(submission,ui.viewport){
        const auto args=read(seed,"race-args",24);track=word(args);control=word(args,4);scene=word(args,8);music_track=word(args,12);split=word(args,16);game_type=word(args,20);
        input::require(track==1,"pause first actual source seed track identity");
        import(cars,read(seed,"cars",sizeof(cars)));import(selection,read(seed,"selection",sizeof(selection)));import(chosen,read(seed,"chosen",16));
        const auto raw_vehicles=read(seed,"vehicles",8*bt::Record{}.size());
        for(unsigned i=0;i<8;++i){import(vehicle_carrier[i],std::span(raw_vehicles).subspan(i*sizeof(bt::Record),sizeof(bt::Record)));bt::transfer(vehicle_carrier[i],vehicles,i,true,models);}
        import(controller_carrier,read(seed,"controllers",sizeof(controller_carrier)));
        for(unsigned i=0;i<4;++i){const auto at=i*rt::controller_stride+0xac;const auto pointer=word(controller_carrier,at);
            input::require(pointer>=0x49a1f8&&(pointer-0x49a1f8)%0x7724==0&&(pointer-0x49a1f8)/0x7724<6,"pause controller vehicle source pointer");
            const auto value=(pointer-0x49a1f8)/0x7724;std::memcpy(controller_carrier.data()+at,&value,4);
            import(overlays[i],std::span(controller_carrier).subspan(i*rt::controller_stride+0x1e8,40));}
        import(race_carrier,read(seed,"race-globals",sizeof(race_carrier)));rt::globals(race_carrier,initialization(reset),true);rt::controllers(controller_carrier,hud(),true);
        import(audio_carrier,read(seed,"audio",sizeof(audio_carrier)));at::transfer(audio_carrier,driver_state,audio_initialization,maintenance,stream,level,options,true);
        audio.window=audio_carrier[0x1a8c/4];import(driver_state.spatial_template,read(seed,"audio-spatial-template",64));
        import(frontend.input_bindings,read(seed,"physical-bindings",32));
        const auto text_bytes=read(seed,"game-text-arena",game_text.arena.size());std::memcpy(game_text.arena.data(),text_bytes.data(),text_bytes.size());
        const auto pointers=read(seed,"game-text-pointers",420);
        for(unsigned i=0;i<105;++i){const auto p=word(pointers,i*4);input::require(p==0||(p>=0x60f6a0&&p<0x6126a0),"pause text arena pointer");
            game_text.offsets[i]=p?std::optional(p-0x60f6a0):std::nullopt;}
        const auto text_scalar=read(seed,"game-text-scalar",8);game_text.remaining_bytes=word(text_scalar);game_text.language=word(text_scalar,4);
        const auto menu_words=read(seed,"menu",20);menu.physical.token=0;menu.release=word(menu_words,4);menu.selection=word(menu_words,8);menu.highlight=word(menu_words,12);menu.ready=word(menu_words,16);
        input::require(word(menu_words)==0,"pause initial physical seed must be null");import(menu.rows,read(seed,"rows",192));
        const auto font=sgn::read_file(seed/"font-state.bin");constexpr auto globals=pause_meter_wire::globals;
        input::require(font.size()>=globals+68+255*28,"pause complete font seed");std::copy_n(font.begin(),globals,ui.carrier.begin());
        const auto ui_words=read(seed,"ui-globals",28);std::memcpy(ui.carrier.data()+globals,ui_words.data(),28);
        const auto current_font=word(ui_words);input::require(current_font==0x577bd0||current_font==0x579770,"pause original selected font");
        pause_meter_wire::put(ui.carrier,globals,1+(current_font-0x577bd0)/0x1ba0);
        std::copy_n(font.begin()+globals+68,255*28,ui.carrier.begin()+pause_meter_wire::materials);
        for(const auto [name,offset,size]:std::array<std::tuple<const char*,unsigned,unsigned>,3>{{
            {"viewport",pause_meter_wire::viewport,152},{"descriptor",pause_meter_wire::quad,140},{"alpha",pause_meter_wire::alpha,320}}}){
            const auto bytes=read(seed,name,size);std::copy(bytes.begin(),bytes.end(),ui.carrier.begin()+offset);}
        pause_meter_wire::put(ui.carrier,pause_meter_wire::labels,0);pause_meter_wire::put(ui.carrier,pause_meter_wire::labels+4,1);
        // The existing codec is a transient decoder. Move its sole fonts and
        // publish other fields once; it is destroyed before any menu executes.
        pause_meter_test::State decoded;decoded.carrier=ui.carrier;decoded.transfer(true);
        ui.fonts=std::move(decoded.fonts);ui.selected=decoded.selected;ui.quality=decoded.quality;ui.legacy=decoded.legacy;
        for(unsigned f=0;f<2;++f){const auto identity=ui.fonts.source_word(f,4);
            input::require(identity==f+1&&ui.fonts.allocate_scratch(f)==identity,"pause actual font scratch owner identity");}
        ui.viewport=decoded.viewport;ui.quad=decoded.quad;ui.clip=decoded.clip;
        if(std::filesystem::exists(seed/"clip.bin"))import(ui.clip.words,read(seed,"clip",36));
        if(std::filesystem::exists(seed/"selected-overlay.bin"))
            input::require(word(read(seed,"selected-overlay",4))==textures.selected_overlay_texture(),"pause initial overlay source identity");
        submission.alpha_vertices.assign(decoded.alpha.begin(),decoded.alpha.end());submission.specular_enabled=decoded.specular;
        materials.reset_loading_42c480();
        for(unsigned slot=0;slot<255;++slot)for(unsigned lane=0;lane<7;++lane)
            materials.source_word(slot,lane*4)=word(font,globals+68+slot*28+lane*4);
        materials.active_cursor()=word(font,globals+3*4);
        texture_device.destinations.emplace(15,textures.source_image(16));
        if(std::filesystem::exists(seed/"texture-15-u16.bin")){
            const auto pixels=sgn::read_file(seed/"texture-15-u16.bin");const auto extent=read(seed,"texture-15-extent",8);
            const auto width=word(extent),height=word(extent,4);
            input::require(width>0&&height>0&&width<=65535&&height<=65535&&pixels.size()==std::size_t(width)*height*2,"pause source device preimage extent");
            auto& image=texture_device.destinations.at(15);image.width=width;image.height=height;image.rgba.resize(std::size_t(width)*height*4);
            for(std::size_t i=0;i<pixels.size()/2;++i){const unsigned p=unsigned(pixels[i*2])|(unsigned(pixels[i*2+1])<<8);
                for(unsigned c=0;c<3;++c){const unsigned five=(p>>(10-c*5))&31;image.rgba[i*4+c]=static_cast<std::uint8_t>((five*255+15)/31);}
                image.rgba[i*4+3]=(p&0x8000)?255:0;}
        }
        texture_device.primary_tokens.emplace(15,materials.record(15).primary_texture);
        const auto descriptors=splines.descriptors();input::require(!descriptors.empty(),"pause source track spline");
        const auto spline=splines.find(descriptors.front().id);input::require(bool(spline),"pause source first spline");reset_parameters.spline=*spline;
        reset_parameters.selector=std::bit_cast<std::int32_t>(game_type);reset_parameters.course_length=clock.course_length;
        import_extra(seed);
    }

    std::map<std::uint32_t,void*> extra_fields(){return {
        {0x612bf4,&pending},{0x612958,&second_pause},{0x61294c,&frame_pause},{0x612960,&render_b},{0x60f2b0,&clock.game_mode},
        {0x60df98,&game_type},{0x576be0,&split},{0x61297c,&detail},{0x60f020,&selected_preset},{0x62e0a8,&level.input_a},
        {0x62e0f0,&level.input_b},{0x489790,&level.scale},{0x62e0f8,&level.output_a},{0x489798,&level.output_b},{0x48979c,&level.output_b_copy},
        {0x4835e0,&menu.language},{0x612be8,&scene},{0x612bec,&options.selection_phase},{0x612cf8,&transition_ticks},{0x612bf0,&suppress},
        {0x60f2c8,&clock.countdown_pending},{0x61295c,&pause_status},{0x60f018,&control},{0x60dfa4,&music_track},{0x612954,&pause_age},
        {0x62f5ac,&stream.playback_active},{0x62e088,&stream.allocated},{0x62df38,&stream.flags},
        {0x62df50,&stream.timer},{0x62e08c,&stream.result},
        {0x4d5b20,&audio_pause},{0x60f2cc,&clock.rolling_state},{0x60eff4,&clock.remaining_ticks},{0x60eff8,&clock.lap_count},
        {0x60effc,&records.previous_lap_ticks},{0x60f000,&records.lap_ticks},{0x60f004,&finish.best_total_ticks},{0x60f008,&records.score},
        {0x60f00c,&finish.best_total_score},{0x4d5ca4,&reset_globals.transition_active},{0x4d5ca8,&reset_globals.special_value},
        {0x4d5cac,&reset_globals.special_timer},{0x62f5b0,&driver_state.duplicate_count},{0x62f5b4,&driver_state.playing_count},
        {0x4897a0,&driver_state.range},{0x4897a4,&driver_state.range_squared},{0x4897a8,&driver_state.inverse_range},
        {0x60d730,&options.music},{0x60d734,&options.audio_a},{0x60d738,&options.audio_b},{0x60d73c,&options.render_a},
        {0x60d740,&options.render_b},{0x60d74c,&options.texture},{0x60d750,&options.split},{0x60d764,&options.laps},
        {0x60d768,&options.challenge_ticks},{0x60d76c,&options.record},{0x60d770,&options.record_f004},{0x60d774,&options.record_f008},
        {0x60d778,&options.record_f00c},{0x60d77c,&options.game_type},{0x60d780,&options.race_mode},{0x60d784,&options.start_delay},
        {0x60d788,&options.transient},{0x60df70,&options.demo_override},{0x60df94,&options.display_selector}};}
    void import_extra(const std::filesystem::path& seed){
        const auto bytes=sgn::read_file(seed/"extra-words.bin");input::require(bytes.size()%8==0,"pause extra seed extent");const auto fields=extra_fields();
        for(std::size_t at=0;at<bytes.size();at+=8){if(word(bytes,at)==0x62df08){maintenance.music.buffer.value.source_low_word(word(bytes,at+4));continue;}
            const auto field=fields.find(word(bytes,at));input::require(field!=fields.end(),"pause unmapped extra seed owner");
            std::memcpy(field->second,bytes.data()+at+4,4);}
    }
};
}
