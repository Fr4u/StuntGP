#pragma once
#include "sgn/retail_audio_driver.hpp"
#include <bit>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace sgn::retail;
using sgn::Vec3;
namespace {
using Record=std::array<std::uint32_t,640>;
using Event=std::array<std::uint32_t,20>;
template<class T>void read(std::ifstream& f,T& x){f.read(reinterpret_cast<char*>(&x),sizeof x);if(!f)throw std::runtime_error("truncated audio corpus");}
float real(std::uint32_t x){return std::bit_cast<float>(x);}
std::uint32_t bits(float x){return std::bit_cast<std::uint32_t>(x);}
std::uint32_t id(AudioBufferId x){return static_cast<std::uint32_t>(x.value);}
std::uint32_t id(AudioSpatialId x){return static_cast<std::uint32_t>(x.value);}
std::uint32_t id(AudioListenerId x){return static_cast<std::uint32_t>(x.value);}
std::array<std::uint32_t,16> pack(const AudioSpatialParameters& p) {
    return {p.size,bits(p.position.x),bits(p.position.y),bits(p.position.z),bits(p.velocity.x),bits(p.velocity.y),bits(p.velocity.z),
            p.inside_cone_angle,p.outside_cone_angle,bits(p.cone_orientation.x),bits(p.cone_orientation.y),bits(p.cone_orientation.z),
            std::bit_cast<std::uint32_t>(p.outside_cone_volume),bits(p.minimum_distance),bits(p.maximum_distance),p.mode};
}
AudioSpatialParameters unpack(const Record& r) {
    return {r[16],{real(r[17]),real(r[18]),real(r[19])},{real(r[20]),real(r[21]),real(r[22])},r[23],r[24],
        {real(r[25]),real(r[26]),real(r[27])},std::bit_cast<std::int32_t>(r[28]),real(r[29]),real(r[30]),r[31]};
}
struct Device final:AudioDevice {
    const Record& r;std::vector<Event> trace;
    explicit Device(const Record& input):r(input){}
    AudioResult call(unsigned op,std::uint32_t object,std::initializer_list<std::uint32_t> args) {
        if(args.size()>16)throw std::runtime_error("audio event overflow");
        Event e{};e[0]=op;e[1]=object;e[2]=r[op<13?584+op:602+op-13];e[3]=static_cast<std::uint32_t>(args.size());
        std::copy(args.begin(),args.end(),e.begin()+4);trace.push_back(e);return std::bit_cast<std::int32_t>(e[2]);
    }
    AudioResult status(AudioBufferId b,std::uint32_t& x)override {auto h=call(0,id(b),{r[597]});if(!h||r[600])x=r[597];return h;}
    AudioResult duplicate(AudioBufferId b,AudioBufferId& x)override{auto h=call(1,id(b),{r[598]});if(!h||r[600])x={r[598]};return h;}
    AudioResult query_spatial(AudioBufferId b,AudioSpatialId& x)override{auto h=call(2,id(b),{r[599]});if(!h||r[600])x={r[599]};return h;}
    std::uint32_t release(AudioBufferId b)override{return std::bit_cast<std::uint32_t>(call(3,id(b),{}));}
    std::uint32_t release_spatial(AudioSpatialId b)override{return std::bit_cast<std::uint32_t>(call(13,id(b),{}));}
    AudioResult frequency(AudioBufferId b,std::uint32_t x)override{return call(4,id(b),{x});}
    AudioResult volume(AudioBufferId b,std::int32_t x)override{return call(5,id(b),{std::bit_cast<std::uint32_t>(x)});}
    AudioResult pan(AudioBufferId b,std::int32_t x)override{return call(6,id(b),{std::bit_cast<std::uint32_t>(x)});}
    AudioResult play(AudioBufferId b,std::uint32_t a,std::uint32_t c,std::uint32_t f)override{return call(7,id(b),{a,c,f});}
    AudioResult stop(AudioBufferId b)override{return call(8,id(b),{});}
    AudioResult spatial_parameters(AudioSpatialId b,const AudioSpatialParameters& p,std::uint32_t apply)override {
        if(apply!=0)throw std::runtime_error("unexpected spatial deferred apply");
        Event e{};e[0]=9;e[1]=id(b);e[2]=r[593];e[3]=16;const auto a=pack(p);std::copy(a.begin(),a.end(),e.begin()+4);
        trace.push_back(e);return std::bit_cast<std::int32_t>(e[2]);
    }
    AudioResult spatial_mode(AudioSpatialId b,std::uint32_t m,std::uint32_t a)override{return call(10,id(b),{m,a});}
    AudioResult spatial_position(AudioSpatialId b,Vec3 v,std::uint32_t a)override{return call(11,id(b),{bits(v.x),bits(v.y),bits(v.z),a});}
    AudioResult spatial_velocity(AudioSpatialId b,Vec3 v,std::uint32_t a)override{return call(12,id(b),{bits(v.x),bits(v.y),bits(v.z),a});}
    AudioResult listener_position(AudioListenerId b,Vec3 v,std::uint32_t a)override{return call(14,id(b),{bits(v.x),bits(v.y),bits(v.z),a});}
    AudioResult listener_velocity(AudioListenerId b,Vec3 v,std::uint32_t a)override{return call(15,id(b),{bits(v.x),bits(v.y),bits(v.z),a});}
    AudioResult listener_orientation(AudioListenerId b,Vec3 f,Vec3 u,std::uint32_t a)override{return call(16,id(b),{bits(f.x),bits(f.y),bits(f.z),bits(u.x),bits(u.y),bits(u.z),a});}
};
void transfer(Record& r,AudioDriverState& s,LevelAudioState& level,contact::player::Controls& controls,bool loading) {
    auto u=[&](std::uint32_t& v,std::size_t i){if(loading)v=r[i];else r[i]=v;};
    u(s.initialized,0);u(s.spatial_mode,1);u(s.range,2);u(s.range_squared,3);
    if(loading)s.inverse_range=real(r[4]);else r[4]=bits(s.inverse_range);
    u(s.duplicate_count,5);u(s.playing_count,6);u(level.output_b,7);
    if(loading)s.spatial_template=unpack(r);else {const auto p=pack(s.spatial_template);std::copy(p.begin(),p.end(),r.begin()+16);}
    for(unsigned i=0;i<128;++i){auto& p=s.samples[i];const auto at=32+i*4;
        if(loading){p.buffer={r[at]};p.spatial={r[at+1]};}else{r[at]=id(p.buffer);r[at+1]=id(p.spatial);}
        u(p.type,at+2);u(p.frequency,at+3);
    }
    if(loading)for(unsigned i=0;i<4;++i){auto& c=controls.controllers[i];const auto at=544+i*10;
        c.active=std::bit_cast<std::int32_t>(r[at]);c.camera.position={real(r[at+1]),real(r[at+2]),real(r[at+3]),0};
        c.camera.side={real(r[at+4]),real(r[at+5]),real(r[at+6]),0};c.camera.velocity={real(r[at+7]),real(r[at+8]),real(r[at+9]),0};
    }
}
}
