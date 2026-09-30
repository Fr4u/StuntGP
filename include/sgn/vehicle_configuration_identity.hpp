#pragma once
#include <array>
#include <span>
namespace sgn::retail
{
// Source vehicle prefix00..1F identifies the configuration format. It is not
// the driver's display name or the filename chosen by level selection.
struct VehicleConfigurationIdentity
{
    std::array<char, 32> header{};
};
void initialize_cold_configuration_identity(std::span<VehicleConfigurationIdentity, 8>); // owned40A2F0 writes,472C60
} // namespace sgn::retail
