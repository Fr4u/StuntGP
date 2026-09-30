#pragma once
#include "sgn/retail_vehicle_load_selection.hpp"
#include "sgn/vehicle_audio_state.hpp"
namespace sgn::retail
{
struct VehicleTuningView
{
    VehicleParameters &parameters;
    VehicleState &physics;
    VehicleSoundIds &sounds;
    VehicleAudioState &audio;
};
// Full41F5D0 for authored forward-ratio counts0..6. Other counts would cross
// the native ratio array and are rejected before any write. The source copies
// opaque tuning words; only combined mass uses floating arithmetic. It does
// not rebuild the engine curve or reset histories after applying these values.
void apply_selection_vehicle_tuning(std::span<const SelectionRecord, 6>, std::span<VehicleTuningView, 6>);
} // namespace sgn::retail
