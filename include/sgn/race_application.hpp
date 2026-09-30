#pragma once
#include "sgn/renderer.hpp"
#include "sgn/world_join_options.hpp"
#include <optional>
#include <array>
#include <iosfwd>

#if defined(SGN_CAMERA_RECOVERY_RECEIVING) && SGN_CAMERA_RECOVERY_RECEIVING == 1
#include "sgn/receiving_camera_witness.hpp"
#endif
namespace sgn
{
namespace retail
{
class SourceApplicationLiteralData;
}
enum class RaceApplicationMode
{
    time_trial,
    normal_race,
    challenge
};
struct RaceApplicationOptions
{
    std::string level{"TRACK1"};
    RaceApplicationMode mode{RaceApplicationMode::normal_race};
    bool muted{};
    std::uint32_t challenge_ticks{};
    bool interpolation_enabled{true};
    std::optional<diagnostics::WorldJoinSelection> world_join; // Explicit diagnostic CLI only; absent by default.
};
// New native application glue. All vehicle movement is delegated to the
// recovered fixed-step driver; rendering never advances simulation state.
class RaceApplication
{
    struct Impl;
    std::unique_ptr<Impl> impl_;

  public:
    RaceApplication(const std::filesystem::path &retail_root, const RaceApplicationOptions &, HWND, Renderer &);
    RaceApplication(const std::filesystem::path &retail_root, const RaceApplicationOptions &, HWND, Renderer &,
                    retail::SourceApplicationLiteralData &);
    ~RaceApplication();
    RaceApplication(const RaceApplication &) = delete;
    RaceApplication &operator=(const RaceApplication &) = delete;
    void tick(const std::array<bool, 256> &keys, bool reset, bool scripted, Renderer &);
    void release_input();
    void suspend_renderer_for_recreation();
    void bind_renderer(Renderer &);
    bool ready_to_draw() const noexcept;
    void resize(unsigned width, unsigned height);
    void invalidate_presentation() noexcept;
    void draw(Renderer &, float dpi, bool active, double alpha = 1.0);
    void report(std::ostream &) const;
    // Opt-in CPU dump of the same retained front observed by report().
    void export_displayed_recipe(const std::filesystem::path &) const;
    bool export_displayed_world_join(const std::filesystem::path &recipe, const std::filesystem::path &sidecar) const;
    void export_displayed_front_bundle(const std::filesystem::path &) const;
#if defined(SGN_CAMERA_RECOVERY_RECEIVING) && SGN_CAMERA_RECOVERY_RECEIVING == 1
    diagnostics::CameraRecoverySnapshot capture_camera_recovery_witness(
        std::uint64_t host_clock_tick, const std::filesystem::path &front_bundle) const;
#endif
    std::uint64_t ticks() const noexcept;
    bool menu_requested() const noexcept;
    bool exit_requested() const noexcept;
};
} // namespace sgn
