#pragma once
namespace sgn::retail
{
class RaceSessionPlatform;
// 444DC9 mandatory full source clear, on this exact platform's VehicleStorage.
// No default implementation. Existing typed={} reset is not qualified here.
class SourceVehicleClear444dc9
{
  public:
    virtual ~SourceVehicleClear444dc9() = default;
    virtual void clear_444dc9(RaceSessionPlatform &) = 0;
};
} // namespace sgn::retail
