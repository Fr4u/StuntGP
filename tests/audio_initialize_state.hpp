#pragma once
#include "sgn/retail_audio_initialize.hpp"
#include "sgn/audio_music_buffer_producer.hpp"
#include <bit>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>

namespace sgn::test::audio_initialize {
using namespace sgn;using namespace sgn::retail;
using Record=std::array<unsigned,0x2000/4>;using Event=std::array<unsigned,64>;
template<class T>void read(std::ifstream& in,T& v){if(!in.read(reinterpret_cast<char*>(&v),sizeof(v)))throw std::runtime_error("short audio initialization corpus");}
inline Bytes bytes(std::ifstream& in,unsigned n){if(n>4*1024*1024)throw std::runtime_error("audio data limit");Bytes b(n);if(!in.read(reinterpret_cast<char*>(b.data()),n))throw std::runtime_error("short audio initialization bytes");return b;}
inline unsigned at(unsigned address){if(address>=0x62df00&&address<0x62f5c0)return (address-0x62df00)/4;if(address>=0x489790&&address<0x4899e0)return (address-0x489790+0x1700)/4;throw std::runtime_error("unmapped initialization word");}
inline void transfer(Record& r,AudioDriverState& d,AudioInitializationState& a,AudioMaintenanceState& m,AudioStreamState& s,LevelAudioState& l,LevelOverrideOptions& o,bool loading){
    auto u=[&](unsigned& x,unsigned address){const auto j=at(address);if(loading)x=r[j];else r[j]=x;};
    auto i=[&](int& x,unsigned address){const auto j=at(address);if(loading)x=std::bit_cast<int>(r[j]);else r[j]=std::bit_cast<unsigned>(x);};
    auto id=[&](auto& x,unsigned address){const auto j=at(address);if(loading)x.value=r[j];else r[j]=static_cast<unsigned>(x.value);};
    id(m.music.buffer,0x62df08);u(s.field_df0c,0x62df0c);u(s.field_df14,0x62df14);u(s.field_df18,0x62df18);u(s.field_df1c,0x62df1c);u(s.field_df20,0x62df20);u(s.field_df24,0x62df24);
    u(s.field_df28,0x62df28);u(s.field_df2c,0x62df2c);u(s.field_df30,0x62df30);u(s.field_df34,0x62df34);u(s.flags,0x62df38);u(s.field_df3c,0x62df3c);u(s.field_df40,0x62df40);
    u(s.timer_period,0x62df4c);u(s.timer,0x62df50);u(s.field_df54,0x62df54);id(s.file,0x62e06c);u(s.data_offset,0x62e07c);u(s.field_e084,0x62e084);u(s.allocated,0x62e088);
    u(s.result,0x62e08c);u(s.field_e090,0x62e090);u(s.field_e098,0x62e098);u(s.field_e09c,0x62e09c);u(s.field_e0a0,0x62e0a0);u(s.playback_active,0x62f5ac);
    u(s.field_e100,0x62e100);u(s.field_e10c,0x62e10c);u(m.music.requested_volume,0x62e0fc);u(m.music.gain,0x62e104);u(m.music.gain_step,0x62e108);
    i(m.music.pan,0x62e110);i(m.music.cached_volume,0x62f5b8);i(m.music.cached_pan,0x62f5bc);
    u(l.scale,0x489790);u(l.input_a,0x62e0a8);u(l.input_b,0x62e0f0);u(l.output_a,0x62e0f8);u(l.output_b,0x489798);u(l.output_b_copy,0x48979c);
    u(d.initialized,0x62e0ac);u(d.spatial_mode,0x4897b4);id(a.device,0x62e0b0);id(a.primary,0x62e114);id(m.listener,0x62f11c);
    u(a.spatial_capability,0x4897ac);u(a.spatial_enabled,0x4897b0);u(a.saved_spatial_mode,0x4899d8);u(a.requested_rate,0x489794);
    u(a.enumeration.cursor_word,0x62f118);u(a.enumeration.count,0x62f120);u(a.enumeration.adjacent_word,0x62f124);
    auto* raw=reinterpret_cast<std::uint8_t*>(r.data());if(loading)std::memcpy(a.enumeration.records.data(),raw+0x1228,1152);else std::memcpy(raw+0x1228,a.enumeration.records.data(),1152);
    for(unsigned j=0;j<128;++j){auto& sample=d.samples[j];auto& extra=a.sample_fields.samples[j];const auto base=0x62e118+j*32;
        id(sample.buffer,base);id(sample.spatial,base+4);u(sample.type,base+8);u(extra.ready,base+12);u(extra.bytes,base+16);u(sample.frequency,base+20);u(extra.bits,base+24);u(extra.channels,base+28);}
    for(unsigned j=0;j<72;++j)if(loading)a.descriptor_types[j]=r[0x1960/4+j];else r[0x1960/4+j]=a.descriptor_types[j];
    if(loading){o.audio_a=r[0x1a80/4];o.audio_b=r[0x1a84/4];}else{r[0x1a80/4]=o.audio_a;r[0x1a84/4]=o.audio_b;}
}
inline std::array<unsigned,7> format_words(const AudioWaveFormat& f){return {f.tag,f.channels,f.samples_per_second,f.bytes_per_second,f.block_align,f.bits_per_sample,f.extra_bytes};}
// Replay only original OS results. Every native request must match the exact
// original call order/arguments before receiving its scripted OS output.
struct Device final:AudioInitializationPlatform,AudioDevice,AudioStreamPlatform,AudioMusicBufferProducer {
    const Record& r;std::span<const Event> expected;std::size_t cursor{};std::map<unsigned,Bytes> buffers;
    explicit Device(const Record& record):r(record){}
    const Event& request(unsigned op,unsigned object,std::span<const unsigned> params){
        if(cursor>=expected.size())throw std::runtime_error("extra initialization OS call op"+std::to_string(op));const auto& e=expected[cursor++];
        if(e[0]!=op||e[1]!=object||e[3]!=params.size()||!std::equal(params.begin(),params.end(),e.begin()+4)){
            std::cerr<<"OS call "<<cursor-1<<" op "<<op<<" expected "<<e[0]<<" object "<<std::hex<<object<<" expected "<<e[1]<<std::dec<<'\n';
            for(unsigned j=0;j<params.size();++j)if(params[j]!=e[4+j])std::cerr<<"param "<<j<<" actual "<<params[j]<<" expected "<<e[4+j]<<'\n';
            throw std::runtime_error("initialization OS request mismatch");}return e;
    }
    const Event& call(unsigned op,unsigned object,std::initializer_list<unsigned> params){return request(op,object,{params.begin(),params.size()});}
    static AudioResult hr(const Event& e){return std::bit_cast<AudioResult>(e[2]);}
    template<class Output> AudioResult create_buffer_value(AudioDeviceId d,const AudioBufferRequest& p,Output& out){
        std::vector<unsigned> args{p.size,p.flags,p.bytes,p.reserved,p.format?1u:0u};if(p.format){const auto f=format_words(*p.format);args.insert(args.end(),f.begin(),f.begin()+6);}
        const auto& e=request(0,static_cast<unsigned>(d.value),args);if(e[21]){out.value=e[20];if(p.bytes>4*1024*1024||buffers.size()>=384)throw std::runtime_error("initialization buffer bound");buffers.emplace(e[20],Bytes(p.bytes,0xcd));}return hr(e);
    }
    AudioResult create_buffer(AudioDeviceId d,const AudioBufferRequest& p,AudioBufferId& out)override{return create_buffer_value(d,p,out);}
    AudioResult create_music_buffer(AudioDeviceId d,const AudioBufferRequest& p,AudioMusicBuffer& out)override{return create_buffer_value(d,p,out);}
    AudioResult query_spatial_buffer(AudioBufferId b,AudioSpatialId& out)override{const auto& e=call(1,static_cast<unsigned>(b.value),{});if(e[21])out.value=e[20];return hr(e);}
    AudioResult lock_buffer(AudioBufferId b,unsigned offset,unsigned count,AudioBufferMemory& memory,unsigned flags)override{
        const auto& e=call(2,static_cast<unsigned>(b.value),{offset,count,flags});if(e[21]){memory.first=buffers.at(static_cast<unsigned>(b.value));memory.second={};}return hr(e);}
    AudioResult unlock_buffer(AudioBufferId b,std::span<std::uint8_t> first,std::span<std::uint8_t> second)override{
        if(first.data()!=buffers.at(static_cast<unsigned>(b.value)).data())throw std::runtime_error("initialization unlock identity");return hr(call(3,static_cast<unsigned>(b.value),{static_cast<unsigned>(first.size()),static_cast<unsigned>(second.size())}));}
    AudioResult capabilities(AudioDeviceId d,std::span<unsigned,24> out)override{const auto& e=call(4,static_cast<unsigned>(d.value),{out[0]});if(e[21])std::copy_n(e.begin()+24,24,out.begin());return hr(e);}
    AudioResult primary_format(AudioPrimaryId p,AudioWaveFormat& f,unsigned count,unsigned& written)override{const auto& e=call(5,static_cast<unsigned>(p.value),{count});if(e[21]){f={static_cast<std::uint16_t>(e[24]),static_cast<std::uint16_t>(e[25]),e[26],e[27],static_cast<std::uint16_t>(e[28]),static_cast<std::uint16_t>(e[29]),static_cast<std::uint16_t>(e[30])};written=e[31];}return hr(e);}
    AudioResult set_primary_format(AudioPrimaryId p,const AudioWaveFormat& f)override{return hr(request(6,static_cast<unsigned>(p.value),format_words(f)));}
    AudioResult enumerate(AudioEnumerationReceiver& receiver)override{
        const auto result=hr(call(7,0,{}));
        while(cursor<expected.size()&&expected[cursor][0]==27){const auto& e=expected[cursor++];if(e[9]>127||e[4]>1||e[2]!=1)throw std::runtime_error("enumeration callback domain");
            std::optional<std::array<unsigned,4>> guid;if(e[4])guid=std::array<unsigned,4>{e[5],e[6],e[7],e[8]};
            const auto* data=reinterpret_cast<const char*>(e.data()+10);const std::string_view description(data,e[9]);
            if(description.find('\0')!=std::string_view::npos||receiver.visit(guid,description)!=bool(e[2]))throw std::runtime_error("enumeration callback result mismatch");}
        return result;
    }
    AudioResult create_default_device(AudioDeviceId& out)override{const auto& e=call(8,0,{});if(e[21])out.value=e[20];return hr(e);}
    AudioResult cooperative_level(AudioDeviceId d,std::uint64_t window,unsigned level)override{return hr(call(9,static_cast<unsigned>(d.value),{static_cast<unsigned>(window),level}));}
    unsigned release_device(AudioDeviceId d)override{return call(10,static_cast<unsigned>(d.value),{})[2];}
    AudioResult query_listener(AudioPrimaryId p,AudioListenerId& out)override{const auto& e=call(11,static_cast<unsigned>(p.value),{});if(e[21])out.value=e[20];return hr(e);}
    AudioResult listener_doppler(AudioListenerId l,float value,unsigned apply)override{return hr(call(13,static_cast<unsigned>(l.value),{std::bit_cast<unsigned>(value),apply}));}
    AudioResult listener_rolloff(AudioListenerId l,float value,unsigned apply)override{return hr(call(12,static_cast<unsigned>(l.value),{std::bit_cast<unsigned>(value),apply}));}
    AudioResult listener_distance(AudioListenerId l,float value,unsigned apply)override{return hr(call(14,static_cast<unsigned>(l.value),{std::bit_cast<unsigned>(value),apply}));}
    unsigned kill_timer(unsigned t)override{return call(15,t,{})[2];}
    AudioResult stop(AudioBufferId b)override{return hr(call(16,static_cast<unsigned>(b.value),{}));}
    AudioResult set_play_cursor(AudioBufferId b,unsigned p)override{return hr(call(17,static_cast<unsigned>(b.value),{p}));}
    int seek_wave(AudioFileId f,int offset,int origin)override{return hr(call(18,static_cast<unsigned>(f.value),{std::bit_cast<unsigned>(offset),std::bit_cast<unsigned>(origin)}));}
    unsigned release(AudioBufferId b)override{return call(19,static_cast<unsigned>(b.value),{})[2];}
    unsigned end_timer_period(unsigned p)override{return call(20,p,{})[2];}
    unsigned close_wave(AudioFileId f,unsigned flags)override{return call(21,static_cast<unsigned>(f.value),{flags})[2];}
#define UNUSED_METHOD(type,name,signature) type name signature override{throw std::runtime_error("unexpected initialization call: " #name);}
    UNUSED_METHOD(AudioResult,status,(AudioBufferId,unsigned&))
    UNUSED_METHOD(AudioResult,duplicate,(AudioBufferId,AudioBufferId&))
    UNUSED_METHOD(AudioResult,query_spatial,(AudioBufferId,AudioSpatialId&))
    UNUSED_METHOD(unsigned,release_spatial,(AudioSpatialId))
    UNUSED_METHOD(AudioResult,frequency,(AudioBufferId,unsigned))
    UNUSED_METHOD(AudioResult,volume,(AudioBufferId,int))
    UNUSED_METHOD(AudioResult,pan,(AudioBufferId,int))
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
