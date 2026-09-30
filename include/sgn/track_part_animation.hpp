#pragma once
#include "sgn/assets.hpp"
#include "sgn/vehicle_math.hpp"
#include <optional>

namespace sgn::retail
{
struct TrackPartAnimation
{
    std::uint32_t kind{}; // source576CD0 +part*48
    VehicleVector angles{}, angular_velocity{}, parameters{};
    std::uint32_t parameter_34{}, phase_u{}, phase_v{}, retained_40{}, parameter_44{};
};
static_assert(sizeof(TrackPartAnimation) == 72 && offsetof(TrackPartAnimation, phase_u) == 0x38);
// The state-copy portion of426260; preserves3C/40. Geometry regeneration by
// 425DF0 and the later426ED0 presentation dispatch are separate resource work.
void load_track_part_animation_records(std::span<TrackPartAnimation>, std::span<const std::uint8_t> pmd);
class TrackPartAnimationBank
{
    std::vector<TrackPartAnimation> records_;

  public:
    TrackPartAnimationBank(); // new host allocation of all32 persistent source slots, no426260 copy
    explicit TrackPartAnimationBank(std::span<const std::uint8_t> track_pmd);
    std::span<TrackPartAnimation> records() noexcept
    {
        return records_;
    }
    std::span<const TrackPartAnimation> records() const noexcept
    {
        return records_;
    }
};
// Complete427140 and427010/427030/427040/427050. Count is borrowed from the
// actual slot3 model descriptor; null represents its original absent pointer.
// Part0 is retained. The passed part pointer in427050 is not read by the source.
void advance_track_part_animations(std::span<TrackPartAnimation>, std::optional<std::uint32_t> model_part_count);
class SourcePmdModelView;
class ModelResources;
// Optional synchronous diagnostics after actual metadata/kind reads and record writes.
struct TrackPhaseAudit
{
    void *context{};
    void (*after)(void *, std::uint32_t instruction, std::uint32_t index, std::uint32_t value) noexcept {};
};
// Full427140 with source-order metadata and per-current-record admission.
// Null registry handle returns without registry/PMD/record access. Neither route
// dereferences the source raw head:427050 never uses its supplied part pointer.
void advance_track_part_animations(std::span<TrackPartAnimation>, SourcePmdModelView, TrackPhaseAudit * = nullptr);
void advance_track_part_animations(std::span<TrackPartAnimation>, ModelResources &, std::optional<std::size_t> handle,
                                   TrackPhaseAudit * = nullptr);

} // namespace sgn::retail
