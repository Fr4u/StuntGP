#include "sgn/audio_stream_playback.hpp"
#include "audio_initialize_state.hpp"

namespace {
using namespace sgn;using namespace sgn::retail;using namespace sgn::test::audio_initialize;
using StreamEvent=std::array<unsigned,16>;
constexpr unsigned ring_address=0x10010000,ring_size=4096;
struct Boundary final:AudioDevice,AudioCreationDevice,AudioMusicBufferProducer,AudioStreamPlaybackPlatform,AudioStreamCursorDevice {
    Bytes ring;std::span<const StreamEvent> events;std::size_t cursor{};
    const StreamEvent& call(unsigned op,unsigned object,std::initializer_list<unsigned> args){
        if(cursor>=events.size())throw std::runtime_error("extra music refill boundary");
        const auto& e=events[cursor++];
        if(e[0]!=op||e[1]!=object||e[3]!=args.size()||!std::equal(args.begin(),args.end(),e.begin()+4))
            throw std::runtime_error("music refill OS order/arguments mismatch at event "+std::to_string(cursor-1)+" operation "+std::to_string(op));
        return e;
    }
    static AudioResult hr(const StreamEvent& e){return std::bit_cast<int>(e[2]);}
    unsigned address(std::span<std::uint8_t> value)const{
        if(!value.data())return 0;
        for(unsigned i=0;i<=ring.size();++i)if(value.data()==ring.data()+i)return ring_address+i;
        throw std::runtime_error("foreign music ring pointer");
    }
    AudioResult play_cursors(AudioBufferId b,unsigned& play,unsigned& write)override{
        const auto& e=call(0,static_cast<unsigned>(b.value),{});play=e[10];write=e[11];return hr(e);
    }
    unsigned milliseconds()override{return call(1,0,{})[2];}
    AudioResult lock_buffer(AudioBufferId b,unsigned offset,unsigned count,AudioBufferMemory& memory,unsigned flags)override{
        const auto& e=call(2,static_cast<unsigned>(b.value),{offset,count,flags});
        auto region=[&](unsigned pointer,unsigned size)->std::span<std::uint8_t>{
            if(!pointer){if(size)throw std::runtime_error("null ring size");return {};}
            if(pointer<ring_address||pointer-ring_address>ring.size()||size>ring.size()-(pointer-ring_address))throw std::runtime_error("ring bounds");
            return std::span<std::uint8_t>(ring).subspan(pointer-ring_address,size);
        };
        memory.first=region(e[10],e[11]);memory.second=region(e[12],e[13]);return hr(e);
    }
    int read_wave(AudioFileId f,std::span<std::uint8_t> output)override{
        const auto& e=call(3,static_cast<unsigned>(f.value),{address(output),static_cast<unsigned>(output.size())});
        const auto count=hr(e);if(count>static_cast<int>(output.size()))throw std::runtime_error("read output bound");
        for(int i=0;i<count;++i)output[static_cast<unsigned>(i)]=static_cast<std::uint8_t>(static_cast<unsigned>(i)+e[10]);return count;
    }
    AudioResult unlock_buffer(AudioBufferId b,std::span<std::uint8_t> first,std::span<std::uint8_t> second)override{
        return hr(call(4,static_cast<unsigned>(b.value),{address(first),static_cast<unsigned>(first.size()),address(second),static_cast<unsigned>(second.size())}));
    }
    unsigned kill_timer(unsigned id)override{return call(5,id,{})[2];}
    AudioResult stop(AudioBufferId b)override{return hr(call(6,static_cast<unsigned>(b.value),{}));}
    AudioResult set_play_cursor(AudioBufferId b,unsigned value)override{return hr(call(7,static_cast<unsigned>(b.value),{value}));}
    int seek_wave(AudioFileId f,int offset,int origin)override{return hr(call(8,static_cast<unsigned>(f.value),{std::bit_cast<unsigned>(offset),std::bit_cast<unsigned>(origin)}));}
    void invoke_completion(unsigned callback,unsigned userdata)override{(void)call(10,callback,{userdata});}
#define UNUSED(type,name,args) type name args override{throw std::runtime_error("unexpected music refill call: " #name);}
    UNUSED(AudioResult,status,(AudioBufferId,unsigned&))
    UNUSED(AudioResult,duplicate,(AudioBufferId,AudioBufferId&))
    UNUSED(AudioResult,query_spatial,(AudioBufferId,AudioSpatialId&))
    UNUSED(unsigned,release,(AudioBufferId))
    UNUSED(unsigned,release_spatial,(AudioSpatialId))
    UNUSED(AudioResult,frequency,(AudioBufferId,unsigned))
    UNUSED(AudioResult,volume,(AudioBufferId,int))
    UNUSED(AudioResult,pan,(AudioBufferId,int))
    UNUSED(AudioResult,play,(AudioBufferId,unsigned,unsigned,unsigned))
    UNUSED(AudioResult,spatial_parameters,(AudioSpatialId,const AudioSpatialParameters&,unsigned))
    UNUSED(AudioResult,spatial_mode,(AudioSpatialId,unsigned,unsigned))
    UNUSED(AudioResult,spatial_position,(AudioSpatialId,Vec3,unsigned))
    UNUSED(AudioResult,spatial_velocity,(AudioSpatialId,Vec3,unsigned))
    UNUSED(AudioResult,listener_position,(AudioListenerId,Vec3,unsigned))
    UNUSED(AudioResult,listener_velocity,(AudioListenerId,Vec3,unsigned))
    UNUSED(AudioResult,listener_orientation,(AudioListenerId,Vec3,Vec3,unsigned))
    UNUSED(AudioResult,create_buffer,(AudioDeviceId,const AudioBufferRequest&,AudioBufferId&))
    UNUSED(AudioResult,create_music_buffer,(AudioDeviceId,const AudioBufferRequest&,AudioMusicBuffer&))
    UNUSED(AudioResult,query_spatial_buffer,(AudioBufferId,AudioSpatialId&))
    UNUSED(AudioResult,capabilities,(AudioDeviceId,std::span<unsigned,24>))
    UNUSED(AudioResult,primary_format,(AudioPrimaryId,AudioWaveFormat&,unsigned,unsigned&))
    UNUSED(AudioResult,set_primary_format,(AudioPrimaryId,const AudioWaveFormat&))
    UNUSED(unsigned,resource_length,(std::string_view))
    UNUSED(bool,full_path,(std::string_view,std::span<std::uint8_t,260>))
    UNUSED(unsigned,timer_capabilities,(unsigned&,unsigned&))
    UNUSED(unsigned,begin_timer_period,(unsigned))
    UNUSED(unsigned,drive_type,(std::string_view))
    UNUSED(bool,disk_sector_bytes,(std::string_view,unsigned&))
    UNUSED(AudioFileId,open_wave,(std::span<const std::uint8_t,260>,unsigned,unsigned))
    UNUSED(unsigned,descend_wave,(AudioFileId,AudioWaveChunk&,const AudioWaveChunk*,unsigned))
    UNUSED(unsigned,ascend_wave,(AudioFileId,AudioWaveChunk&,unsigned))
    UNUSED(unsigned,start_timer,(unsigned,unsigned,std::function<void()>,unsigned))
    UNUSED(void,start_open_worker,(std::function<void()>))
    UNUSED(unsigned,end_timer_period,(unsigned))
    UNUSED(unsigned,close_wave,(AudioFileId,unsigned))
#undef UNUSED
};
}
int main(int argc,char** argv){try{
    if(argc!=2)throw std::runtime_error("expected music refill corpus");std::ifstream in(argv[1],std::ios::binary);
    std::array<char,8> magic{};read(in,magic);if(std::string_view(magic.data(),8)!="SGNSTR01")throw std::runtime_error("music refill magic");
    unsigned cases{};read(in,cases);if(!cases||cases>10000)throw std::runtime_error("music refill case limit");
    std::uint64_t event_count=0;
    for(unsigned n=0;n<cases;++n){
        Record actual{},expected{};read(in,actual);read(in,expected);Boundary os;os.ring=bytes(in,ring_size);const auto wanted_ring=bytes(in,ring_size);
        unsigned count{};read(in,count);if(count>32)throw std::runtime_error("music refill event limit");std::vector<StreamEvent> events(count);for(auto& e:events)read(in,e);os.events=events;
        AudioDriverState d;AudioInitializationState a;AudioMaintenanceState m;AudioStreamState s;LevelAudioState l;LevelOverrideOptions o;contact::player::Controls controls;unsigned split{};
        transfer(actual,d,a,m,s,l,o,true);s.chunk_size=actual[at(0x62e074)];s.stop_threshold=actual[at(0x62e094)];
        advance_audio_stream({{d,os,l,controls,split},m.music,s,a.device,os,os,os,os});
        transfer(actual,d,a,m,s,l,o,false);actual[at(0x62e074)]=s.chunk_size;actual[at(0x62e094)]=s.stop_threshold;
        for(unsigned i=0;i<actual.size();++i)if(actual[i]!=expected[i])throw std::runtime_error("music refill state mismatch case "+std::to_string(n)+" word "+std::to_string(i));
        if(os.ring!=wanted_ring||os.cursor!=events.size())throw std::runtime_error("music refill PCM/event mismatch case "+std::to_string(n));event_count+=events.size();
    }
    if(in.peek()!=std::ifstream::traits_type::eof())throw std::runtime_error("trailing music refill corpus");
    std::cout<<"{\"passed\":true,\"cases\":"<<cases<<",\"compared_words\":"<<static_cast<std::uint64_t>(cases)*0x2000/4
        <<",\"compared_pcm_bytes\":"<<static_cast<std::uint64_t>(cases)*ring_size<<",\"os_events\":"<<event_count<<",\"mismatches\":0}\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
