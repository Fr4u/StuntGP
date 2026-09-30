#include "sgn/race_session.hpp"
#include "sgn/race_session_music.hpp"
#include <array>
#include <atomic>
#include <cstring>
#include <fstream>
#include <iostream>
#include <thread>

using namespace sgn;
using namespace sgn::retail;
namespace {
unsigned checks{},selected{},guarded{},required{};
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);++checks;}
template<class T>void read(std::istream& in,T& value){
    in.read(reinterpret_cast<char*>(&value),sizeof(value));if(!in)throw std::runtime_error("short retained corpus");
}
void skip(std::istream& in,std::uint32_t count){
    check(count<=1024u*1024u,"unbounded corpus field");
    in.ignore(static_cast<std::streamsize>(count));check(bool(in),"short retained corpus field");
}
bool same(const AudioEnvironmentState& a,const AudioEnvironmentState& b){
    static_assert(sizeof(AudioEnvironmentState)==2372);return std::memcmp(&a,&b,sizeof(a))==0;
}
bool no_native(const RaceSessionPlatform& platform){
    const auto h=platform.host_readiness();return !h.assets&&!h.audio_device&&!h.audio_stream&&!h.window_input;
}
void absent_owner(RaceSessionPlatform& platform){
    bool absent=false;try{(void)platform.source_audio_owner();}
    catch(const FormatError& e){absent=std::string_view(e.what())=="source audio owner has not been constructed";}
    check(absent,"environment selection constructed an audio owner");
}
void receive(const std::filesystem::path& root,const char* input_path,const char* expected_path){
    auto platform=std::make_shared<RaceSessionPlatform>(SourceColdConstructionOnly{},root,0.0f);
    RaceSession session(SourceColdAttachmentOnly{},platform);auto& s=session.state();
    check(&s==&platform->state()&&session.shared_platform()==platform,"source Session uses another world/platform");
    absent_owner(*platform);check(no_native(*platform),"cold construction prepared a native backend");
    std::ifstream input(input_path,std::ios::binary),expected(expected_path,std::ios::binary);
    check(bool(input)&&bool(expected),"cannot open pinned existing instruction corpus");
    std::array<char,8> im{},em{};read(input,im);read(expected,em);
    check(im==em&&std::string_view(im.data(),im.size())=="SGNAENV1","wrong instruction corpus schema");
    std::uint32_t count{},expected_count{};read(input,count);read(expected,expected_count);
    check(count==expected_count&&count==2499,"wrong original corpus extent");
    s.scene_mode=1;s.options.selection_phase=2;s.normal_finish.menu_requested=1;s.pause_flag=3;
    for(std::uint32_t i=0;i<count;++i){
        std::uint32_t op{},initialized{},enabled{},spatial{},type{},length{};
        AudioEnvironmentState before,after_original;std::array<std::uint32_t,5> response{};Vec3 point{};
        read(input,op);read(input,before);read(input,initialized);read(input,enabled);read(input,spatial);
        read(input,type);read(input,response);read(input,point);read(input,length);skip(input,length);
        std::uint32_t original_result{},event_words{};
        read(expected,original_result);read(expected,after_original);read(expected,event_words);skip(expected,event_words*4u);
        if(op!=0)continue;
        ++selected;s.audio_environment=before;s.audio_driver.initialized=initialized;
        s.audio_initialization.spatial_enabled=enabled;s.audio_driver.samples[1].spatial={spatial};
        const auto actual=select_source_audio_environment_460af0(*session.shared_platform(),type);
        if(initialized==1&&enabled==1&&type<=6){
            ++required;
            check(actual.status==SourceAudioEnvironmentStatus::property_interface_required&&actual.blocked_address==0x460a9c&&
                actual.required_spatial==s.audio_driver.samples[1].spatial,"active branch skipped or misidentified source QueryInterface");
            // Stored corpus is the complete original call. Our stop precedes
            // QuerySupport, so retain its INPUT support mask, not the later OR.
            after_original.supported_properties=before.supported_properties;
            check(same(s.audio_environment,after_original),"reached source preset differs from original corpus prefix");
        }else{
            ++guarded;check(original_result==1,"existing original guard did not return1");
            check(actual.status==SourceAudioEnvironmentStatus::completed&&actual.blocked_address==0&&actual.required_spatial.value==0,
                "original disabled/out-of-domain guard required a native property call");
            check(same(s.audio_environment,before),"guarded source call wrote environment state");
        }
        check(s.audio_driver.initialized==initialized&&s.audio_driver.samples[1].spatial.value==spatial&&
            s.audio_initialization.spatial_enabled==enabled,"selection changed source driver inputs");
        check(s.scene_mode==1&&s.options.selection_phase==2&&s.normal_finish.menu_requested==1&&s.pause_flag==3,
            "environment child performed caller scene/pending/audio-pause stores");
    }
    check(input.peek()==std::char_traits<char>::eof()&&expected.peek()==std::char_traits<char>::eof(),"trailing existing corpus data");
    check(selected==140&&required>0&&guarded>0,"missing existing460AF0 source branches");
    absent_owner(*platform);check(no_native(*platform),"corpus receiving prepared native resources");

    // Explicit source-owner construction only. No COM/native import is called.
    auto& owner=platform->prepare_source_audio_owner();const auto* identity=&owner;
    check(!owner.host_backend_ready()&&owner.source_device_identity()==0,"deferred actual owner prepared a device");
    s.audio_driver.initialized=1;s.audio_initialization.spatial_enabled=1;
    s.audio_driver.samples[1].spatial={0x123456789abcdef0ull};
    const auto foreign_before=s.audio_environment;std::atomic<bool> rejected{false};
    std::thread foreign([&]{try{(void)select_source_audio_environment_460af0(*platform,0);}
        catch(const std::logic_error&){rejected=true;}});foreign.join();
    check(rejected&&same(s.audio_environment,foreign_before),"foreign thread reached source preset writes");
    RaceSessionPlatform other(SourceColdConstructionOnly{},root,0.0f);const auto other_before=other.state().audio_environment;
    const auto pending=select_source_audio_environment_460af0(*platform,0);
    check(pending.status==SourceAudioEnvironmentStatus::property_interface_required&&pending.required_spatial.value==0x123456789abcdef0ull,
        "source boundary truncated/replaced the retained pointer-width spatial identity");
    check(&platform->source_audio_owner()==identity&&!owner.host_backend_ready()&&owner.source_device_identity()==0,
        "source property stop replaced/prepared the same deferred owner");
    check(same(other.state().audio_environment,other_before)&&no_native(other),"another platform was changed");
    check(!platform->source_allocations().initialized()&&platform->source_allocations().live_count()==0&&no_native(*platform),
        "environment selection booted allocations or native platform services");
}
}
int main(int argc,char** argv)try{
    check(argc==4,"expected NEW retail root, existing input and existing original-qualified output");
    receive(argv[1],argv[2],argv[3]);
    std::cout<<"PASS "<<selected<<" existing460AF0 cases: "<<guarded<<" completed guards, "<<required
        <<" exact property stops; "<<checks<<" checks; no native backend\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
