#pragma once
#include "sgn/retail_audio_driver.hpp"
#include <bit>
#include <vector>

namespace sgn::retail::session_world_test {
// Actual OS results, not scripted responses. op/object/return/count/20args/out.
// Every call below forwards to the real device exactly once before recording.
using AudioDeviceEvent=std::array<std::uint32_t,28>;
class RecordingAudioDevice final:public AudioDevice {
    AudioDevice& device_;
    void record(unsigned op,std::uint64_t object,std::uint32_t result,
        std::initializer_list<unsigned> params,std::uint64_t output=0){
        if(events.size()>=4096||params.size()>20)throw FormatError("audio capture event capacity");
        AudioDeviceEvent e{};e[0]=op;e[1]=static_cast<unsigned>(object);e[2]=static_cast<unsigned>(object>>32);e[3]=result;e[4]=static_cast<unsigned>(params.size());
        std::copy(params.begin(),params.end(),e.begin()+5);e[25]=static_cast<unsigned>(output);e[26]=static_cast<unsigned>(output>>32);events.push_back(e);
    }
    static unsigned bits(float v){return std::bit_cast<unsigned>(v);}
    static unsigned bits(int v){return std::bit_cast<unsigned>(v);}
public:
    std::vector<AudioDeviceEvent> events;
    explicit RecordingAudioDevice(AudioDevice& actual):device_(actual){events.reserve(4096);}
    AudioResult status(AudioBufferId id,unsigned& flags)override{const auto r=device_.status(id,flags);record(0,id.value,bits(r),{},flags);return r;}
    AudioResult duplicate(AudioBufferId id,AudioBufferId& out)override{const auto r=device_.duplicate(id,out);record(1,id.value,bits(r),{},out.value);return r;}
    AudioResult query_spatial(AudioBufferId id,AudioSpatialId& out)override{const auto r=device_.query_spatial(id,out);record(2,id.value,bits(r),{},out.value);return r;}
    unsigned release(AudioBufferId id)override{const auto r=device_.release(id);record(3,id.value,r,{});return r;}
    AudioResult frequency(AudioBufferId id,unsigned value)override{const auto r=device_.frequency(id,value);record(4,id.value,bits(r),{value});return r;}
    AudioResult volume(AudioBufferId id,int value)override{const auto r=device_.volume(id,value);record(5,id.value,bits(r),{bits(value)});return r;}
    AudioResult pan(AudioBufferId id,int value)override{const auto r=device_.pan(id,value);record(6,id.value,bits(r),{bits(value)});return r;}
    AudioResult play(AudioBufferId id,unsigned first,unsigned second,unsigned flags)override{const auto r=device_.play(id,first,second,flags);record(7,id.value,bits(r),{first,second,flags});return r;}
    AudioResult stop(AudioBufferId id)override{const auto r=device_.stop(id);record(8,id.value,bits(r),{});return r;}
    AudioResult spatial_parameters(AudioSpatialId id,const AudioSpatialParameters& p,unsigned apply)override{
        const auto r=device_.spatial_parameters(id,p,apply);const auto w=std::bit_cast<std::array<unsigned,16>>(p);
        record(9,id.value,bits(r),{w[0],w[1],w[2],w[3],w[4],w[5],w[6],w[7],w[8],w[9],w[10],w[11],w[12],w[13],w[14],w[15],apply});return r;
    }
    AudioResult spatial_mode(AudioSpatialId id,unsigned mode,unsigned apply)override{const auto r=device_.spatial_mode(id,mode,apply);record(10,id.value,bits(r),{mode,apply});return r;}
    AudioResult spatial_position(AudioSpatialId id,Vec3 p,unsigned apply)override{const auto r=device_.spatial_position(id,p,apply);record(11,id.value,bits(r),{bits(p.x),bits(p.y),bits(p.z),apply});return r;}
    AudioResult spatial_velocity(AudioSpatialId id,Vec3 p,unsigned apply)override{const auto r=device_.spatial_velocity(id,p,apply);record(12,id.value,bits(r),{bits(p.x),bits(p.y),bits(p.z),apply});return r;}
    unsigned release_spatial(AudioSpatialId id)override{const auto r=device_.release_spatial(id);record(13,id.value,r,{});return r;}
    AudioResult listener_position(AudioListenerId id,Vec3 p,unsigned apply)override{const auto r=device_.listener_position(id,p,apply);record(14,id.value,bits(r),{bits(p.x),bits(p.y),bits(p.z),apply});return r;}
    AudioResult listener_velocity(AudioListenerId id,Vec3 p,unsigned apply)override{const auto r=device_.listener_velocity(id,p,apply);record(15,id.value,bits(r),{bits(p.x),bits(p.y),bits(p.z),apply});return r;}
    AudioResult listener_orientation(AudioListenerId id,Vec3 f,Vec3 u,unsigned apply)override{const auto r=device_.listener_orientation(id,f,u,apply);record(16,id.value,bits(r),{bits(f.x),bits(f.y),bits(f.z),bits(u.x),bits(u.y),bits(u.z),apply});return r;}
};
}
