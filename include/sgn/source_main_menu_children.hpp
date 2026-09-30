#pragma once
#include "sgn/source_menu_language_binding.hpp"
#include "sgn/source_mode2_route.hpp"

namespace sgn::retail
{
class SourceMenuSampleNative;
class NativeSourceMenuCrt;
struct SourceApplicationShutdownServices;
struct SourceSavePageServices;
// Required remaining application/source implementations. Demo and replay-time
// defaults call the existing recovered children. Page/mode bindings remain
// mandatory; the actual menu binds exit directly to its retained CRT/shutdown.
struct SourceMainMenuRequiredServices
{
    virtual ~SourceMainMenuRequiredServices() = default;
    virtual std::uint32_t create_remaining_page(FrontendState &, std::uint32_t key, std::uint32_t kind) = 0;
    virtual void close_remaining_page(FrontendState &, std::uint32_t page, std::uint32_t callback) = 0;
    virtual void remaining_page_event(FrontendState &, std::uint32_t page, std::uint32_t callback, std::uint32_t event,
                                      std::uint32_t argument) = 0;
    virtual void run_remaining_mode(FrontendState &, std::uint32_t page, std::uint32_t entry) = 0;
    virtual std::uint32_t &demo_index_627fdc(FrontendState &);
    virtual void load_demo_43b0e0(FrontendState &, std::uint32_t record);
    virtual std::uint32_t replay_time_word_45811b(FrontendState &, std::uint32_t identity);
};
// Inert composition; all references are existing App-lifetime owners. The
// inherited language/draw/navigation layers retain their current source banks.
// Actual frontend, CRT and shutdown borrows must outlive every menu use.
// The Core caller supplies its own frontend and the Composition RetiringShutdown;
// source exit ends with the host termination signal. External App cleanup
// remains mandatory after native callbacks have returned.
class SourceMainMenuChildren final : public SourceMenuLanguageChildren,
                                     public SourceGameLoopModeServices,
                                     public SourceMode2RouteServices
{
    FrontendState &frontend_;
    NativeSourceMenuCrt &crt_;
    SourceApplicationShutdownServices &shutdown_;
    CampaignScoreMusic &music_;
    FrontendPerformanceCounter &counter_;
    SourceMenuSampleNative &samples_;
    SourceSavePageServices &save_;
    SourceMainMenuRequiredServices &required_;

  protected:
    std::uint32_t create_other_page(FrontendState &, std::uint32_t, std::uint32_t) final;
    void close_other_page(FrontendState &, std::uint32_t, std::uint32_t) final;
    void remaining_selection_page_event(FrontendState &, std::uint32_t, std::uint32_t, std::uint32_t,
                                        std::uint32_t) final;

  public:
    SourceMainMenuChildren(FrontendState &actual_frontend, NativeSourceMenuCrt &,
                           SourceApplicationShutdownServices &same_retiring_shutdown, MenuKeyboardAcquisition &,
                           Win32MenuDirectInputStartup &, CampaignScoreMusic &, SessionOuterFrameServices &,
                           PolygonSubmissionSink &, SourceFrontendMaterialBinding &, std::uint8_t &,
                           FrontendPerformanceCounter &, SourceMenuSampleNative &, SourceSavePageServices &,
                           SourceMainMenuRequiredServices &);
    void feedback_440880(FrontendState &, std::uint32_t) final;
    void feedback_440890(FrontendState &, std::uint32_t) final;
    void maintain_audio_462280(FrontendState &) final;
    void prepare_camera_402e90(FrontendState &) final;
    void prepare_geometry_44fd60(FrontendState &) final;
    std::uint32_t &demo_index_627fdc(FrontendState &) final;
    void load_demo_43b0e0(FrontendState &, std::uint32_t) final;
    void exit_462dfa(FrontendState &, std::uint32_t) final;
    void run_mode(FrontendState &, std::uint32_t, std::uint32_t) final;
    std::uint32_t car_available_437230(FrontendState &, std::uint32_t, std::uint32_t) final;
    std::uint32_t track_available_4371c0(FrontendState &, std::uint32_t, std::uint32_t) final;
    void rank_car_437cc0(FrontendState &, std::uint32_t) final;
    std::uint32_t format_name_46280c(FrontendState &, FrontendLanguageTextIdentity, std::uint32_t,
                                     std::span<std::uint8_t, 64>) final;
    void prepare_race_43a780(FrontendState &, std::uint32_t) final;
    void finish_race_43d500(FrontendState &, std::uint32_t) final;
    // Matches Camera's pending SaveDraw hook once Main inserts that layer.
    virtual std::uint32_t replay_time_word_45811b(FrontendState &, std::uint32_t) final;
};
} // namespace sgn::retail
