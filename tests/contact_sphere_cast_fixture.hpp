#pragma once
#include "sgn/contact_sphere_cast.hpp"
#include <cstring>
namespace sgn::retail::contact::sphere_test {
using Record=std::array<std::uint8_t,0x400>;
inline unsigned word(const Record& b,unsigned at){unsigned v;std::memcpy(&v,b.data()+at,4);return v;}
inline void put(Record& b,unsigned at,unsigned v){std::memcpy(b.data()+at,&v,4);}
struct State {solver::Workspace work;ContactCastState query;ContactCastPayloadAudit audit;ContactQueryInternalsAudit internals_audit;solver::PayloadAudit payload;};
constexpr std::array<unsigned,6> payload_at{0xd0,0xe0,0x134,0x1a4,0x1b4,0x194};
inline void transfer(Record& b,State& s,bool read){
    const auto blob=[&](auto& value,unsigned at){if(read)std::memcpy(&value,b.data()+at,sizeof value);else std::memcpy(b.data()+at,&value,sizeof value);};
    const auto identity=[&](std::optional<std::uint32_t>& value,unsigned at){if(read){const auto id=word(b,at);value=id==0xffffffffu?std::nullopt:std::optional{id};}else put(b,at,value.value_or(0xffffffffu));};
    const auto flag=[&](bool& value,unsigned at){if(read){const auto v=word(b,at);if(v>1)throw FormatError("sphere provenance flag");value=v!=0;}else put(b,at,value?1:0);};
    auto& q=s.query;blob(s.work.query_mass,0xc0);blob(q.start,0xc4);blob(q.end,0xd4);blob(s.work.query_motion,0xe8);blob(q.motion_payload,0xf4);blob(q.step,0x118);blob(q.direction,0x128);
    blob(q.step_length,0x148);blob(q.travel_limit,0x14c);blob(q.remaining,0x150);blob(q.radius,0x154);blob(q.radius_squared,0x158);
    identity(q.internals.plane_vertex,0x15c);identity(q.internals.plane_normal,0x160);identity(q.polygon,0x164);blob(q.internals.endpoint_plane_distance,0x168);blob(q.internals.start_plane_distance,0x16c);
    blob(q.internals.auxiliary,0x188);blob(s.work.last_normal,0x198);blob(q.center,0x1a8);blob(q.surface,0x1b8);blob(q.internals.candidate_distance,0x1c8);blob(q.hit_flag,0x1e4);blob(q.polygon_list,0x23c);blob(q.internals.tree_tier,0x240);
    blob(q.mode,0x300);blob(q.internals.contact_kind,0x304);
    flag(s.audit.start,0x340);flag(s.audit.end,0x344);flag(s.audit.direction,0x348);flag(s.payload.normal,0x34c);flag(s.audit.center,0x350);flag(s.internals_audit.auxiliary,0x354);
}
inline bool masked(const Record& b,unsigned at){for(unsigned j=0;j<payload_at.size();++j)if(at==payload_at[j])return word(b,0x340+j*4)!=0;return false;}
}
