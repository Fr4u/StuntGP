#include "sgn/audio_stream_playback.hpp"
#include "audio_initialize_state.hpp"
#include <algorithm>
#include <string>

namespace {
using namespace sgn;using namespace sgn::retail;using namespace sgn::test::audio_initialize;
using MusicEvent=std::array<unsigned,44>;
struct Request {MusicEvent e;Bytes payload;};
std::string source_path(unsigned track){return "D:\\Game\\Sound\\Music\\"+(track==1?std::string("Empty"):"Track"+std::to_string(track))+".wav";}
unsigned track_number(std::string_view path){
    const auto slash=path.find_last_of("/\\");const auto name=path.substr(slash==path.npos?0:slash+1);
    if(name=="Empty.wav")return 1;
    if(!name.starts_with("Track")||!name.ends_with(".wav"))throw std::runtime_error("unexpected music path");
    const auto n=std::stoul(std::string(name.substr(5,name.size()-9)));
    if(n<2||n>23)throw std::runtime_error("music track bound");return static_cast<unsigned>(n);
}
std::array<unsigned,5> chunk_words(const AudioWaveChunk& c){return {c.id,c.size,c.type,c.data_offset,c.flags};}
AudioWaveChunk chunk(std::span<const unsigned,5> w){return {w[0],w[1],w[2],w[3],w[4]};}

struct Boundary final:AudioDevice,AudioCreationDevice,AudioMusicBufferProducer,AudioStreamPlaybackPlatform,AudioStreamCursorDevice {
    std::map<unsigned,Bytes> buffers;std::span<const Request> requests;std::size_t cursor{};
    const Request& request(unsigned op,unsigned object,std::span<const unsigned> args){
        if(cursor>=requests.size())throw std::runtime_error("extra enabled music request op"+std::to_string(op));
        const auto& r=requests[cursor++];const auto& e=r.e;
        if(e[0]!=op||e[1]!=object||e[3]!=args.size()||!std::equal(args.begin(),args.end(),e.begin()+4)){
            std::cerr<<"request "<<cursor-1<<" operation "<<op<<" expected "<<e[0]<<" object "<<object<<" expected "<<e[1]<<'\n';
            for(unsigned i=0;i<args.size();++i)if(args[i]!=e[4+i])std::cerr<<"argument "<<i<<" actual "<<args[i]<<" expected "<<e[4+i]<<'\n';
            throw std::runtime_error("enabled music OS request order/arguments mismatch");
        }return r;
    }
    const Request& call(unsigned op,unsigned object,std::initializer_list<unsigned> args={}){return request(op,object,{args.begin(),args.size()});}
    static AudioResult hr(const Request& r){return std::bit_cast<int>(r.e[2]);}
    unsigned resource_length(std::string_view name)override{
        const auto n=track_number(name);
        if(name!="sound/Music/"+std::string(n==1?"Empty":"Track"+std::to_string(n))+".wav")throw std::runtime_error("native resource path mapping");
        return call(0,n).e[2];
    }
    bool full_path(std::string_view name,std::span<std::uint8_t,260> output)override{
        const auto n=track_number(name);
        const auto relative="sound/Music/"+(n==1?std::string("Empty"):"Track"+std::to_string(n))+".wav";
        if(name!=relative)throw std::runtime_error("native fullpath argument mapping");
        const auto& r=call(1,n,{260});
        if(r.payload.size()>output.size()||r.payload.empty()||r.payload.back()!=0)throw std::runtime_error("full path result bounds");
        if(std::string(reinterpret_cast<const char*>(r.payload.data()))!=source_path(n))throw std::runtime_error("source path result");
        std::copy(r.payload.begin(),r.payload.end(),output.begin());return r.e[2]!=0;
    }
    unsigned timer_capabilities(unsigned& minimum,unsigned& maximum)override{const auto& r=call(2,0);minimum=r.e[28];maximum=r.e[29];return r.e[2];}
    unsigned begin_timer_period(unsigned period)override{return call(3,period).e[2];}
    unsigned drive_type(std::string_view root)override{if(root!="D:\\")throw std::runtime_error("drive root");return call(4,0).e[2];}
    bool disk_sector_bytes(std::string_view root,unsigned& bytes)override{if(root!="D:\\")throw std::runtime_error("sector root");const auto& r=call(5,0);bytes=r.e[28];return r.e[2]!=0;}
    AudioFileId open_wave(std::span<const std::uint8_t,260> raw,unsigned buffer_bytes,unsigned flags)override{
        const auto end=std::find(raw.begin(),raw.end(),std::uint8_t{});if(end==raw.end())throw std::runtime_error("unterminated wave path");
        const std::string path(reinterpret_cast<const char*>(raw.data()),static_cast<std::size_t>(end-raw.begin()));
        const auto n=track_number(path);if(path!=source_path(n))throw std::runtime_error("wave canonical path");
        return {call(6,n,{buffer_bytes,flags}).e[2]};
    }
    unsigned descend_wave(AudioFileId f,AudioWaveChunk& c,const AudioWaveChunk* parent,unsigned flags)override{
        std::vector<unsigned> args{flags,parent?1u:0u,(flags&0x10)?c.id:0u};
        if(parent){const auto w=chunk_words(*parent);args.insert(args.end(),w.begin(),w.end());}
        const auto& r=request(7,static_cast<unsigned>(f.value),args);c=chunk(std::span<const unsigned,5>(r.e.data()+28,5));return r.e[2];
    }
    int read_wave(AudioFileId f,std::span<std::uint8_t> output)override{
        const auto& r=call(8,static_cast<unsigned>(f.value),{static_cast<unsigned>(output.size())});const auto result=hr(r);
        if((result>=0&&r.payload.size()!=static_cast<unsigned>(result))||r.payload.size()>output.size())throw std::runtime_error("mmioRead payload size");
        std::copy(r.payload.begin(),r.payload.end(),output.begin());return result;
    }
    unsigned ascend_wave(AudioFileId f,AudioWaveChunk& c,unsigned flags)override{
        const auto w=chunk_words(c);std::vector<unsigned> args{flags};args.insert(args.end(),w.begin(),w.end());
        const auto& r=request(9,static_cast<unsigned>(f.value),args);c=chunk(std::span<const unsigned,5>(r.e.data()+28,5));return r.e[2];
    }
    int seek_wave(AudioFileId f,int offset,int origin)override{return hr(call(10,static_cast<unsigned>(f.value),{std::bit_cast<unsigned>(offset),std::bit_cast<unsigned>(origin)}));}
    template<class Output> AudioResult create_buffer_value(AudioDeviceId device,const AudioBufferRequest& p,Output& output){
        if(!p.format)throw std::runtime_error("missing buffer format");const auto w=format_words(*p.format);
        std::vector<unsigned> args{p.size,p.flags,p.bytes,p.reserved};args.insert(args.end(),w.begin(),w.end());
        const auto& r=request(11,static_cast<unsigned>(device.value),args);
        if(r.e[29]){output={r.e[28]};if(p.bytes>512000||buffers.contains(r.e[28]))throw std::runtime_error("create buffer bound/identity");buffers.emplace(r.e[28],Bytes(p.bytes,0xcd));}
        return hr(r);
    }
    AudioResult create_buffer(AudioDeviceId d,const AudioBufferRequest& p,AudioBufferId& out)override{return create_buffer_value(d,p,out);}
    AudioResult create_music_buffer(AudioDeviceId d,const AudioBufferRequest& p,AudioMusicBuffer& out)override{return create_buffer_value(d,p,out);}
    AudioResult lock_buffer(AudioBufferId b,unsigned offset,unsigned count,AudioBufferMemory& memory,unsigned flags)override{
        const auto& r=call(12,static_cast<unsigned>(b.value),{offset,count,flags});auto& bytes=buffers.at(static_cast<unsigned>(b.value));
        if(offset!=0||count!=bytes.size()||r.e[28]!=count)throw std::runtime_error("full buffer lock bound");memory.first=bytes;memory.second={};return hr(r);
    }
    AudioResult unlock_buffer(AudioBufferId b,std::span<std::uint8_t> first,std::span<std::uint8_t> second)override{
        const auto& bytes=buffers.at(static_cast<unsigned>(b.value));if(first.data()!=bytes.data()||first.size()!=bytes.size()||!second.empty())throw std::runtime_error("unlock buffer identity");
        return hr(call(13,static_cast<unsigned>(b.value),{static_cast<unsigned>(first.size()),static_cast<unsigned>(second.size())}));
    }
    AudioResult status(AudioBufferId b,unsigned& flags)override{const auto& r=call(14,static_cast<unsigned>(b.value));flags=r.e[28];return hr(r);}
    AudioResult volume(AudioBufferId b,int value)override{return hr(call(15,static_cast<unsigned>(b.value),{std::bit_cast<unsigned>(value)}));}
    AudioResult play(AudioBufferId b,unsigned a,unsigned c,unsigned flags)override{return hr(call(16,static_cast<unsigned>(b.value),{a,c,flags}));}
    unsigned start_timer(unsigned delay,unsigned resolution,std::function<void()> callback,unsigned flags)override{
        if(!callback)throw std::runtime_error("missing source timer callback binding");return call(17,0,{delay,resolution,flags}).e[2];
    }
    unsigned kill_timer(unsigned id)override{return call(18,id).e[2];}
    AudioResult stop(AudioBufferId b)override{return hr(call(19,static_cast<unsigned>(b.value)));}
    AudioResult set_play_cursor(AudioBufferId b,unsigned position)override{return hr(call(20,static_cast<unsigned>(b.value),{position}));}
    unsigned release(AudioBufferId b)override{return call(21,static_cast<unsigned>(b.value)).e[2];}
    unsigned end_timer_period(unsigned period)override{return call(22,period).e[2];}
    unsigned close_wave(AudioFileId f,unsigned flags)override{return call(23,static_cast<unsigned>(f.value),{flags}).e[2];}
    AudioResult pan(AudioBufferId b,int value)override{return hr(call(25,static_cast<unsigned>(b.value),{std::bit_cast<unsigned>(value)}));}
#define UNUSED(type,name,args) type name args override{throw std::runtime_error("unexpected enabled music call: " #name);}
    UNUSED(AudioResult,duplicate,(AudioBufferId,AudioBufferId&))
    UNUSED(AudioResult,query_spatial,(AudioBufferId,AudioSpatialId&))
    UNUSED(unsigned,release_spatial,(AudioSpatialId))
    UNUSED(AudioResult,frequency,(AudioBufferId,unsigned))
    UNUSED(AudioResult,spatial_parameters,(AudioSpatialId,const AudioSpatialParameters&,unsigned))
    UNUSED(AudioResult,spatial_mode,(AudioSpatialId,unsigned,unsigned))
    UNUSED(AudioResult,spatial_position,(AudioSpatialId,Vec3,unsigned))
    UNUSED(AudioResult,spatial_velocity,(AudioSpatialId,Vec3,unsigned))
    UNUSED(AudioResult,listener_position,(AudioListenerId,Vec3,unsigned))
    UNUSED(AudioResult,listener_velocity,(AudioListenerId,Vec3,unsigned))
    UNUSED(AudioResult,listener_orientation,(AudioListenerId,Vec3,Vec3,unsigned))
    UNUSED(AudioResult,query_spatial_buffer,(AudioBufferId,AudioSpatialId&))
    UNUSED(AudioResult,capabilities,(AudioDeviceId,std::span<unsigned,24>))
    UNUSED(AudioResult,primary_format,(AudioPrimaryId,AudioWaveFormat&,unsigned,unsigned&))
    UNUSED(AudioResult,set_primary_format,(AudioPrimaryId,const AudioWaveFormat&))
    UNUSED(AudioResult,play_cursors,(AudioBufferId,unsigned&,unsigned&))
    UNUSED(void,start_open_worker,(std::function<void()>))
    UNUSED(void,invoke_completion,(unsigned,unsigned))
    UNUSED(unsigned,milliseconds,())
#undef UNUSED
};

struct World {
    Record record;AudioDriverState driver;AudioInitializationState initialization;AudioMaintenanceState maintenance;
    AudioStreamState stream;LevelAudioState level;LevelOverrideOptions options;contact::player::Controls controls;
    AudioMusicTrack music;unsigned split{};Boundary boundary;
    void codec(bool loading){
        transfer(record,driver,initialization,maintenance,stream,level,options,loading);
        auto u=[&](unsigned& x,unsigned a){if(loading)x=record[at(a)];else record[at(a)]=x;};
        u(music.selected,0x62f5a8);u(stream.timer_capabilities.minimum(),0x62df44);u(stream.timer_capabilities.maximum(),0x62df48);
        u(stream.chunk_id,0x62e070);u(stream.chunk_size,0x62e074);u(stream.chunk_type,0x62e078);u(stream.chunk_flags,0x62e080);
        u(stream.stop_threshold,0x62e094);u(stream.status,0x62e0a4);
        auto* raw=reinterpret_cast<std::uint8_t*>(record.data());
        if(loading)std::memcpy(stream.filename_tail.data(),raw+0x58,256);else std::memcpy(raw+0x58,stream.filename_tail.data(),256);
        auto bytes=[&](auto& value,unsigned offset){if(loading)std::memcpy(&value,raw+offset,sizeof(value));else std::memcpy(raw+offset,&value,sizeof(value));};
        auto& f=stream.format;bytes(f.tag,0x158);bytes(f.channels,0x15a);bytes(f.samples_per_second,0x15c);bytes(f.bytes_per_second,0x160);
        bytes(f.block_align,0x164);bytes(f.bits_per_sample,0x166);bytes(f.extra_bytes,0x168);
    }
    void run(unsigned requested,unsigned loop){start_audio_music({{driver,boundary,level,controls,split},maintenance.music,stream,initialization.device,boundary,boundary,boundary,boundary},music,requested,loop);}
};
void compare(const Record& actual,const Record& expected,unsigned group,unsigned step){
    for(unsigned i=0;i<actual.size();++i)if(actual[i]!=expected[i]){
        std::cerr<<"group "<<group<<" step "<<step<<" word "<<i<<" actual "<<std::hex<<actual[i]<<" expected "<<expected[i]<<std::dec<<'\n';
        throw std::runtime_error("enabled music retained state mismatch");
    }
}
}
int main(int argc,char** argv){try{
    if(argc!=2)throw std::runtime_error("expected enabled music corpus");std::ifstream in(argv[1],std::ios::binary);
    std::array<char,8> magic{};read(in,magic);if(std::string_view(magic.data(),8)!="SGNMUS01")throw std::runtime_error("enabled music magic");
    unsigned groups{};read(in,groups);if(!groups||groups>4096)throw std::runtime_error("enabled music group bound");
    std::uint64_t steps=0,continuations=0,event_count=0,pcm_bytes=0;
    for(unsigned g=0;g<groups;++g){
        World w;read(in,w.record);w.codec(true);unsigned count{};read(in,count);if(!count||count>32)throw std::runtime_error("enabled music continuation bound");
        for(unsigned step=0;step<count;++step){
            Record before{},expected{};read(in,before);read(in,expected);w.codec(false);compare(w.record,before,g,step);
            unsigned requested{},loop{},events{};read(in,requested);read(in,loop);read(in,events);if(events>128)throw std::runtime_error("music event bound");
            std::vector<Request> requests(events);for(auto& r:requests){read(in,r.e);unsigned size{};read(in,size);r.payload=bytes(in,size);}
            w.boundary.requests=requests;w.boundary.cursor=0;w.run(requested,loop);w.codec(false);compare(w.record,expected,g,step);
            if(w.boundary.cursor!=requests.size())throw std::runtime_error("unconsumed music requests");
            unsigned buffers{};read(in,buffers);if(buffers!=w.boundary.buffers.size())throw std::runtime_error("retained buffer count");
            for(unsigned b=0;b<buffers;++b){unsigned id{},size{};read(in,id);read(in,size);const auto wanted=bytes(in,size);
                if(w.boundary.buffers.at(id)!=wanted)throw std::runtime_error("music PCM content mismatch");pcm_bytes+=size;}
            ++steps;continuations+=step!=0;event_count+=events;
        }
    }
    if(in.peek()!=std::ifstream::traits_type::eof())throw std::runtime_error("trailing enabled music corpus");
    std::cout<<"{\"passed\":true,\"groups\":"<<groups<<",\"cases\":"<<steps<<",\"continuations\":"<<continuations
             <<",\"compared_state_bytes\":"<<steps*2*0x2000<<",\"compared_pcm_bytes\":"<<pcm_bytes<<",\"os_events\":"<<event_count<<",\"mismatches\":0}\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
