#pragma once
#include "vehicle_presentation_frame_state.hpp"
#include "contact_takeoff_state.hpp"
#include "sgn/retail_scene_entity_presentation.hpp"
#include <cmath>

namespace sgn::retail::scene_test {
using namespace presentation_test;
struct State : presentation_test::State {
    contact::effects::EffectParticleBank effects;
    contact::effects::ImpactParticles impacts;
    contact::effects::ImpactParticleDrawList impact_draws;
    std::array<contact::effects::EffectAnimationTexture,4> animations{};
    std::map<std::uint32_t,float> adjacent_values;
    std::vector<contact::effects::EffectAngleSample> adjacent;
    contact::effects::EffectSpriteColorAudit color_audit;
    using presentation_test::State::State;
    SceneEntityPresentation scene_bindings(){return {bindings(),effects,impacts,impact_draws,animations,adjacent,&color_audit};}
    void input_provenance(const SourceImage& image,const std::set<std::uint32_t>& tags){
        const auto& globals=image.region(0);const auto& attributes=image.region(3,3);
        for(unsigned slot=0;slot<64;++slot)for(unsigned field=0;field<7;++field)
            effects.particles[slot].source_undefined[field]=tags.contains(globals.offset+0x62a078-globals.address+
                slot*0xd0+contact::takeoff_oracle::undefined_offsets[field]);
        for(unsigned i=0;i<2048;++i)color_audit.source_undefined[i]=tags.contains(attributes.offset+i*16+12);
    }
    void transfer(SourceImage& image,bool input){
        presentation_test::State::transfer(image,input);
        contact::takeoff_oracle::transfer_bank(image.at(0x62a078,0x3400),effects,input);
        image.field(effects.active_signal,0x62d478,input);
        static_assert(sizeof(contact::effects::ImpactParticle)==0x440);
        image.field(impacts.particles,0x612da4,input);
        image.field(impacts.emitted,0x612d98,input);image.field(impacts.cursor,0x612d9c,input);
        image.field(impacts.throttle,0x612da0,input);image.field(impacts.rate_selector,0x61297c,input);
        for(unsigned i=0;i<32;++i){
            using Codec=contact::effects::ImpactParticleDrawList;
            if(input)impact_draws.slots[i]=Codec::decode_slot(image.get(0x61b5a4+i*4));
            else image.put(0x61b5a4+i*4,Codec::encode_slot(impact_draws.slots[i]));
        }
        using Codec=contact::effects::ImpactParticleDrawList;
        if(input)impact_draws.count=Codec::decode_cursor(image.get(0x61b624));
        else image.put(0x61b624,Codec::encode_cursor(impact_draws.count));
        const auto animation_base=image.region(6).address;
        for(unsigned i=0;i<4;++i){
            if(input){const auto pointer=image.get(animation_base+i*16);
                require(pointer>=0x6227d4&&(pointer-0x6227d4)%28==0,"scene animation texture row");
                animations[i]={animation_base+i*16,(pointer-0x6227d4)/28};
            }else {require(animations[i].identity==animation_base+i*16,"scene native animation identity");
                image.put(animation_base+i*16,0x6227d4+animations[i].first_texture*28);}
        }
        if(input){
            adjacent.clear();adjacent_values.clear();
            for(const auto& particle:effects.particles){
                if(particle.active!=1||particle.render_enabled!=1||!(particle.flags&4))continue;
                const auto value=double(particle.angle)*std::bit_cast<float>(std::uint32_t{0x41b60b61});
                require(std::isfinite(value)&&value>=-9223372036854775808.0&&value<9223372036854775808.0,"scene angle input domain");
                const auto index=static_cast<std::uint32_t>(static_cast<std::int64_t>(value));
                for(auto sample:std::array{index,index+2048u})if(sample>=10240)
                    adjacent_values[sample]=std::bit_cast<float>(image.get(0x48a5d8+sample*4));
            }
            for(const auto& [index,value]:adjacent_values)adjacent.push_back({index,value});
        }
    }
};
}
