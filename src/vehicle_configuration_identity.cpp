#include "sgn/vehicle_configuration_identity.hpp"
#include <algorithm>
namespace sgn::retail
{
void initialize_cold_configuration_identity(std::span<VehicleConfigurationIdentity, 8> identities)
{
    for (auto &identity : identities)
        identity = {};
    static constexpr char literal[] = "VehicleConfig v0.5";
    static_assert(sizeof(literal) == 19);
    std::copy(std::begin(literal), std::end(literal), identities[0].header.begin());
}
} // namespace sgn::retail
