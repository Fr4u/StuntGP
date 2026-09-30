#pragma once
#include "../audio_initialize_state.hpp"
#include "sgn/audio_stream_playback.hpp"
#include "input_wire.hpp"
#include <sstream>

namespace pause_menu_test {
namespace os=sgn::test::audio_initialize;
struct AudioResponse {
    unsigned operation,object,result,output;
    std::vector<unsigned> arguments;
};
inline std::vector<AudioResponse> read_audio_responses(const std::filesystem::path& path){
    std::ifstream in(path,std::ios::binary);const auto count=input::number(in);
    input::require(count<=4096,"pause external sound response count");std::vector<AudioResponse> output;
    for(unsigned i=0;i<count;++i){AudioResponse e{input::number(in),input::number(in),input::number(in),input::number(in),{}};
        const auto n=input::number(in);input::require(n<=17,"pause external sound arguments");
        for(unsigned j=0;j<n;++j)e.arguments.push_back(input::number(in));output.push_back(std::move(e));}
    input::require(in.peek()==std::char_traits<char>::eof(),"pause external sound trailing bytes");return output;
}
inline std::vector<os::Event> read_initialization_responses(const std::filesystem::path& path){
    std::ifstream in(path,std::ios::binary);const auto count=input::number(in);
    input::require(count<=8192,"pause external initialization response count");std::vector<os::Event> output(count);
    for(auto& e:output)os::read(in,e);
    input::require(in.peek()==std::char_traits<char>::eof(),"pause external initialization trailing bytes");return output;
}
// Only OS outputs are replayed. Each full native audio child must issue the
// exact source request before it receives that output. No gameplay state,
// callback outcome or post-menu bytes enter this receiver.
class ExternalAudio final:public r::AudioDevice {
    os::Device& initialization_;
    static unsigned id(std::uint64_t v){input::require(v<=0xffffffffu,"pause external sound identity");return static_cast<unsigned>(v);}
    const AudioResponse& call(unsigned op,std::uint64_t object,std::initializer_list<unsigned> args){
        input::require(cursor<responses.size(),"extra pause external sound call");const auto& e=responses[cursor++];
        if(e.operation!=op||e.object!=id(object)||e.arguments!=std::vector<unsigned>(args)){
            std::ostringstream detail;detail<<"pause external sound request mismatch at "<<cursor-1
                <<" native op="<<op<<" object="<<object<<" args=";
            for(auto v:args)detail<<v<<',';detail<<" source op="<<e.operation<<" object="<<e.object<<" args=";
            for(auto v:e.arguments)detail<<v<<',';throw sgn::FormatError(detail.str());}return e;
    }
    static r::AudioResult hr(const AudioResponse& e){return std::bit_cast<r::AudioResult>(e.result);}
public:
    std::vector<AudioResponse> responses;std::size_t cursor{};
    explicit ExternalAudio(os::Device& init):initialization_(init){}
    r::AudioResult status(r::AudioBufferId b,unsigned& flags)override{const auto& e=call(0,b.value,{});flags=e.output;return hr(e);}
    r::AudioResult frequency(r::AudioBufferId b,unsigned v)override{return hr(call(4,b.value,{v}));}
    r::AudioResult volume(r::AudioBufferId b,int v)override{return hr(call(5,b.value,{std::bit_cast<unsigned>(v)}));}
    r::AudioResult pan(r::AudioBufferId b,int v)override{return hr(call(6,b.value,{std::bit_cast<unsigned>(v)}));}
    r::AudioResult play(r::AudioBufferId b,unsigned a,unsigned c,unsigned f)override{return hr(call(7,b.value,{a,c,f}));}
    r::AudioResult stop(r::AudioBufferId b)override{return initialization_.stop(b);}
    unsigned release(r::AudioBufferId b)override{return initialization_.release(b);}
#define PM_EXTERNAL_UNREACHED(type,name,signature) type name signature override{throw sgn::FormatError("unreached pause external OS operation: " #name);}
    PM_EXTERNAL_UNREACHED(r::AudioResult,duplicate,(r::AudioBufferId,r::AudioBufferId&))
    PM_EXTERNAL_UNREACHED(r::AudioResult,query_spatial,(r::AudioBufferId,r::AudioSpatialId&))
    PM_EXTERNAL_UNREACHED(unsigned,release_spatial,(r::AudioSpatialId))
    PM_EXTERNAL_UNREACHED(r::AudioResult,spatial_parameters,(r::AudioSpatialId,const r::AudioSpatialParameters&,unsigned))
    PM_EXTERNAL_UNREACHED(r::AudioResult,spatial_mode,(r::AudioSpatialId,unsigned,unsigned))
    PM_EXTERNAL_UNREACHED(r::AudioResult,spatial_position,(r::AudioSpatialId,sgn::Vec3,unsigned))
    PM_EXTERNAL_UNREACHED(r::AudioResult,spatial_velocity,(r::AudioSpatialId,sgn::Vec3,unsigned))
    PM_EXTERNAL_UNREACHED(r::AudioResult,listener_position,(r::AudioListenerId,sgn::Vec3,unsigned))
    PM_EXTERNAL_UNREACHED(r::AudioResult,listener_velocity,(r::AudioListenerId,sgn::Vec3,unsigned))
    PM_EXTERNAL_UNREACHED(r::AudioResult,listener_orientation,(r::AudioListenerId,sgn::Vec3,sgn::Vec3,unsigned))
#undef PM_EXTERNAL_UNREACHED
};
class ExternalStream final:public r::AudioStreamPlaybackPlatform,public r::AudioStreamCursorDevice {
    os::Device& device_;
public:
    explicit ExternalStream(os::Device& device):device_(device){}
    unsigned kill_timer(unsigned v)override{return device_.kill_timer(v);}
    unsigned end_timer_period(unsigned v)override{return device_.end_timer_period(v);}
    r::AudioResult set_play_cursor(r::AudioBufferId b,unsigned v)override{return device_.set_play_cursor(b,v);}
    int seek_wave(r::AudioFileId f,int offset,int origin)override{return device_.seek_wave(f,offset,origin);}
    unsigned close_wave(r::AudioFileId f,unsigned flags)override{return device_.close_wave(f,flags);}
    // A reached unsupported external call fails qualification; never succeeds
    // as a no-op. These are OS services, not substituted game callees.
#define PM_STREAM_UNREACHED(type,name,signature) type name signature override{throw sgn::FormatError("unreached pause stream OS operation: " #name);}
    PM_STREAM_UNREACHED(unsigned,resource_length,(std::string_view))
    PM_STREAM_UNREACHED(bool,full_path,(std::string_view,std::span<std::uint8_t,260>))
    PM_STREAM_UNREACHED(unsigned,timer_capabilities,(unsigned&,unsigned&))
    PM_STREAM_UNREACHED(unsigned,begin_timer_period,(unsigned))
    PM_STREAM_UNREACHED(unsigned,drive_type,(std::string_view))
    PM_STREAM_UNREACHED(bool,disk_sector_bytes,(std::string_view,unsigned&))
    PM_STREAM_UNREACHED(r::AudioFileId,open_wave,(std::span<const std::uint8_t,260>,unsigned,unsigned))
    PM_STREAM_UNREACHED(unsigned,descend_wave,(r::AudioFileId,r::AudioWaveChunk&,const r::AudioWaveChunk*,unsigned))
    PM_STREAM_UNREACHED(unsigned,ascend_wave,(r::AudioFileId,r::AudioWaveChunk&,unsigned))
    PM_STREAM_UNREACHED(int,read_wave,(r::AudioFileId,std::span<std::uint8_t>))
    PM_STREAM_UNREACHED(unsigned,start_timer,(unsigned,unsigned,std::function<void()>,unsigned))
    PM_STREAM_UNREACHED(void,start_open_worker,(std::function<void()>))
    PM_STREAM_UNREACHED(void,invoke_completion,(unsigned,unsigned))
    PM_STREAM_UNREACHED(unsigned,milliseconds,())
    PM_STREAM_UNREACHED(r::AudioResult,play_cursors,(r::AudioBufferId,unsigned&,unsigned&))
#undef PM_STREAM_UNREACHED
};
}
