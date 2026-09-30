#pragma once
#include "sgn/retail_camera.hpp"
#include <bit>
#include <cstring>
#include <vector>

// Explicit guest/native field mapping shared by independent camera oracles.
namespace sgn::retail::camera_test {
using Bytes=std::vector<std::uint8_t>;
constexpr std::size_t camera_size=0x300,vehicle_size=0x2900;
inline std::uint32_t word(const Bytes& data,std::size_t at) {
    std::uint32_t result;std::memcpy(&result,data.data()+at,4);return result;
}
inline void put(Bytes& data,std::size_t at,std::uint32_t value) { std::memcpy(data.data()+at,&value,4); }
inline float real(const Bytes& data,std::size_t at) { return std::bit_cast<float>(word(data,at)); }
inline void transfer(Bytes& c,Bytes& v,CameraState& s,CameraVehicle& vehicle,bool reading) {
    auto integer=[&](std::uint32_t& value,Bytes& data,std::size_t at) { if(reading)value=word(data,at);else put(data,at,value); };
    auto scalar=[&](float& value,Bytes& data,std::size_t at) { if(reading)value=real(data,at);else put(data,at,std::bit_cast<std::uint32_t>(value)); };
    auto vector=[&](CameraVector& value,Bytes& data,std::size_t at) {
        scalar(value.x,data,at);scalar(value.y,data,at+4);scalar(value.z,data,at+8);scalar(value.w,data,at+12);
    };
    integer(s.mode,c,4);vector(s.position,c,8);vector(s.target,c,0x28);vector(s.previous_target,c,0x68);
    integer(s.world_to_camera_reference,c,0x4c);integer(s.camera_to_world_reference,c,0x50);
    vector(s.up,c,0x38);vector(s.side,c,0x7c);vector(s.velocity,c,0xc4);vector(s.drift,c,0xd8);
    vector(s.stabilized_up,c,0x1ac);scalar(s.twist,c,0x78);
    scalar(s.lens,c,0xb0);scalar(s.near_clip,c,0xb4);scalar(s.far_factor,c,0xb8);
    scalar(s.half_width,c,0xbc);scalar(s.half_height,c,0xc0);
    integer(s.collision_bypass_ticks,c,0x1a4);integer(s.settle_ticks,c,0x1bc);
    scalar(s.speed_lift,c,0x1c0);scalar(s.height_offset,c,0x1c4);
    scalar(s.linear_speed,c,0xd4);vector(s.angular_velocity,c,0xf4);vector(s.angular_acceleration,c,0x104);
    integer(s.pending_follow_setup,c,0x1d4);
    for(std::size_t i=0;i<3;++i)integer(s.control_angles[i],c,0xe8+i*4);
    integer(s.pending_lens,c,0x1e0);scalar(s.requested_lens,c,0x1e4);
    for(std::size_t i=0;i<16;++i)scalar(s.projection[i],c,0x200+i*4);
    scalar(s.inverse_depth_scale,c,0x240);scalar(s.depth_offset,c,0x244);
    for(std::size_t i=0;i<16;++i) {scalar(s.camera_to_world_rotation[i],c,0x250+i*4);scalar(s.world_to_camera_rotation[i],c,0x290+i*4);}
    integer(vehicle.active,v,0x1ba8);integer(vehicle.adaptive_distance,v,0x2304);
    integer(vehicle.contact_class,v,0x1fd8);integer(vehicle.contact_counter,v,0x2740);scalar(vehicle.contact_height,v,0x2744);
    vector(vehicle.position,v,0x1bac);vector(vehicle.velocity,v,0x1c8c);
    vector(vehicle.right,v,0x1cbc);vector(vehicle.up,v,0x1ccc);vector(vehicle.forward,v,0x1cdc);
    for(std::size_t i=0;i<16;++i)scalar(vehicle.orientation[i],v,0x1bcc+i*4);
    vector(vehicle.orbit_offset,v,0x22cc);vector(vehicle.target_offset,v,0x22dc);vector(vehicle.angles,v,0x22ec);
    vector(vehicle.shift,v,0x2308);vector(vehicle.previous_shift,v,0x2318);
    scalar(vehicle.distance,v,0x22fc);scalar(vehicle.approach_scale,v,0x2300);
    scalar(vehicle.speed,v,0x1cf0);scalar(vehicle.progress,v,0x23b0);scalar(vehicle.height,v,0x244);
    integer(vehicle.camera_mode,v,0x22c8);
}
}
