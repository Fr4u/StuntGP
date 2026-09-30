#include "sgn/source_audio_initialization_tail.hpp"
#include "sgn/race_session.hpp"
#include "audio_initialize_state.hpp"
#include <functional>
#include <thread>

namespace fixture {
using namespace sgn;using namespace sgn::retail;
struct Marker {};
struct Plan {
    RaceSessionPlatform* expected_platform{};const AssetStore* expected_assets{};
    unsigned calls{};std::function<SourceAudioCatalogLoadOutcome(RaceSessionPlatform&)> call;
} plan;
void require(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
}
namespace sgn::retail {
//CPU link boundary only: tests the thin composition's borrows and ordering.
//This symbol replaces the A10 platform call in this executable; it is not a
//production implementation, a native receiving run or A10 semantic evidence.
SourceAudioCatalogLoadOutcome load_platform_source_audio_catalog_after_listener(
    const SourceAudioListenerContinuationOutcome& reached,RaceSessionPlatform& platform,const AssetStore& assets){
    using namespace fixture;++plan.calls;
    require(&platform==plan.expected_platform&&&assets==plan.expected_assets,"adapter changed platform/asset owner");
    require(std::holds_alternative<SourceAudioSamplesBoundary>(reached),"prior failure reached A10 boundary");
    require(platform.state().audio_driver.initialized==0,"initialized was published before A10");
    return plan.call(platform);
}
}
namespace {
using namespace sgn;using namespace sgn::retail;using fixture::require;using namespace sgn::test::audio_initialize;
Record snapshot(RaceSessionState& w){
    Record r{};transfer(r,w.audio_driver,w.audio_initialization,w.audio_maintenance,w.audio_stream,w.audio_level,w.options,false);return r;
}
void seed(RaceSessionState& w){
    Record r{};unsigned value=0x621a2;
    for(auto& x:r){value=value*1664525u+1013904223u;x=value;}
    r[at(0x62e0ac)]=0;r[at(0x62f5ac)]=0;r[at(0x62e0f8)]=17;r[at(0x489790)]=255;
    r[at(0x62e0a8)]=100;r[at(0x62e0f0)]=255;r[at(0x4897b0)]=2;
    transfer(r,w.audio_driver,w.audio_initialization,w.audio_maintenance,w.audio_stream,w.audio_level,w.options,true);
}
}
int main(int argc,char** argv){try{
    require(argc==2,"expected retail asset root");AssetStore assets(argv[1]);
    RaceSessionPlatform platform(SourceColdConstructionOnly{},argv[1]);auto& w=platform.state();
    fixture::plan.expected_platform=&platform;fixture::plan.expected_assets=&assets;
    const SourceAudioListenerContinuationOutcome reached=SourceAudioSamplesBoundary{};unsigned groups=0;
    for(const auto failure:{SourceAudioListenerAcquisitionFailure{0x461a89,std::nullopt},
        SourceAudioListenerAcquisitionFailure{0x461a97,AudioResult{-7}},SourceAudioListenerAcquisitionFailure{0x461a97,AudioResult{1}}}){
        seed(w);const auto before=snapshot(w);fixture::plan.calls=0;
        const auto outcome=complete_platform_source_audio_initialization_after_listener(failure,platform,assets);
        const auto& result=std::get<SourceAudioListenerAcquisitionFailure>(outcome);
        require(result.source_site==failure.source_site&&result.query_result==failure.query_result,"prior source failure changed");
        require(fixture::plan.calls==0&&snapshot(w)==before,"prior failure touched owner state or A10");++groups;
    }
    for(bool prior_failure:{false,true}){
        seed(w);const auto before=snapshot(w);fixture::plan.calls=0;bool rejected=false;
        std::thread foreign([&]{try{
            const SourceAudioListenerContinuationOutcome input=prior_failure?SourceAudioListenerContinuationOutcome{SourceAudioListenerAcquisitionFailure{0x461a89,std::nullopt}}:reached;
            (void)complete_platform_source_audio_initialization_after_listener(input,platform,assets);
        }catch(const std::logic_error&){rejected=true;}});foreign.join();
        require(rejected&&fixture::plan.calls==0&&snapshot(w)==before,"first owner-thread guard failed before side effects");++groups;
    }
    seed(w);fixture::plan.calls=0;
    fixture::plan.call=[](RaceSessionPlatform& p)->SourceAudioCatalogLoadOutcome{
        auto& state=p.state();state.audio_initialization.sample_fields.samples[3].ready=1;
        state.audio_driver.samples[3].frequency=12345;return SourceAudioCatalogLoadResult{false,0x461eec,AudioResult{-9}};
    };
    auto expected=snapshot(w);expected[at(0x62e118+3*32+12)]=1;expected[at(0x62e118+3*32+20)]=12345;
    auto result=std::get<SourceAudioInitializationTailResult>(complete_platform_source_audio_initialization_after_listener(reached,platform,assets));
    require(!result.source_return&&result.last_native_call==0x461eec&&result.last_native_result==AudioResult{-9},"false catalog facts changed");
    require(fixture::plan.calls==1&&snapshot(w)==expected,"false catalog lost its prefix or ran tail");++groups;
    seed(w);fixture::plan.calls=0;expected=snapshot(w);
    fixture::plan.call=[](RaceSessionPlatform& p)->SourceAudioCatalogLoadOutcome{
        auto& state=p.state();state.audio_initialization.sample_fields.samples[9].ready=1;
        state.audio_driver.samples[9].frequency=19191;state.audio_initialization.spatial_enabled=7;throw fixture::Marker{};
    };
    expected[at(0x62e118+9*32+12)]=1;expected[at(0x62e118+9*32+20)]=19191;expected[at(0x4897b0)]=7;
    bool marker=false;try{(void)complete_platform_source_audio_initialization_after_listener(reached,platform,assets);}catch(const fixture::Marker&){marker=true;}
    require(marker&&fixture::plan.calls==1&&snapshot(w)==expected,"exception lost prefix, translated failure or published tail");++groups;
    for(const unsigned enabled:{0u,1u,7u,0xffffffffu}){
        seed(w);fixture::plan.calls=0;
        fixture::plan.call=[enabled](RaceSessionPlatform& p)->SourceAudioCatalogLoadOutcome{
            auto& state=p.state();state.audio_initialization.spatial_enabled=enabled;
            //A10 may return true while individual entries failed. Preserve facts.
            state.audio_initialization.sample_fields.samples[15].ready=1;state.audio_driver.samples[15].buffer={};
            return SourceAudioCatalogLoadResult{true,0x461f26,AudioResult{-11}};
        };
        result=std::get<SourceAudioInitializationTailResult>(complete_platform_source_audio_initialization_after_listener(reached,platform,assets));
        require(result.source_return&&result.last_native_result==AudioResult{-11}&&fixture::plan.calls==1,"normal A10 was rejected or repeated");
        require(w.audio_driver.initialized==1&&w.audio_driver.spatial_mode==enabled,"tail cached enabled before A10 or missed initialized");
        require(w.audio_initialization.sample_fields.samples[15].ready==1&&w.audio_driver.samples[15].buffer.value==0,"tail rewrote partial catalog");
        require(w.audio_stream.field_df18%256==0&&w.audio_stream.field_df54%256==0&&w.audio_maintenance.music.buffer.value==0,"music child not called on same owners");
        require(w.options.audio_a==100&&w.options.audio_b==255&&w.audio_level.output_a==650&&w.audio_level.output_b==1658&&w.audio_level.output_b_copy==1658,"460BC0 omitted, reordered or used another level");
        const auto readiness=platform.host_readiness();require(!readiness.audio_device&&!readiness.audio_stream,"tail prepared a host backend");++groups;
    }
    //The unchanged full initialize_audio must reject a split level owner before
    //its prefix writes or any replay-device request.
    seed(w);const auto before=snapshot(w);Record replay{};Device device(replay);contact::player::Controls controls;unsigned split=0;
    AudioDriver driver{w.audio_driver,device,w.audio_level,controls,split};LevelAudioState other=w.audio_level;
    AudioStartupWorld world{driver,w.audio_maintenance,w.audio_stream,w.audio_initialization,other,w.options,assets,device,device,0};
    bool mismatch=false;try{(void)initialize_audio(world);}catch(const std::invalid_argument&){mismatch=true;}
    require(mismatch&&snapshot(w)==before&&device.cursor==0,"shared extraction moved the original same-level guard");++groups;
    std::cout<<"{\"passed\":true,\"groups\":"<<groups<<",\"A10_boundary_is_CPU_double\":true,\"native_calls\":0,\"same_platform_borrows\":true}\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
