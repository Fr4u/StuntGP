#pragma once
#include "sgn/retail_scene_transition.hpp"
#include "sgn/source_cold_scene_tail.hpp"
#include "sgn/source_frontend_initialization.hpp"
namespace sgn::retail
{
// NEW synchronous adapter over the actual application owners. Construction is
// inert; initialize_scene owns the 60D61C store before the43D0B0 call. The Main
// receiver performs all remaining audio/configuration/allocation/clock work.
class SourceColdMenuSceneServices final : public SceneTransitionServices
{
    SourceColdSceneStartupContext cold_;
    LightGammaDevice *const &gamma_;
    SourceMenuUpdateChildren *update_{};
    SourceNavigationPageServices *navigation_{};
    PersistenceServices &persistence_;
    SourceFrontendDataStartup &data_;
    SceneTransitionServices &remaining_;
    void require_same_world() const;

  public:
    SourceColdMenuSceneServices(SourceColdSceneStartupContext, LightGammaDevice *const &source_gamma_5da1dc,
                                PersistenceServices &, SourceFrontendDataStartup &, SceneTransitionServices &remaining);
    // NEW HOST one-time attachment of the actual menu after device production.
    // No source page creation, prefix initialization or synthetic receiver.
    void attach_menu(SourceMenuUpdateChildren &, SourceNavigationPageServices &);
    void perform(SceneOperation) override;
    void select_audio_environment(std::uint32_t) override;
    void initialize_menu_viewport(std::uint32_t) override;
    void reset_tick_clock() override;
};
} // namespace sgn::retail
