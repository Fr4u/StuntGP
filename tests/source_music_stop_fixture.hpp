#pragma once
// Verbatim receiving codec/device prefix from the existing NEW startup oracle.
#include "sgn/retail_audio_startup.hpp"
#include <bit>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>

namespace {
using namespace sgn;using namespace sgn::retail;
constexpr unsigned size=0x300;using Record=std::array<std::uint32_t,size/4>;using Event=std::array<std::uint32_t,8>;
template<class T>void read(std::ifstream& in,T& value){if(!in.read(reinterpret_cast<char*>(&value),sizeof(value)))throw std::runtime_error("short audio startup corpus");}
unsigned word(const Record& b,unsigned at){return b.at(at/4);}
void transfer(Record& r,AudioStreamState& s,AudioMusicState& m,LevelAudioState& level,AudioPrimaryId& primary,AudioListenerId& listener,bool loading){
    auto u=[&](unsigned& x,unsigned offset){if(loading)x=word(r,offset);else r[offset/4]=x;};
    auto i=[&](std::int32_t& x,unsigned offset){if(loading)x=std::bit_cast<std::int32_t>(word(r,offset));else r[offset/4]=std::bit_cast<unsigned>(x);};
    auto id=[&](auto& x,unsigned offset){if(loading)x.value=word(r,offset);else r[offset/4]=static_cast<unsigned>(x.value);};
    id(m.buffer,8);u(s.field_df0c,0xc);u(s.field_df14,0x14);u(s.field_df18,0x18);u(s.field_df1c,0x1c);u(s.field_df20,0x20);u(s.field_df24,0x24);
    u(s.field_df28,0x28);u(s.field_df2c,0x2c);u(s.field_df30,0x30);u(s.field_df34,0x34);u(s.flags,0x38);u(s.field_df3c,0x3c);u(s.field_df40,0x40);
    u(s.timer_period,0x4c);u(s.timer,0x50);u(s.field_df54,0x54);id(s.file,0x16c);u(s.data_offset,0x17c);u(s.field_e084,0x184);u(s.allocated,0x188);
    u(s.result,0x18c);u(s.field_e090,0x190);u(s.field_e098,0x198);u(s.field_e09c,0x19c);u(s.field_e0a0,0x1a0);
    u(s.playback_active,0x220);u(s.field_e100,0x200);u(s.field_e10c,0x20c);u(m.requested_volume,0x1fc);u(m.gain,0x204);u(m.gain_step,0x208);
    i(m.pan,0x210);i(m.cached_volume,0x22c);i(m.cached_pan,0x230);
    u(level.scale,0x240);u(level.input_a,0x1a8);u(level.input_b,0x1f0);u(level.output_a,0x1f8);u(level.output_b,0x248);u(level.output_b_copy,0x24c);
    id(primary,0x214);id(listener,0x260);
}
struct Device final:AudioDevice,AudioListenerSetupDevice,AudioStreamPlatform {
    const Record& r;std::vector<Event> events;explicit Device(const Record& record):r(record){}
    unsigned call(unsigned op,unsigned object,std::initializer_list<unsigned> args){Event event{};event[0]=op;event[1]=object;event[2]=word(r,0x280+op*4);event[3]=static_cast<unsigned>(args.size());std::copy(args.begin(),args.end(),event.begin()+4);events.push_back(event);return event[2];}
    AudioResult query_listener(AudioPrimaryId p,AudioListenerId& out)override{const auto hr=call(0,static_cast<unsigned>(p.value),{word(r,0x2ac)});if(!hr||word(r,0x2b0))out.value=word(r,0x2ac);return std::bit_cast<AudioResult>(hr);}
    AudioResult listener_doppler(AudioListenerId l,float x,unsigned a)override{return std::bit_cast<AudioResult>(call(2,static_cast<unsigned>(l.value),{std::bit_cast<unsigned>(x),a}));}
    AudioResult listener_rolloff(AudioListenerId l,float x,unsigned a)override{return std::bit_cast<AudioResult>(call(1,static_cast<unsigned>(l.value),{std::bit_cast<unsigned>(x),a}));}
    AudioResult listener_distance(AudioListenerId l,float x,unsigned a)override{return std::bit_cast<AudioResult>(call(3,static_cast<unsigned>(l.value),{std::bit_cast<unsigned>(x),a}));}
    unsigned kill_timer(unsigned t)override{return call(4,t,{});}
    AudioResult stop(AudioBufferId b)override{return std::bit_cast<AudioResult>(call(5,static_cast<unsigned>(b.value),{}));}
    AudioResult set_play_cursor(AudioBufferId b,unsigned x)override{return std::bit_cast<AudioResult>(call(6,static_cast<unsigned>(b.value),{x}));}
    std::int32_t seek_wave(AudioFileId f,std::int32_t offset,std::int32_t origin)override{return std::bit_cast<std::int32_t>(call(7,static_cast<unsigned>(f.value),{std::bit_cast<unsigned>(offset),std::bit_cast<unsigned>(origin)}));}
    unsigned release(AudioBufferId b)override{return call(8,static_cast<unsigned>(b.value),{});}
    unsigned end_timer_period(unsigned p)override{return call(9,p,{});}
    unsigned close_wave(AudioFileId f,unsigned flags)override{return call(10,static_cast<unsigned>(f.value),{flags});}
    // Test-only forbidden calls. A missing production operation cannot pass as a no-op.
#define UNUSED_METHOD(type,name,signature) type name signature override{throw std::runtime_error("unexpected audio startup call: " #name);}
    UNUSED_METHOD(AudioResult,status,(AudioBufferId,unsigned&))
    UNUSED_METHOD(AudioResult,duplicate,(AudioBufferId,AudioBufferId&))
    UNUSED_METHOD(AudioResult,query_spatial,(AudioBufferId,AudioSpatialId&))
    UNUSED_METHOD(unsigned,release_spatial,(AudioSpatialId))
    UNUSED_METHOD(AudioResult,frequency,(AudioBufferId,unsigned))
    UNUSED_METHOD(AudioResult,volume,(AudioBufferId,std::int32_t))
    UNUSED_METHOD(AudioResult,pan,(AudioBufferId,std::int32_t))
    UNUSED_METHOD(AudioResult,play,(AudioBufferId,unsigned,unsigned,unsigned))
    UNUSED_METHOD(AudioResult,spatial_parameters,(AudioSpatialId,const AudioSpatialParameters&,unsigned))
    UNUSED_METHOD(AudioResult,spatial_mode,(AudioSpatialId,unsigned,unsigned))
    UNUSED_METHOD(AudioResult,spatial_position,(AudioSpatialId,Vec3,unsigned))
    UNUSED_METHOD(AudioResult,spatial_velocity,(AudioSpatialId,Vec3,unsigned))
    UNUSED_METHOD(AudioResult,listener_position,(AudioListenerId,Vec3,unsigned))
    UNUSED_METHOD(AudioResult,listener_velocity,(AudioListenerId,Vec3,unsigned))
    UNUSED_METHOD(AudioResult,listener_orientation,(AudioListenerId,Vec3,Vec3,unsigned))
#undef UNUSED_METHOD
};
}
