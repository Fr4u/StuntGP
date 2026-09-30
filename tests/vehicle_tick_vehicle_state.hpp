#pragma once
#include "vehicle_tick_visual_state.hpp"
#include "vehicle_tick_particle_state.hpp"
#include "contact_solver_state.hpp"
#include "contact_ai_stunt_state.hpp"
#include "contact_rail_state.hpp"
#include "retail_vehicle_anchors_state.hpp"
#include "retail_vehicle_load_state.hpp"
#include "vehicle_ai_automatic_state.hpp"
#include "vehicle_progression_state.hpp"
#include "vehicle_restart_state.hpp"
#include "vehicle_tick_state.hpp"

namespace sgn::retail::tick_test {
// Corpus identities: models use ModelResources' stable tokens, recordings and
// strip corners use index+1 (zero null). Original pointers are translated by the
// source fixture at its boundary, never stored in production state.
inline void transfer_vehicle(std::span<std::uint8_t> b,VehicleStorage& storage,unsigned slot,bool loading,ModelResources& models) {
    if(b.size()<0x7800||slot>=6)throw FormatError("full tick vehicle record bounds");
    const auto get=[&](unsigned at){std::uint32_t value;std::memcpy(&value,b.data()+at,4);return value;};
    const auto put=[&](unsigned at,std::uint32_t value){std::memcpy(b.data()+at,&value,4);};
    const auto field=[&](auto& value,unsigned at){if(loading)std::memcpy(&value,b.data()+at,sizeof(value));else std::memcpy(b.data()+at,&value,sizeof(value));};
    contact::oracle::transfer(b,storage.parameters[slot],storage.physics[slot],storage.race[slot],storage.contact[slot],loading);
    load_oracle::transfer(b,storage.history[slot],storage.load[slot],loading);
    anchor_oracle::transfer(b,storage.anchors[slot],storage.presentation[slot],storage.trail_inputs[slot],storage.race[slot],loading);
    automatic_oracle::transfer(b,storage.ai[slot],loading);contact::ai_stunt_oracle::transfer(b,storage.ai_stunts[slot],loading);
    contact::rail_oracle::transfer(b,storage.rail[slot],loading);restart_oracle::transfer(b,storage.restart[slot],loading);
    tick_oracle::transfer(b,storage.tick[slot],loading);progression_oracle::transfer(b,storage.progression[slot],loading);
    oracle::particles::transfer_vehicle(b,storage.particles[slot],loading);
    transfer_visual(b,storage,slot,loading,[&](std::uint32_t id){return models.resolve(id);},[&](std::optional<std::size_t> id){return models.identity(id);});
    field(storage.configuration_identity[slot].header,0);field(storage.race[slot].place,0x24ac);
    field(storage.drivers[slot].skill,0x1b8c);field(storage.drivers[slot].mode,0x1b90);
    field(storage.air[slot].completed_airtime,0x7200);field(storage.route_assist[slot].blend,0x265c);
    field(storage.initialization_extras[slot].field_28b0,0x28b0);
    field(storage.model_effects[slot].glow_phases,0x28b4);
    field(storage.startup[slot].field_2280,0x2280);field(storage.startup[slot].fields_768c,0x768c);
    auto& camera=storage.camera_settings[slot];field(camera.orbit_offset,0x22cc);field(camera.target_offset,0x22dc);field(camera.angles,0x22ec);
    field(camera.distance,0x22fc);field(camera.approach_scale,0x2300);field(camera.adaptive_distance,0x2304);
    if(loading){const auto value=get(0x7268);if(value>6)throw FormatError("full tick replay identity");
        storage.race[slot].recording=value?std::optional(ReplayHandle{value-1}):std::nullopt;
    }else put(0x7268,storage.race[slot].recording?storage.race[slot].recording->slot+1:0);
    for(unsigned j=0;j<4;++j){const auto at=0x3964+j*0xd10;auto& ring=storage.skid_rings[slot].items[j];
        field(storage.strips[slot].items[j],at);
        if(loading){const auto value=get(at+12);if(value>512)throw FormatError("full tick strip corner identity");ring.last_corner=value?std::optional(value-1):std::nullopt;}
        else put(at+12,ring.last_corner?*ring.last_corner+1:0);
        static_assert(sizeof(VehicleSkidSegment)==0x34);field(ring.segments,at+16);
    }
}
}
