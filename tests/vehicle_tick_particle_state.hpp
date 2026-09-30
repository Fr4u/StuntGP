#pragma once
#include "sgn/contact_engine_particles.hpp"
#include "sgn/contact_effects.hpp"
#include "sgn/contact_payload_audit.hpp"
#include "contact_takeoff_state.hpp"
#include <algorithm>
#include <type_traits>

namespace sgn::retail::oracle::particles {
struct WorldOffsets {
    unsigned impact_pool{},impact_emitted{},impact_cursor{},impact_throttle{},rate_selector{};
    unsigned effect_pool{},effect_signal{},staging{},staging_initialized{};
};
inline constexpr unsigned impact_bytes=0x8800,effect_bytes=0x3400,staging_bytes=0xd0;
inline void validate(std::span<std::uint8_t> bytes,const WorldOffsets& at){
    std::array<std::pair<unsigned,unsigned>,9> ranges{{{at.impact_pool,impact_bytes},{at.impact_emitted,4},{at.impact_cursor,4},{at.impact_throttle,4},{at.rate_selector,4},
        {at.effect_pool,effect_bytes},{at.effect_signal,4},{at.staging,staging_bytes},{at.staging_initialized,4}}};
    for(const auto [start,count]:ranges)if(start%4||start>bytes.size()||count>bytes.size()-start)throw std::runtime_error("tick particle fixture range");
    std::sort(ranges.begin(),ranges.end());for(unsigned j=1;j<ranges.size();++j)if(ranges[j].first-ranges[j-1].first<ranges[j-1].second)throw std::runtime_error("tick particle fixture overlap");
}
inline void scalar(std::span<std::uint8_t> bytes,unsigned at,std::uint32_t& value,bool read){
    if(at>bytes.size()||bytes.size()-at<4)throw std::runtime_error("tick particle fixture scalar range");
    if(read)std::memcpy(&value,bytes.data()+at,4);else std::memcpy(bytes.data()+at,&value,4);
}
inline void transfer_vehicle(std::span<std::uint8_t> bytes,contact::effects::VehicleParticleState& state,bool read){
    if(bytes.size()<0x7724)throw std::runtime_error("tick particle vehicle fixture range");
    scalar(bytes,0x761c,state.field_761c,read);scalar(bytes,0x7620,state.field_7620,read);scalar(bytes,0x7628,state.field_7628,read);
    for(unsigned j=0;j<4;++j){auto value=std::bit_cast<std::uint32_t>(state.field_1ff8[j]);scalar(bytes,0x1ff8+j*0x98,value,read);if(read)state.field_1ff8[j]=std::bit_cast<float>(value);}
}
// Loading source bytes never resets native provenance. Fresh scenarios must
// create fresh owners; continuations serialize their own actual native state.
inline void transfer_world(std::span<std::uint8_t> bytes,const WorldOffsets& at,
    contact::effects::ImpactParticles& impact,contact::effects::EffectParticleBank& effect,contact::effects::EffectStaging& staging,bool read){
    validate(bytes,at);static_assert(sizeof(contact::effects::ImpactParticle)==0x440&&std::is_trivially_copyable_v<contact::effects::ImpactParticle>);
    if(read)std::memcpy(impact.particles.data(),bytes.data()+at.impact_pool,impact_bytes);else std::memcpy(bytes.data()+at.impact_pool,impact.particles.data(),impact_bytes);
    scalar(bytes,at.impact_emitted,impact.emitted,read);scalar(bytes,at.impact_cursor,impact.cursor,read);scalar(bytes,at.rate_selector,impact.rate_selector,read);
    auto throttle=std::bit_cast<std::uint32_t>(impact.throttle);scalar(bytes,at.impact_throttle,throttle,read);if(read)impact.throttle=std::bit_cast<std::int32_t>(throttle);
    contact::takeoff_oracle::transfer_bank(bytes.subspan(at.effect_pool,effect_bytes),effect,read);scalar(bytes,at.effect_signal,effect.active_signal,read);
    contact::takeoff_oracle::transfer(bytes.subspan(at.staging,staging_bytes),staging.particle,read);scalar(bytes,at.staging_initialized,staging.initialized,read);
    if(std::any_of(staging.particle.source_undefined.begin(),staging.particle.source_undefined.end(),[](bool b){return b;}))throw std::runtime_error("global engine staging cannot inherit stack undefinedness");
}
enum class Provenance {impact_position_w,impact_velocity_w,effect_stack_word,effect_frame_word};
struct UndefinedWord {unsigned offset{},source_address{},slot{},field{};Provenance origin{};};
inline std::vector<UndefinedWord> provenance(const WorldOffsets& at,const contact::effects::EffectParticleBank& bank,
    const contact::effects::EffectParticleFrameAudit& frame,const contact::solver::PayloadAudit& impact){
    std::vector<UndefinedWord> result;
    for(unsigned j=0;j<32;++j){
        if(impact.particle_position[j])result.push_back({at.impact_pool+j*0x440+0x14,0x612da4+j*0x440+0x14,j,0x14,Provenance::impact_position_w});
        if(impact.particle_velocity[j])result.push_back({at.impact_pool+j*0x440+0x24,0x612da4+j*0x440+0x24,j,0x24,Provenance::impact_velocity_w});
    }
    for(unsigned j=0;j<64;++j){
        for(unsigned k=0;k<7;++k)if(bank.particles[j].source_undefined[k]){const auto field=contact::takeoff_oracle::undefined_offsets[k];result.push_back({at.effect_pool+j*0xd0+field,0x62a078+j*0xd0+field,j,field,Provenance::effect_stack_word});}
        for(unsigned k=0;k<16;++k)if(frame.undefined[j][k]){const auto field=0x18+k*4;result.push_back({at.effect_pool+j*0xd0+field,0x62a078+j*0xd0+field,j,field,Provenance::effect_frame_word});}
    }
    std::sort(result.begin(),result.end(),[](const auto& a,const auto& b){return a.offset<b.offset;});
    for(std::size_t j=1;j<result.size();++j)if(result[j-1].offset==result[j].offset)throw std::runtime_error("overlapping particle provenance");return result;
}
inline std::vector<unsigned> excluded(const WorldOffsets& at,const contact::effects::EffectParticleBank& bank,
    const contact::effects::EffectParticleFrameAudit& frame,const contact::solver::PayloadAudit& impact){
    std::vector<unsigned> result;for(const auto& word:provenance(at,bank,frame,impact))result.push_back(word.offset);return result;
}
}
