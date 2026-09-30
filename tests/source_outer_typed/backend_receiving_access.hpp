#pragma once
#include "sgn/race_application_polygon_backend.hpp"
#include <stdexcept>
namespace sgn {
// The single approved friend exposes NEW host owner operations/observations.
// No recipe/image is assigned directly, no class layout/body is rewritten,
// and no source callback or surface result is replaced by this seam.
struct TypedOuterReceivingAccess {
    static retail::LoadingFrameDevice& surface_device(RaceApplicationPolygonBackend& app){return app.surfaces_;}
    static void seed_completed_platform_submission(RaceApplicationPolygonBackend& app,
        ImmutablePolygonSubmission completed){
        if(app.recipes_.submissions[1]||!completed)throw std::logic_error("receiving back seed requires an empty back and actual submission");
        (void)validate_polygon_submission(*completed);
        app.completed_back(std::move(completed)); //Real capture-backend completion, GPU stage and current gamma snapshot.
    }
    static void flip_first_platform_seed(RaceApplicationPolygonBackend& app){
        if(app.recipes_.submissions[0]||!app.recipes_.submissions[1])throw std::logic_error("receiving seed Flip requires only first back");
        retail::prepare_loading_surface_432a60(app.surfaces_); //Actual required wrapper/adapter/fence exchange.
    }
    static std::array<ImmutablePolygonSubmission,2> submissions(const RaceApplicationPolygonBackend& app){return app.recipes_.submissions;}
    static std::array<SdrGammaRamp,2> ramps(const RaceApplicationPolygonBackend& app){return app.recipes_.ramps;}
    static SdrGammaRamp current_gamma(const RaceApplicationPolygonBackend& app){return app.gamma_.snapshot();}
    static std::uint64_t applies(const RaceApplicationPolygonBackend& app){return app.gamma_.apply_count();}
};
}
