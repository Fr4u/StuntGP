#include "sgn/vehicle_model_basis.hpp"

namespace sgn::retail
{
void prepare_vehicle_model_basis(const VehicleTrailModel &model, VehicleMatrix &output)
{
    auto x = vehicle_cross(model.base_forward, model.base_up);
    auto y = vehicle_cross(x, model.base_forward);
    x = vehicle_normalize(x);
    y = vehicle_normalize(y);
    x.w = output[3];
    y.w = output[7];
    output = vehicle_basis(x, y, model.base_forward);
}
} // namespace sgn::retail
