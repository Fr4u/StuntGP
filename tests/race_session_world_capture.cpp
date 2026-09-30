#include "sgn/race_session_world_tick.hpp"
#include "sgn/race_overlay_textures.hpp"
#include "session_world_state.hpp"
#include "session_audio_device_capture.hpp"
#include <exception>
#include <fstream>
#include <iostream>

namespace {
using namespace sgn;using namespace sgn::retail;using namespace sgn::retail::session_world_test;
template<class T>void write(std::ofstream& f,const T& value){
    static_assert(std::is_trivially_copyable_v<T>);f.write(reinterpret_cast<const char*>(&value),sizeof(value));if(!f)throw FormatError("world capture write");
}
void write_bytes(std::ofstream& f,std::span<const std::uint8_t> b){f.write(reinterpret_cast<const char*>(b.data()),static_cast<std::streamsize>(b.size()));if(!f)throw FormatError("world capture payload write");}
void write_string(std::ofstream& f,std::string_view s){write(f,narrow(s.size()));f.write(s.data(),static_cast<std::streamsize>(s.size()));if(!f)throw FormatError("world capture string write");}
class Images final:public TextureReplacementDevice {
public:
    Image actual;unsigned uploads{};
    explicit Images(const AssetStore& assets):actual(assets.texture("Overlays\\Start0")){}
    std::uint16_t destination_width(unsigned slot)const override{if(slot!=15||actual.width!=256)throw FormatError("world capture texture destination");return 256;}
    void upload_to_destination(unsigned slot,const Image& image)override{if(slot!=15)throw FormatError("world capture upload slot");actual=image;++uploads;}
};
struct AudioRequests final:AudioRequestObserver {
    RaceSessionState& state;std::vector<std::array<unsigned,8>> events;bool invalid{};
    explicit AudioRequests(RaceSessionState& s):state(s){events.reserve(1024);}
    void observe(unsigned sample,unsigned flags,unsigned frequency,unsigned volume,int pan,unsigned force,const unsigned* handle)noexcept override{
        unsigned address=0;
        if(handle){
            for(unsigned i=0;i<6;++i){auto& a=state.vehicles.audio[i];const unsigned base=whole_tick_test::car_base+i*whole_tick_test::car_stride;
                for(const auto& [p,offset]:std::array<std::pair<const unsigned*,unsigned>,9>{{{&a.horn_handle,0x2338},{&a.impact_handle,0x2370},{&a.grip_handle,0x237c},
                    {&a.scrape_handle,0x2380},{&a.rolling_handle,0x2384},{&a.boost_handle,0x2388},{&a.idle_handle,0x238c},{&a.rev_handle,0x2390},{&a.motion_handle,0x239c}}})
                    if(p==handle)address=base+offset;
            }
            if(handle==&state.audio_frontier.steering_handle)address=0x570ad0;
            if(!address)invalid=true;
        }
        if(events.size()==events.capacity()){invalid=true;return;}
        events.push_back({0x461150,sample,flags,frequency,volume,std::bit_cast<unsigned>(pan),force,address});
    }
};
struct Stage {unsigned entry{},device_events{},audio_requests{};Snapshot snapshot;};
struct Audit {
    Capture& capture;RecordingAudioDevice& audio;AudioRequests& requests;
    std::vector<Stage> stages;std::exception_ptr error;
    Audit(Capture& c,RecordingAudioDevice& a,AudioRequests& r):capture(c),audio(a),requests(r){stages.reserve(7);}
    void after(unsigned entry)noexcept{
        if(error)return;
        try{if(stages.size()==7)throw FormatError("too many world stages");if(entry==0x415df0)capture.impact_list_published=true;
            stages.push_back({entry,narrow(audio.events.size()),narrow(requests.events.size()),capture.snapshot()});}
        catch(...){error=std::current_exception();}
    }
};
}
int main(int argc,char** argv)try{
    if(argc!=3)throw FormatError("expected retail root and new capture output");
    if(std::filesystem::exists(argv[2]))throw FormatError("preserve existing native world capture");
    std::ofstream f(argv[2],std::ios::binary);f.write("SGNSWCP1",8);write(f,3u);
    for(const auto level:{"TRACK1","TRACK7","TRACK24"}){
        RaceSessionLaunch launch;launch.level_name=level;launch.master_gain=0;
        RaceSession session(argv[1],launch);Capture capture(session,argv[1]);
        Images images(session.assets());RaceOverlayTextures textures(session.assets(),images,session.shared_platform());
        RecordingAudioDevice audio(session.audio_device());AudioRequests requests(session.state());
        const auto initial=capture.snapshot();const auto again=capture.snapshot();
        if(initial.bytes!=again.bytes||initial.defined!=again.defined)throw FormatError("capture changed native initial state");
        Audit audit(capture,audio,requests);SessionWorldTickAudit hook{};hook.context=&audit;hook.audio=&requests;
        hook.after_stage=[](void* p,unsigned entry)noexcept{static_cast<Audit*>(p)->after(entry);};
        tick_session_world(session,{textures,audio,{}},&hook);
        if(audit.error)std::rethrow_exception(audit.error);
        if(audit.stages.size()!=7||requests.invalid)throw FormatError("incomplete actual world capture");
        write_string(f,level);write(f,narrow(capture.regions.size()));
        for(const auto& r:capture.regions){write(f,std::array<unsigned,6>{r.kind,r.index,r.address,r.length,r.descriptor,r.offset});write_string(f,r.name);}
        write(f,capture.size);write_bytes(f,initial.bytes);write_bytes(f,initial.defined);write(f,narrow(audit.stages.size()));
        for(const auto& stage:audit.stages){write(f,std::array<unsigned,3>{stage.entry,stage.device_events,stage.audio_requests});write_bytes(f,stage.snapshot.bytes);write_bytes(f,stage.snapshot.defined);}
        write(f,narrow(audio.events.size()));for(const auto& e:audio.events)write(f,e);
        write(f,narrow(requests.events.size()));for(const auto& e:requests.events)write(f,e);
        write(f,images.uploads);write(f,textures.selected_overlay_texture());
        std::cout<<level<<" models="<<session.models().size()<<" stages="<<audit.stages.size()<<" bytes="<<capture.size
            <<" OS="<<audio.events.size()<<" requests="<<requests.events.size()<<'\n';
    }
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
