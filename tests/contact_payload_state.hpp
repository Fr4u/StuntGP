#pragma once
#include "sgn/contact_payload_audit.hpp"
#include <algorithm>
#include <vector>

namespace sgn::retail::contact::oracle {
// Conversion only: definedness itself comes from the executed native branches.
// These addresses belong to the test corpus, never to the runtime state model.
inline std::vector<unsigned> payload_mask(const solver::PayloadAudit& audit){
    std::vector<unsigned> result;
    const auto add=[&](unsigned at,bool undefined){if(undefined)result.push_back(at);};
    add(0x3b42c,audit.normal);add(0x3b45c,audit.rotation_delta);
    for(unsigned j=0;j<6;++j){const auto at=j*0x7800;const auto& car=audit.vehicles[j];
        add(at+0x1bb8,car.position);add(at+0x1bc8,car.previous_position);add(at+0x1c98,car.velocity);
        add(at+0x1bd8,car.orientation_first);add(at+0x1c18,car.incremental_first);
        add(at+0x727c,car.body_position);add(at+0x728c,car.body_previous_position);add(at+0x72ac,car.body_velocity);
        add(at+0x72c4,car.body_orientation_first);add(at+0x7304,car.body_incremental_first);add(at+0x7388,car.snapshot_first);
        for(unsigned k=0;k<6;++k){const auto probe=at+0x73b4+k*0x4c;
            add(probe+28,car.probes[k].current);add(probe+44,car.probes[k].previous);add(probe+60,car.probes[k].velocity);}
        for(unsigned k=0;k<4;++k)add(at+0x2038+k*0x98,car.wheel_normal[k]);
    }
    for(unsigned j=0;j<32;++j){add(0x30014+j*0x440,audit.particle_position[j]);add(0x30024+j*0x440,audit.particle_velocity[j]);}
    std::sort(result.begin(),result.end());return result;
}
}
