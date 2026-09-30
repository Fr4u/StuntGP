#pragma once
#include <array>

namespace sgn::retail::contact::solver
{
// Optional qualification diagnostics only. No values here control physics or
// replace a stored fourth word. true means the original source word derives
// from an unwritten stack slot; false means it remains exactly comparable.
// A fresh fixture starts entirely defined, independently of its bit patterns.
struct ProbePayloadAudit
{
    bool current{}, previous{}, velocity{};
};
struct VehiclePayloadAudit
{
    bool position{}, previous_position{}, velocity{}, orientation_first{}, incremental_first{};
    bool body_position{}, body_previous_position{}, body_velocity{}, body_orientation_first{}, body_incremental_first{},
        snapshot_first{};
    std::array<ProbePayloadAudit, 6> probes{};
    std::array<bool, 4> wheel_normal{};
};
struct PayloadAudit
{
    std::array<VehiclePayloadAudit, 6> vehicles{};
    std::array<bool, 32> particle_position{}, particle_velocity{};
    bool normal{}, rotation_delta{};
};
} // namespace sgn::retail::contact::solver
