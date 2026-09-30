#pragma once
#include "sgn/retail_race.hpp"
#include "sgn/source_camera_spline.hpp"
#include "sgn/track_spline_source.hpp"
#include "sgn/retail_level_selection.hpp"

namespace sgn::retail
{
// AI-owned words only. Shared physical/race fields remain in their owners.
struct AiAutomaticMemory
{
    std::int32_t reverse_ticks{};                          // 2664
    float bend{}, lateral_fraction{};                      // 2668,266C
    std::uint32_t counter2674{};                           // 2674
    std::int32_t passing_ticks{};                          // 268C
    std::uint32_t counter2694{};                           // 2694
    std::int32_t following_ticks{}, steering_ramp_ticks{}; // 26A0,26A4
    std::uint32_t start_throttle_ticks{};                  // 26AC,407F60 countdown pulse
    std::optional<std::size_t> passing_target;             // 2688, source vehicle pointer
    float passing_offset{};                                // 2690
    std::uint32_t turbo_cooldown{}, turbo_rest{};          // 2698,269C
};
struct AiAutomaticVehicle
{
    const VehicleParameters &parameters;
    VehicleState &physical;
    VehicleRaceState &race;
    AiAutomaticMemory &ai;
};
// One source route cache, with disjoint compatibility/source representations.
// Source writes publish each nullable descriptor independently in405260 order.
class AiRouteBindings final
{
    using Native = std::optional<std::array<SplineView, 3>>;
    using Source = std::array<CameraSplineBinding, 3>;
    std::variant<Native, Source> value_;

  public:
    AiRouteBindings() = default;
    AiRouteBindings &operator=(std::array<SplineView, 3> value)
    {
        value_ = Native{value};
        return *this;
    }
    void reset() noexcept
    {
        value_ = Native{};
    }
    explicit operator bool() const noexcept
    {
        if (value_.index() == 0)
            return std::get<0>(value_).has_value();
        const auto &a = std::get<1>(value_);
        return bool(a[0]) && bool(a[1]) && bool(a[2]);
    }
    const std::array<SplineView, 3> &operator*() const
    {
        if (value_.index() != 0)
            throw SourceAllocationError(0x405260, "source AI routes require their actual PMD receiver");
        return std::get<0>(value_).value();
    }
    // NEW R18: representation query for spline_resolution (no payload read).
    bool source_form() const noexcept
    {
        return value_.index() == 1;
    }
    const CameraSplineBinding &source(unsigned index) const
    {
        if (index >= 3)
            throw SourceAllocationError(0x405260, "source AI route index exceeds three owners");
        if (value_.index() != 1)
            throw SourceAllocationError(0x405260, "AI route cache has no source representation");
        return std::get<1>(value_)[index];
    }
    void bind_source(unsigned index, std::optional<SourceCameraSplineRef> value)
    {
        if (index >= 3)
            throw SourceAllocationError(0x405260, "source AI route index exceeds three owners");
        if (value_.index() != 1)
        {
            if (std::get<0>(value_))
                throw SourceAllocationError(0x405260,
                                            "cannot replace live compatibility AI routes with source identities");
            value_ = Source{};
        }
        std::get<1>(value_)[index].bind_source(std::move(value));
    }
};
struct AiAutomaticGlobals
{
    std::uint32_t initialized{};            // 49A164, exactly 1
    AiRouteBindings routes;                 // 49A158/15C/160
    std::array<float, 3> route_distances{}; // 49A16C/170/174
    Vec3 target{};                          // 49A148/14C/150
    std::uint32_t target_opaque{};          // 49A154, pit copies authored W; route target W is undefined
    float target_speed{}, response_scale{}; // 499FA8/499FB0
    std::uint32_t braking_allowed{};        // 499FAC
    std::optional<unsigned> ranking_tail;   // 612D2C, BSS word read after six ranking entries
};
struct AiAutomaticEnvironment
{
    Vec3 world_up;                                  // 482688/68C/690
    float energy_exempt_speed{}, automatic_speed{}; // 60F2EC/60F2F0
    std::uint32_t game_type{};                      // 60DF98, not 60F2B0
};
namespace contact::player
{
struct Controls;
}
struct AiPlayerPrelude
{
    const contact::player::Controls &controls;
    std::span<const LevelCarSlot, 6> cars;
    VehicleRandom &random;
};
// The ordinary branch needs additional authoritative owners; a caller which
// only qualifies automatic driving may leave this boundary unbound.
struct AiRivalStep
{
    virtual ~AiRivalStep() = default;
    virtual void tick(unsigned current) = 0;
};
// Full407F60. The countdown belongs to the currently selected controller car,
// while the pulse, authored skill and RNG branch belong to the current car.
void update_player_start_throttle(std::span<AiAutomaticVehicle, 6>, unsigned current, const AiPlayerPrelude &);
// The route cache stores identities published by the track owner (R18: via
// TrackSplineSource::publish_ai_routes); readers resolve them per call.
// Replacing a level requires an explicitly reset cache, matching its lifecycle.
void initialize_ai_automatic(const TrackSplineSource &, std::span<AiAutomaticVehicle, 6>,
                             AiAutomaticGlobals &);                              // full 405260
Vec3 sample_ai_route(const AiAutomaticGlobals &, float distance, float lateral); // 406870; undefined W absent
void choose_ai_route_target(AiAutomaticVehicle, AiAutomaticGlobals &);           // 406B10
void choose_ai_route_speed(AiAutomaticVehicle, AiAutomaticGlobals &, const AiAutomaticEnvironment &); // 406970
void apply_ai_pedals(AiAutomaticVehicle, AiAutomaticGlobals &); // 406610, calls real 40CC90
// Executes 408000's init/common guards and either automatic or ordinary AI.
// The ordinary branch requires its additional authoritative rival bindings.
// No delta time: one original simulation tick. Source early returns remain valid.
void ai_automatic_tick(const TrackSplineSource &, std::span<AiAutomaticVehicle, 6>, std::size_t self,
                       AiAutomaticGlobals &, const AiAutomaticEnvironment &, const AiPlayerPrelude *player = nullptr,
                       AiRivalStep *rival = nullptr);
} // namespace sgn::retail
