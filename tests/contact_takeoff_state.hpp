#pragma once
#include "sgn/contact_takeoff_particles.hpp"
#include <bit>
#include <cstring>
#include <span>
#include <stdexcept>
#include <vector>

namespace sgn::retail::contact::takeoff_oracle {
inline constexpr unsigned slot_size=0xd0;
inline constexpr std::array<unsigned,7> undefined_offsets{0x64,0x90,0x94,0xc0,0xc4,0xc8,0xcc};
// Fixture-only codec. No retail offset or address becomes runtime object storage.
// Reading words preserves metadata; a fresh fixture owner must reset the bank.
inline void transfer(std::span<std::uint8_t> b,effects::EffectParticle& p,bool read){
    if(b.size()!=slot_size)throw std::runtime_error("particle fixture size");
    const auto u=[&](std::uint32_t& value,unsigned at){if(read)std::memcpy(&value,b.data()+at,4);else std::memcpy(b.data()+at,&value,4);};
    const auto f=[&](float& value,unsigned at){auto bits=std::bit_cast<std::uint32_t>(value);u(bits,at);if(read)value=std::bit_cast<float>(bits);};
    const auto v=[&](Vector4& value,unsigned at){f(value.x,at);f(value.y,at+4);f(value.z,at+8);u(value.opaque,at+12);};
    u(p.active,0);u(p.flags,4);v(p.position,8);
    for(unsigned j=0;j<16;++j)u(p.frame_payload[j],0x18+j*4);
    v(p.velocity,0x58);f(p.field_68,0x68);f(p.gravity_scale,0x6c);
    u(p.age,0x70);u(p.lifetime,0x74);u(p.lifetime_mode,0x78);u(p.render_enabled,0x7c);
    u(p.animation_binding,0x80);u(p.animation_frame,0x84);u(p.texture,0x88);u(p.base_color,0x8c);
    u(p.secondary_color,0x90);u(p.field_94,0x94);u(p.render_flags,0x98);
    f(p.base_scale,0x9c);f(p.scale_fade_in,0xa0);f(p.scale_fade_out,0xa4);f(p.scale,0xa8);
    f(p.base_alpha,0xac);f(p.alpha_fade_in,0xb0);f(p.alpha_fade_out,0xb4);f(p.alpha,0xb8);
    f(p.angle,0xbc);f(p.angular_velocity,0xc0);f(p.color_fade_in,0xc4);f(p.color_fade_out,0xc8);u(p.color,0xcc);
}
inline void transfer_bank(std::span<std::uint8_t> b,effects::EffectParticleBank& bank,bool read){
    if(b.size()!=64*slot_size)throw std::runtime_error("particle bank fixture size");
    for(unsigned j=0;j<64;++j)transfer(b.subspan(j*slot_size,slot_size),bank.particles[j],read);
}
inline std::vector<unsigned> excluded(const effects::EffectParticleBank& bank,unsigned base){
    std::vector<unsigned> result;
    for(unsigned j=0;j<64;++j)for(unsigned k=0;k<7;++k)
        if(bank.particles[j].source_undefined[k])result.push_back(base+j*slot_size+undefined_offsets[k]);
    return result;
}
}
