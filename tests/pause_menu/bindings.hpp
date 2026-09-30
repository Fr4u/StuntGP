#pragma once
#include "borrowed_fixture.hpp"
#include "sgn/race_overlay_textures.hpp"
#include "sgn/retail_material_storage.hpp"

namespace pause_menu_test {

// The receiving fixture supplies the already initialized full 40D500 world.
// reset.current may be the sole4D5BB8 owner or an invocation-only parameter
// with a separate live owner. In the latter variant construct this binding
// and its reset context for the invocation; do not retain a second cursor.
class BorrowedRestart final:public r::PauseMenuRestartSource {
    r::RaceInitializationWorld world_;
    r::VehicleResetParameters& reset_parameters_;
    std::span<const r::LevelCarSlot,6> cars_;
    std::span<const r::SelectionRecord,6> selection_;
    std::span<const std::uint32_t,4> chosen_;
    const std::uint32_t& control_;
    const std::uint32_t& scene_;
    const std::uint32_t& track_;
    unsigned& current_output_;
public:
    BorrowedRestart(r::RaceInitializationWorld world,r::VehicleResetParameters& parameters,
        std::span<const r::LevelCarSlot,6> cars,std::span<const r::SelectionRecord,6> selection,
        std::span<const std::uint32_t,4> chosen,const std::uint32_t& control,
        const std::uint32_t& scene,const std::uint32_t& track,unsigned& current_output):world_(world),reset_parameters_(parameters),
        cars_(cars),selection_(selection),chosen_(chosen),control_(control),scene_(scene),track_(track),current_output_(current_output){
        if(&world.reset.parameters!=&parameters)
            throw sgn::FormatError("pause restart fixture must refresh the actual reset invocation parameters");
    }
    r::PauseMenuRestartInvocation borrow_restart()override{
        // These are invocation parameters, never persistent source globals.
        // Refresh after stop_music; do not feed any expected output back here.
        reset_parameters_.mode=std::bit_cast<std::int32_t>(world_.clock.game_mode);
        reset_parameters_.stage=std::bit_cast<std::int32_t>(world_.clock.phase);
        if(&current_output_!=&world_.reset.current)world_.reset.current=current_output_;
        return {world_,{cars_,selection_,chosen_,std::bit_cast<std::int32_t>(control_),scene_,track_},current_output_};
    }
};

// Borrow vector objects rather than their allocations. An earlier complete
// child may replace an allocation; each later child sees its current storage.
// No copy of fonts, cached selected material, viewport or specular state exists.
class BorrowedDrawing final:public r::PauseMenuDrawingSources {
    r::RetailFontResources& fonts_;
    std::uint32_t& selected_;
    r::PolygonSubmissionState& submission_;
    const r::PhysicalMaterialStorage& materials_;
    const std::uint32_t& quality_;
    const std::uint32_t& legacy_;
    const r::ViewportState& viewport_;
public:
    BorrowedDrawing(r::RetailFontResources& fonts,std::uint32_t& selected,
        r::PolygonSubmissionState& submission,const r::PhysicalMaterialStorage& materials,
        const std::uint32_t& quality,const std::uint32_t& legacy,const r::ViewportState& viewport):
        fonts_(fonts),selected_(selected),submission_(submission),materials_(materials),
        quality_(quality),legacy_(legacy),viewport_(viewport){}
    r::TextSubmission borrow_text()override{
        return {fonts_,selected_,submission_.specular_enabled,materials_.active_materials(),quality_,legacy_,viewport_};
    }
    std::span<r::PolygonSubmissionVertex> borrow_alpha_vertices()override{return submission_.alpha_vertices;}
};

// Assembly has only borrowed views. Missing menu words live in MenuOwners;
// every other word is received from its existing child fixture owner. There
// is no action service, synthetic gameplay state or duplicate input binding.
inline r::PauseMenuWorld bind_world(MenuOwners& menu,const r::FrontendLaunchState& frontend,
    r::PauseLifecycleWorld lifecycle,r::RaceInitializationWorld initialization,
    const std::uint32_t& render_b,const std::uint32_t& game_type,std::uint32_t& detail,
    r::AudioStreamPlaybackWorld streaming,BorrowedRestart& restart,
    r::SceneTransitionWorld& transition,r::SceneRaceResultWorld& results,
    r::GameTextState& game_text,r::RaceOverlayTextures& textures,
    std::span<std::uint32_t,35> descriptor,r::UiLineClipScratch& clip,r::PauseMeterLabels& labels,
    BorrowedDrawing& drawing,r::PolygonSubmissionSink& sink){
    if(&lifecycle.physical_input!=&menu.physical)
        throw sgn::FormatError("pause menu fixture must share Loader's physical identity owner");
    r::PauseMenuWorld world{menu.view(frontend),lifecycle,initialization.clock,render_b,game_type,detail,
        initialization.selected_camera_preset,initialization.audio.options,initialization.audio.audio,
        streaming,restart,transition,results,initialization.vehicles,initialization.frame_projection,
        game_text,textures,descriptor,clip,labels,drawing,sink};
    r::validate_pause_menu_bindings(world);
    return world;
}
}
