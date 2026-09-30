// CPU native-stage link doubles only. Real prefix, tail helper, platform world
// and physical reset; never evidence of real DirectSound admission.
#include "sgn/source_cold_audio_input.hpp"
#include "sgn/source_audio_default_device.hpp"
#include "sgn/source_audio_primary.hpp"
#include "source_cold_audio_physical.hpp"
#include "audio_initialize_state.hpp"
#include <thread>
#include <vector>

namespace fixture {
using namespace sgn;using namespace sgn::retail;using namespace sgn::test::cold_audio;
struct Marker {unsigned stage;};
struct Plan {
    RaceSessionPlatform* platform{};const AssetStore* assets{};InputWindowHandles handles{19,23};
    PhysicalImage physical_before{};unsigned fail{},exception{};AudioResult hr{-7},cooperative_hr{-13};
    std::uint32_t format_site{0x461dea},enabled{7};bool null_primary{};bool catalog_return{true};
    std::vector<unsigned> stages;
} plan;
void stage(unsigned n,RaceSessionPlatform& p){
    require(&p==plan.platform,"stage borrowed a different platform");
    require(n==plan.stages.size()+1,"source stage skipped, reordered or repeated");
    auto& s=p.state();require(physical_image(s)==plan.physical_before,"input reset ran before audio returned");
    require(s.audio_driver.initialized==0,"initialized published before actual tail helper");
    if(n==1){
        require(s.audio_initialization.device.value==0&&s.audio_initialization.primary.value==0&&s.audio_maintenance.listener.value==0,"prefix pointers not cleared");
        require(s.audio_level.output_b==256&&s.audio_initialization.spatial_enabled==1&&s.audio_initialization.spatial_capability==1,"prefix values missing");
        require(s.audio_initialization.enumeration.cursor_word==AudioEnumerationState::source_origin&&s.audio_initialization.enumeration.count==0,"prefix enumeration missing");
        for(auto v:s.audio_initialization.enumeration.records)require(v==0,"prefix records missing");
    }
    plan.stages.push_back(n);
    // Explicit synthetic native-stage prefix, retained across a false/throw.
    s.audio_initialization.sample_fields.samples[n].ready=100+n;
    if(plan.exception==n)throw Marker{n};
}
}
namespace sgn::retail {
using fixture::plan;using fixture::stage;using sgn::test::cold_audio::require;
SourceAudioEnumerationOutcome enumerate_session_source_audio_46210e(RaceSessionPlatform& p){
    stage(1,p);if(plan.fail==1)return SourceAudioEnumerationFailure{plan.hr};return SourceAudioDefaultDeviceBoundary{};
}
SourceAudioDefaultDeviceOutcome create_session_source_audio_default_device_462146(const SourceAudioDefaultDeviceBoundary&,RaceSessionPlatform& p){
    stage(2,p);p.state().audio_initialization.device.value=0x123400;
    if(plan.fail==2)return SourceAudioDefaultDeviceFailure{plan.hr};return SourceAudioCooperativeLevelBoundary{};
}
SourceAudioPrimaryBufferBoundary set_platform_source_audio_cooperative_462165(const SourceAudioCooperativeLevelBoundary&,RaceSessionPlatform& p,InputWindowHandles handles){
    stage(3,p);require(handles.instance==plan.handles.instance&&handles.window==plan.handles.window,"HWND handles changed");
    auto& s=p.state();return {plan.cooperative_hr,s.audio_initialization.device,s.audio_initialization.primary,AudioBufferRequest{20,0x11,0,0,{}}};
}
SourceAudioPrimaryOutcome create_platform_source_audio_primary_461d25(const SourceAudioPrimaryBufferBoundary& reached,RaceSessionPlatform& p){
    stage(4,p);auto& s=p.state();require(&reached.output==&s.audio_initialization.primary&&reached.device==s.audio_initialization.device&&reached.cooperative_result==plan.cooperative_hr,"primary owner or ignored cooperative result changed");
    s.audio_initialization.primary.value=0x223400;
    if(plan.fail==4)return SourceAudioPrimaryFailure{plan.hr};return SourceAudioDeviceCapsBoundary{s.audio_initialization.device,s.audio_initialization.primary};
}
SourceAudioPrimaryFormatResult complete_platform_source_audio_primary_format_461d4c(const SourceAudioDeviceCapsBoundary& reached,RaceSessionPlatform& p){
    stage(5,p);require(&reached.primary==&p.state().audio_initialization.primary,"format changed primary slot");
    return plan.fail==5?SourceAudioPrimaryFormatResult{plan.format_site,plan.hr}:SourceAudioPrimaryFormatResult{0x461dea,0};
}
SourceAudioListenerAcquisitionOutcome acquire_platform_source_audio_listener_462174(const SourceAudioPrimaryFormatResult& reached,RaceSessionPlatform& p){
    stage(6,p);require(reached.source_return(),"false A5 reached listener");
    if(plan.fail==6)return SourceAudioListenerAcquisitionFailure{plan.null_primary?0x461a89u:0x461a97u,plan.null_primary?std::nullopt:std::optional<AudioResult>{plan.hr}};
    p.state().audio_initialization.spatial_enabled=plan.enabled;return SourceAudioSamplesBoundary{};
}
SourceAudioListenerContinuationOutcome continue_platform_source_audio_listener_461aa0(const SourceAudioListenerAcquisitionOutcome& reached,RaceSessionPlatform& p){
    stage(7,p);require(std::holds_alternative<SourceAudioSamplesBoundary>(reached),"prior failure reached A7");
    return SourceAudioSamplesBoundary{};
}
SourceAudioInitializationTailOutcome complete_platform_source_audio_initialization_after_listener(const SourceAudioListenerContinuationOutcome& reached,RaceSessionPlatform& p,const AssetStore& assets){
    stage(8,p);require(&assets==plan.assets&&&assets==&p.assets(),"A10 borrowed another asset store");
    require(std::holds_alternative<SourceAudioSamplesBoundary>(reached),"prior failure reached A10");
    auto& s=p.state();const bool returned=complete_audio_initialization_tail_462187(plan.catalog_return,s.audio_driver,s.audio_stream,s.audio_maintenance.music,s.audio_level,s.options,s.audio_initialization.spatial_enabled);
    return SourceAudioInitializationTailResult{returned,0x461f26,AudioResult{-17}};
}
}
namespace {
using namespace sgn;using namespace sgn::retail;using namespace sgn::test::cold_audio;
using namespace sgn::test::audio_initialize;
Record audio_image(RaceSessionState& s){Record r{};transfer(r,s.audio_driver,s.audio_initialization,s.audio_maintenance,s.audio_stream,s.audio_level,s.options,false);return r;}
void seed(RaceSessionState& s,unsigned input_b){
    Record r{};unsigned v=0x442152;
    for(auto& x:r){v=v*1664525u+1013904223u;x=v;}
    transfer(r,s.audio_driver,s.audio_initialization,s.audio_maintenance,s.audio_stream,s.audio_level,s.options,true);
    s.audio_level.scale=255;s.audio_level.input_a=100;s.audio_level.input_b=input_b;s.audio_level.output_a=17;s.audio_stream.playback_active=0;
    s.audio_initialization.sample_fields.samples={};seed_physical(s);
    s.source_input.previous_activity=191;s.menu_input.previous_pause=193;s.source_input.force_feedback.initialized_576b70=197;
    s.source_scheduler.frequency=199;s.tick_clock.increment=211;s.tick_clock.scheduled=223;s.tick_clock.observed=227;
    s.startup.fields_62d480.fill(229);s.startup.fields_62dcf0.fill(233);
}
void outside_preserved(const RaceSessionState& s){
    require(s.source_input.previous_activity==191&&s.menu_input.previous_pause==193&&s.source_input.force_feedback.initialized_576b70==197,"physical reset escaped record extent");
    require(s.source_scheduler.frequency==199&&s.tick_clock.increment==211&&s.tick_clock.scheduled==223&&s.tick_clock.observed==227,"composition touched clock");
    for(auto x:s.startup.fields_62d480)require(x==229,"composition reset keyboard owner");
    for(auto x:s.startup.fields_62dcf0)require(x==233,"composition reset old keyboard owner");
}
}
int main(int argc,char** argv)try{
    require(argc==2,"expected NEW retail root");unsigned cases=0,normal_false=0,exceptions=0,wrong_threads=0;
    auto run=[&](bool parent,unsigned input_b,fixture::Plan requested,bool prepare_assets=true){
        auto platform=std::make_shared<RaceSessionPlatform>(SourceColdConstructionOnly{},argv[1]);auto& s=platform->state();seed(s,input_b);
        auto alias=std::shared_ptr<RaceSessionState>(platform,&s);std::weak_ptr<RaceSessionPlatform> weak=platform;
        fixture::plan=requested;auto& p=fixture::plan;p.platform=platform.get();p.physical_before=physical_image(s);
        if(prepare_assets){platform->prepare_host_assets();p.assets=&platform->assets();}
        bool interrupted=false,missing_assets=false;SourceColdAudioResult result{};
        try{result=parent?initialize_platform_source_audio_input_442152(*platform,p.handles):initialize_platform_source_audio_462090(*platform,p.handles);}
        catch(const fixture::Marker& e){require(e.stage==p.exception,"exception changed");interrupted=true;++exceptions;}
        catch(const FormatError&){require(!prepare_assets&&p.stages.size()==7,"unexpected host admission failure");interrupted=true;missing_assets=true;++exceptions;}
        const bool expected_false=input_b==0||p.fail!=0||!p.catalog_return;
        if(!interrupted){
            require(result.source_return==!expected_false,"source Boolean changed");
            if(expected_false)++normal_false;
            if(input_b==0)require(p.stages.empty()&&result.result_site==0x4620fd&&!result.native_result,"false prefix ran native/asset stage");
            else if(p.fail){
                const auto site=p.fail==1?0x46210eu:p.fail==2?0x462146u:p.fail==4?0x461d25u:p.fail==5?p.format_site:p.null_primary?0x461a89u:0x461a97u;
                require(result.result_site==site&&result.native_result==(p.null_primary?std::nullopt:std::optional<AudioResult>{p.hr}),"false native facts changed");
                require(p.stages.size()==p.fail,"false source return called later stage");
            }else require(p.stages.size()==8&&result.native_result==AudioResult{-17},"normal catalog facts changed");
            require(physical_image(s)==(parent?reset_image():p.physical_before),"normal false/true physical continuation differs");
            require(s.audio_driver.initialized==(!expected_false?1u:0u),"initialized used as readiness or tail ordering lost");
        }else{
            require(physical_image(s)==p.physical_before&&s.audio_driver.initialized==0,"exception reset input or ran tail");
            require(p.stages.size()==(missing_assets?7:p.exception),"exception called a later stage");
        }
        for(unsigned i=1;i<=8;++i)require(s.audio_initialization.sample_fields.samples[i].ready==(i<=p.stages.size()?100+i:0),"partial stage prefix lost or future prefix invented");
        outside_preserved(s);const auto readiness=platform->host_readiness();require(!readiness.audio_device&&!readiness.audio_stream&&!readiness.window_input,"CPU composition prepared native host state");
        require(alias.get()==&platform->state()&&!alias.owner_before(platform)&&!platform.owner_before(alias),"retained world/control block changed");
        platform.reset();require(!weak.expired()&&alias.get()==&s,"composition destroyed retained platform");alias.reset();require(weak.expired(),"retained platform did not expire");++cases;
    };
    for(bool parent:{false,true}){
        run(parent,0,{},false);
        for(unsigned stage:{1u,2u,4u,6u})for(AudioResult hr:{-7,1}){fixture::Plan p;p.fail=stage;p.hr=hr;run(parent,255,p,false);}
        for(unsigned site:{0x461d4cu,0x461d7eu,0x461deau})for(AudioResult hr:{-9,1}){fixture::Plan p;p.fail=5;p.format_site=site;p.hr=hr;run(parent,255,p,false);}
        fixture::Plan null_primary;null_primary.fail=6;null_primary.null_primary=true;run(parent,255,null_primary,false);
        fixture::Plan false_catalog;false_catalog.catalog_return=false;run(parent,255,false_catalog);
        for(unsigned enabled:{0u,1u,7u,0xffffffffu})for(AudioResult hr:{-13,0,1}){fixture::Plan p;p.enabled=enabled;p.cooperative_hr=hr;run(parent,255,p);}
        for(unsigned stage=1;stage<=8;++stage){fixture::Plan p;p.exception=stage;run(parent,255,p);}
        run(parent,255,{},false); //Existing assets() admission after A7, not a source false.
    }
    for(bool parent:{false,true}){
        RaceSessionPlatform platform(SourceColdConstructionOnly{},argv[1]);auto& s=platform.state();seed(s,0);
        const auto audio=audio_image(s);const auto physical=physical_image(s);fixture::plan={};bool caught=false;
        std::thread foreign([&]{try{if(parent)(void)initialize_platform_source_audio_input_442152(platform,{});else(void)initialize_platform_source_audio_462090(platform,{});}catch(const std::logic_error&){caught=true;}});foreign.join();
        require(caught&&fixture::plan.stages.empty()&&audio_image(s)==audio&&physical_image(s)==physical,"owner-thread guard not first");++wrong_threads;++cases;
    }
    std::cout<<"{\"passed\":true,\"cases\":"<<cases<<",\"normal_false\":"<<normal_false<<",\"exception_prefixes\":"<<exceptions
        <<",\"first_thread_guards\":"<<wrong_threads<<",\"native_stage_link_doubles\":true,\"native_calls\":0,\"QPC_calls\":0,\"same_retained_platform\":true}\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
