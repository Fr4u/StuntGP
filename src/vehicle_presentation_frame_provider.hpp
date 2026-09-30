#pragma once
#include "sgn/retail_vehicle_presentation_frame.hpp"

namespace sgn::retail::detail {
// Optional synchronous receiving interposition. No persistent state or opt-in
// flag is added to Session. Null is the ordinary production entry.
struct PresentationCallEvent {
    std::uint32_t site,entry,slot;
    bool after;
};
struct PresentationCallAudit {
    void* context{};
    void (*boundary)(void*,PresentationCallEvent){};
};
VehiclePresentationDraws prepare_vehicle_presentation_frame_observed(VehiclePresentationFrame,PresentationCallAudit*);
void render_vehicle_pair_b_glows_observed(VehicleTrailRenderBindings,PresentationCallAudit*);
}
