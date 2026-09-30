#pragma once
#include "sgn/vehicle_trails.hpp"
namespace sgn::retail::detail {
//Synchronous borrow: never retain context or callback beyond this stage call.
struct PairTrailProvider {
    const void* context;
    VehicleTrailModel (*observe_basis)(const void*);
    VehicleTrailModel basis_inputs() const{return observe_basis(context);}
};
void update_pair_a(VehiclePairTrailBindings,PairTrailProvider);
void update_pair_b(VehiclePairTrailBindings,PairTrailProvider);
}
