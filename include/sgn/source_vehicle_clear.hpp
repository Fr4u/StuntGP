#pragma once
#include "sgn/source_car_selection.hpp"

namespace sgn::retail
{
struct RaceSessionState;
// 40A305 REP: six existing complete records, then two nonphysical tails.
void clear_source_cold_vehicle_records_40a305(RaceSessionState &) noexcept;
// Concrete 444DC9 child. Borrows the existing platform and its existing world;
// the platform must outlive this object. Construction performs no source clear.
// Admitted source DF=0; all access remains on the platform's owner thread.
// No callbacks/parallel readers may observe the middle of the source REP.
class SourceVehicleClear444dc9Impl final : public SourceVehicleClear444dc9
{
    RaceSessionPlatform &platform_;

  public:
    explicit SourceVehicleClear444dc9Impl(RaceSessionPlatform &);
    SourceVehicleClear444dc9Impl(const SourceVehicleClear444dc9Impl &) = delete;
    SourceVehicleClear444dc9Impl &operator=(const SourceVehicleClear444dc9Impl &) = delete;
    void clear_444dc9(RaceSessionPlatform &) override;
};
} // namespace sgn::retail
