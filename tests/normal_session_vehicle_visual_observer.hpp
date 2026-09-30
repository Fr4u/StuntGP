#pragma once
// Raw object observations. No source addresses, exporter helpers or producer.
#include "normal_session_native_observer.hpp"

namespace normal_native_observer {
inline void capture_vehicle_visual(RaceSession& session,const std::filesystem::path& output){
    const auto& s=session.state();const auto& b=s.vehicles;Observations out;
    for(unsigned slot=0;slot<b.presentation.size();++slot){
        const auto key="vehicle/"+std::to_string(slot)+"/";const auto& p=b.presentation[slot];const auto& m=b.models[slot];
        const auto raw=[&](const char* name,const auto& value){out.ref(key+name,value);};
        raw("presentation/3134",p.field_3134);raw("presentation/33c8",p.field_33c8);raw("presentation/3508",p.field_3508);
        raw("presentation/3924",p.field_3924);raw("presentation/3928",p.field_3928);raw("presentation/idle_hold",p.idle_hold);raw("presentation/idle_interval",p.idle_interval);
        raw("model/position",m.position);raw("model/scale",m.scale);raw("model/target",m.target);raw("model/up",m.up);
        raw("model/tail",m.template_tail);raw("model/word2708",m.field_2708);raw("model/body_rotation",m.body_rotation);
        const auto handle=[&](const char* name,const auto& value){
            const auto prefix=key+"model/"+name+"/";
            out.value(prefix+"present",std::uint32_t(value.has_value()));
            out.value(prefix+"handle",static_cast<std::uint64_t>(value.value_or(0)));
            out.value(prefix+"identity",session.models().identity(value));
            // An actual independently observed field identity of this ModelResource.
            out.value(prefix+"detail_address",value?reinterpret_cast<std::uint64_t>(&session.models().at(*value).current_detail_level()):std::uint64_t{0});
        };
        handle("body",m.model);handle("shadow",m.shadow);
        for(unsigned i=0;i<b.wheels[slot].size();++i){
            const auto prefix=key+"wheel/"+std::to_string(i)+"/";const auto& w=b.wheels[slot][i];
            out.ref(prefix+"center",p.wheel_centers[i]);out.ref(prefix+"previous",w.previous_angle);out.ref(prefix+"current",w.current_angle);out.ref(prefix+"rotation",w.rotation);
        }
        if(slot>=b.trails.size())continue;
        const auto& t=b.trails[slot];const auto& in=b.trail_inputs[slot];const auto& a=b.anchors[slot];const auto& e=b.model_effects[slot];const auto& shadow=b.shadows[slot];
        raw("trails/speed",t.speed);raw("trails/input_word",in.field_28a0);raw("segments",b.segments[slot].items);
        raw("effects/phase",e.phase);raw("effects/enabled",e.enabled);raw("effects/brake",e.cached_brake);raw("effects/route",e.route_envelope);raw("effects/glow",e.glow_phases);
        raw("anchor/position",a.position);raw("anchor/forward",a.forward);raw("anchor/extent",a.extent);
        raw("shadow/factor392c",shadow.factor_392c);raw("shadow/factor3930",shadow.factor_3930);raw("shadow/position",shadow.position);raw("shadow/normal",shadow.normal);raw("shadow/forward",shadow.forward);
        for(unsigned i=0;i<t.pair_a.size();++i){
            const auto prefix=key+"pair/"+std::to_string(i)+"/";
            out.ref(prefix+"a",t.pair_a[i]);out.ref(prefix+"b",t.pair_b[i]);out.ref(prefix+"input_a",in.pair_a[i]);out.ref(prefix+"input_b",in.pair_b[i]);
            out.ref(prefix+"cursor_a",t.cursor_a[i]);out.ref(prefix+"cursor_b",t.cursor_b[i]);out.ref(prefix+"available_b",in.pair_b_available[i]);
            out.ref(prefix+"scale",e.current_scale[i]);out.ref(prefix+"trail",e.trails[i]);out.ref(prefix+"cursor",e.cursor[i]);
            out.ref(prefix+"anchor_available",a.effect_available[i]);out.ref(prefix+"anchor_position",a.effect_position[i]);out.ref(prefix+"anchor_scale",a.effect_base_scale[i]);
        }
        for(unsigned i=0;i<b.skid_rings[slot].items.size();++i){
            const auto prefix=key+"skid/"+std::to_string(i)+"/";const auto& ring=b.skid_rings[slot].items[i];
            out.ref(prefix+"header",b.strips[slot].items[i]);out.ref(prefix+"segments",ring.segments);
            if(ring.last_corner&&*ring.last_corner>=s.strip.corners.size())throw std::runtime_error("raw skid corner outside actual geometry");
            out.value(prefix+"present",std::uint32_t(ring.last_corner.has_value()));out.value(prefix+"index",ring.last_corner.value_or(0));
            out.value(prefix+"address",ring.last_corner?reinterpret_cast<std::uint64_t>(&s.strip.corners[*ring.last_corner]):std::uint64_t{0});
        }
    }
    out.ref("model_basis",s.model_basis);out.ref("random",s.random.state);out.save(output);
}
}
