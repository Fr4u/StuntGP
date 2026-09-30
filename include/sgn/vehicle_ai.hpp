#pragma once
#include "sgn/assets.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace sgn::retail
{
// Recovered tactical AI and target steering. Spline projection, route sampling,
// throttle/braking, stunts and the complete 00408000 tick are not implemented.
// Fields are observations in retail units, not a second physics simulation.
struct AiRacer
{
    std::int32_t active{};                                       // +1ba8, exactly 1 is active
    Vec3 position{}, right{}, forward{};                         // +1bac, +1cbc, +1cdc
    Vec3 route_position{}, route_right{};                        // +24b0, +24d0
    float speed{}, throttle{}, route_progress{};                 // +1cf0, +1cf8, +23b0
    std::int32_t contact_class{};                                // +1fd8
    float bend{};                                                // +2668
    std::int32_t race_index{};                                   // +23a4, not inferred as lap
    std::array<std::uint32_t, 6> rival_counters{};               // +25f4
    std::optional<std::size_t> passing_target{};                 // +2688 pointer -> slot
    std::int32_t passing_ticks{};                                // +268c
    float passing_offset{};                                      // +2690
    std::int32_t following_ticks{};                              // +26a0
    float steering{}, max_steering{}, up_y{};                    // +1cf4, +024c, +1cd0
    float driving_factor{}, steering_divisor_signal{};           // +2660, +289c
    std::int32_t steering_ramp_ticks{}, reverse_ticks{}, gear{}; // +26a4, +2664, +1f10
};
using AiField = std::array<AiRacer, 6>; // Fixed six-slot traversal in retail EXE.
struct AiTactics
{
    Vec3 target{};                          // globals 0049a148..150
    float target_speed{}, response_scale{}; // 00499fa8, 00499fb0
    float spline_length{}, course_length{}; // [00571328]+18, 0060f010
    Vec3 world_up{};                        // globals 00482688..690
};

// Retail selection returns null if no eligible opponent exists. This API
// represents it explicitly; tactical callers reject missing required rivals
// instead of reproducing the retail null dereference. Inputs must be finite.
std::optional<std::size_t> ai_select_ahead(const AiField &, std::size_t self);  // 00407a70..00407b1c
std::optional<std::size_t> ai_select_behind(const AiField &, std::size_t self); // 00407b20..00407c0d
void ai_choose_passing_side(AiField &, std::size_t self);                       // 004073e0..0040750b
void ai_follow_rival(AiField &, std::size_t self, AiTactics &);                 // 00407c10..00407dca
void ai_pass_rival(AiField &, std::size_t self, AiTactics &);                   // 00407510..004077ac
void ai_yield_to_rival(AiField &, std::size_t self, AiTactics &);               // 00407de0..00407f58
void ai_steer_to_target(AiField &, std::size_t self, AiTactics &); // 00406700..0040686f, calls 004143e0 and 00414420
void ai_recover_stalled(AiField &, std::size_t self);              // 00405b50..00405bd8
} // namespace sgn::retail
