#pragma once
#include "sgn/vehicle_trails.hpp"
namespace sgn::retail::detail {
//Private synchronous borrow. The append core never retains context/callbacks.
//Separating this bridge keeps existing single-TU legacy oracle builds linkable.
struct SpeedTrailProvider {
    const void* context;
    float (*point_z)(const void*);
    VehicleTrailModel (*basis)(const void*);
    float position_z() const{return point_z(context);}
    VehicleTrailModel basis_inputs() const{return basis(context);}
};
void update_speed_trail(VehicleSpeedTrailBindings,SpeedTrailProvider);
}
