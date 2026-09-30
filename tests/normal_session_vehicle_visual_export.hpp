#pragma once
// Output-only actual Session owners. No animation, clear, reconstruction or
// source execution occurs here. The two nonphysical presentation slots retain
// their own values; trails and skid rings exist only for the six physical cars.
#include "normal_session_race_export.hpp"

namespace normal_session_owner {
inline void export_vehicle_visual_owners(RaceSession& session,Image& out,NativeAddresses& addresses,const ModelsExport& models){
    using namespace race_export_detail;
    const auto& s=session.state();const auto& bank=s.vehicles;
    static_assert(sizeof(VehicleVector)==16&&sizeof(VehicleMatrix)==64);
    static_assert(sizeof(VehicleStripHeader)==12&&sizeof(VehicleSkidSegment)==0x34&&sizeof(VehicleStripCorner)==16);
    static_assert(sizeof(VehicleSegment)==48&&sizeof(VehicleSegments)==288);
    for(unsigned slot=0;slot<VehicleStorage::presentation_slots;++slot){
        const auto base=add(vehicles,std::size_t(slot)*vehicle_stride);const auto key="visual/vehicle/"+std::to_string(slot)+"/";
        const auto live=[&](const char* name,unsigned offset,const auto& value){field(out,addresses,key+name,base+offset,value);};
        const auto& p=bank.presentation[slot];const auto& m=bank.models[slot];
        live("flag3134",0x3134,p.field_3134);live("flag33c8",0x33c8,p.field_33c8);live("flag3508",0x3508,p.field_3508);
        live("flag3924",0x3924,p.field_3924);live("flag3928",0x3928,p.field_3928);live("idle-hold",0x7614,p.idle_hold);live("idle-interval",0x7618,p.idle_interval);
        const auto model_pointer=[&](const char* name,unsigned offset,const auto& handle){
            const auto target=models.descriptor_for(handle?&session.models().at(*handle):nullptr);
            pointer(out,key+name,base+offset,target,target?104:0);
        };
        model_pointer("body",0x26b0,m.model);model_pointer("shadow",0x26b4,m.shadow);
        live("model-position",0x26b8,m.position);live("model-scale",0x26c8,m.scale);live("model-target",0x26d8,m.target);live("model-up",0x26e8,m.up);
        // 40A3EF copies the outer template. Its +44/+48 words are matrix
        // identities, like the sole world template, not portable raw pointers.
        field(out,addresses,key+"template-tail0",base+0x26f8,m.template_tail[0]);
        field(out,addresses,key+"template-tail3",base+0x2704,m.template_tail[3]);
        for(unsigned i=1;i<=2;++i){
            const auto target=matrix_address(m.template_tail[i]);
            addresses.array(key+"template-matrix/"+std::to_string(i),std::span(&m.template_tail[i],1),base+0x26f8+i*4);
            if(m.template_tail[i]==1)require(addresses.address(&s.shared_model_identity,64)==target,"vehicle template uses another identity matrix");
            else if(target)require(addresses.address(&s.view_matrices.matrices[(m.template_tail[i]-2)/64],64)==target,"vehicle template uses another matrix arena");
            pointer(out,key+"template-matrix/"+std::to_string(i),base+0x26f8+i*4,target,target?64:0);
        }
        live("model2708",0x2708,m.field_2708);live("body-rotation",0x270c,m.body_rotation);
        for(unsigned i=0;i<4;++i){
            const auto prefix=key+"wheel/"+std::to_string(i)+"/";const auto& w=bank.wheels[slot][i];
            field(out,addresses,prefix+"center",base+0x201c+i*0x98,p.wheel_centers[i]);
            field(out,addresses,prefix+"previous-angle",base+0x2070+i*0x98,w.previous_angle);
            field(out,addresses,prefix+"current-angle",base+0x2074+i*0x98,w.current_angle);
            field(out,addresses,prefix+"rotation",base+0x274c+i*64,w.rotation);
        }
        if(slot>=VehicleStorage::physical_slots)continue;
        const auto& t=bank.trails[slot];const auto& input=bank.trail_inputs[slot];const auto& a=bank.anchors[slot];
        const auto& e=bank.model_effects[slot];const auto& shadow=bank.shadows[slot];
        live("trail-input28a0",0x28a0,input.field_28a0);live("speed-trail",0x351c,t.speed);
        live("segments",0x315c,bank.segments[slot].items);
        live("effect-phase",0x225c,e.phase);live("effect-enabled",0x226c,e.enabled);live("effect-brake",0x28a4,e.cached_brake);live("route-envelope",0x7684,e.route_envelope);
        live("glow-phases",0x28b4,e.glow_phases);
        live("anchor-position",0x3138,a.position);live("anchor-forward",0x3148,a.forward);live("anchor-extent",0x3158,a.extent);
        live("shadow-factor392c",0x392c,shadow.factor_392c);live("shadow-factor3930",0x3930,shadow.factor_3930);
        live("shadow-position",0x3934,shadow.position);live("shadow-normal",0x3944,shadow.normal);live("shadow-forward",0x3954,shadow.forward);
        constexpr std::array<unsigned,2> trail_a{0x2d14,0x2f2c},trail_b{0x28d0,0x2aec};
        constexpr std::array<unsigned,2> input_a{0x2d04,0x2f1c},input_b{0x28c0,0x2adc};
        constexpr std::array<unsigned,2> cursor_a{0x2f14,0x312c},cursor_b{0x2ad0,0x2cec},available_b{0x28bc,0x2ad8};
        for(unsigned i=0;i<2;++i){
            const auto prefix=key+"pair/"+std::to_string(i)+"/";
            const auto emit=[&](const char* name,unsigned offset,const auto& value){field(out,addresses,prefix+name,base+offset,value);};
            emit("trail-a",trail_a[i],t.pair_a[i]);emit("trail-b",trail_b[i],t.pair_b[i]);
            emit("input-a",input_a[i],input.pair_a[i]);emit("input-b",input_b[i],input.pair_b[i]);
            emit("cursor-a",cursor_a[i],t.cursor_a[i]);emit("cursor-b",cursor_b[i],t.cursor_b[i]);emit("available-b",available_b[i],input.pair_b_available[i]);
            emit("effect-scale",0x32b0+i*0x140,e.current_scale[i]);emit("effect-trail",0x32c0+i*0x140,e.trails[i]);emit("effect-cursor",0x33c0+i*0x140,e.cursor[i]);
            emit("anchor-available",0x328c+i*0x140,a.effect_available[i]);emit("anchor-position",0x3290+i*0x140,a.effect_position[i]);emit("anchor-scale",0x32a0+i*0x140,a.effect_base_scale[i]);
        }
        for(unsigned i=0;i<4;++i){
            const auto at=base+0x3964+i*0xd10;const auto prefix=key+"skid/"+std::to_string(i)+"/";
            const auto& ring=bank.skid_rings[slot].items[i];
            field(out,addresses,prefix+"header",at,bank.strips[slot].items[i]);
            field(out,addresses,prefix+"segments",at+16,ring.segments);
            require(!ring.last_corner||*ring.last_corner<s.strip.corners.size(),"skid corner outside actual shared geometry");
            const auto target=ring.last_corner?addresses.address(&s.strip.corners[*ring.last_corner],sizeof(VehicleStripCorner)):0;
            pointer(out,prefix+"last-corner",at+12,target,target?sizeof(VehicleStripCorner):0);
        }
    }
    field(out,addresses,"visual/model-basis",0x4d5b30,s.model_basis);
    field(out,addresses,"visual/random",0x4959c0,s.random.state);
}
}
