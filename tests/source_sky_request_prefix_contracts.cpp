#include "sgn/source_sky_request_prefix.hpp"
#include "sgn/race_session_platform.hpp"
#include "sgn/race_session.hpp"
#include "sgn/scene_resources.hpp"
#include <array>
#include <bit>
#include <cstdint>
#include <exception>
#include <iostream>
#include <limits>
#include <string>
#include <thread>
#include <type_traits>

namespace {
using namespace sgn;
using namespace sgn::retail;
using SkyWords=std::array<std::uint32_t,10>;
static_assert(sizeof(void*)==8);
static_assert(sizeof(float)==4&&std::numeric_limits<float>::is_iec559);
static_assert(sizeof(std::array<SkyTextureMotion,2>)==40);
static_assert(std::is_trivially_copyable_v<std::array<SkyTextureMotion,2>>);
static_assert(std::bit_cast<std::uint32_t>(0.0f)==0u);
using Signature=SourceRequestBoundary(*)(RaceSessionPlatform&,LevelBlock,SourceRequestObserver*);
static_assert(std::is_same_v<decltype(&produce_source_requests_through_sky_clear_42c7b7),Signature>);

void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
LevelBlock bounded(const std::string& text){return {text,text.size()};}
SkyWords sky_words(const RaceSessionPlatform& p){return std::bit_cast<SkyWords>(p.state().texture_animation.sky);}
constexpr SkyWords patterned{0x10203040u,0x80000000u,0x7fc12345u,0x3f800000u,0xbf800000u,
    0x50607080u,0x00000001u,0xffc54321u,0x3a378034u,0xc0000000u};
void seed(RaceSessionPlatform& p){
    auto& state=p.state();
    state.texture_animation.sky=std::bit_cast<std::array<SkyTextureMotion,2>>(patterned);
    state.texture_animation.oscillation=std::bit_cast<float>(0x42ab1234u);
    state.texture_animation.phase=0x7a6b5c4du;
    state.curves.far_clip[0]=std::bit_cast<float>(0x43124567u); //source577688
    state.curves.far_clip[255]=std::bit_cast<float>(0x44765432u);
}
struct OtherWords {
    std::uint32_t oscillation,phase,curve_first,curve_last;
    bool textures,parts,assets,device,stream,input;
    std::size_t models;
    bool operator==(const OtherWords&) const=default;
};
OtherWords other(RaceSessionPlatform& p){
    const auto& s=p.state();const auto h=p.host_readiness();
    return {std::bit_cast<std::uint32_t>(s.texture_animation.oscillation),s.texture_animation.phase,
        std::bit_cast<std::uint32_t>(s.curves.far_clip.front()),std::bit_cast<std::uint32_t>(s.curves.far_clip.back()),
        bool(s.texture_animations),bool(s.track_part_animations),h.assets,h.audio_device,h.audio_stream,h.window_input,
        p.source_scene_resources().models().size()};
}
struct Fixture {
    RaceSessionPlatform platform;
    explicit Fixture(const std::filesystem::path& root,bool initialize=true):platform(SourceColdConstructionOnly{},root){
        // Explicit caller-owned preparation. The adapter performs none of this.
        platform.prepare_host_assets();platform.prepare_source_scene_resources();
        if(initialize)(void)models().initialize_source_requests_42d070_prefix();
        seed(platform);
        const auto h=platform.host_readiness();
        check(h.assets&&!h.audio_device&&!h.audio_stream&&!h.window_input,"fixture started a native backend");
        check(!platform.state().texture_animations&&!platform.state().track_part_animations,"fixture loaded an animation bank");
    }
    ModelResources& models(){return platform.source_scene_resources().models();}
    const SourcePmdRequests& requests(){return models().source_loading_state().requests;}
};
struct Interrupted {};
struct Observe final:SourceRequestObserver {
    Fixture& fixture;
    const bool interrupt;
    const SkyWords before;
    std::uint32_t events{},last{};
    explicit Observe(Fixture& f,bool cut=false):fixture(f),interrupt(cut),before(sky_words(f.platform)){}
    void after_store(const SourceRequestStore& event,const SourcePmdRequests& actual) override{
        check(&actual==&fixture.requests(),"observer received a substitute request owner");
        check(event.ordinal==++events,"request event order changed");
        check(sky_words(fixture.platform)==before,"sky cleared before producer completed");
        last=event.site;
        if(event.site==0x42c7abu&&interrupt)throw Interrupted{};
    }
};
const std::string sky_script="L_SKY = prefixAnimSkyGroundsuffix \nL_TRACK = MustNotBeReached \n-END";
constexpr std::string_view sky_name="prefixAnimSkyGroundsuffix";
void endpoint(const SourceRequestBoundary& result){
    check(result.next_site==0x42c7b7u&&result.required_callee==0&&!result.reached_return,"clear prefix escaped42C7B7");
    check(result.script_cursor==8u+sky_name.size()+1u,"caller script cursor changed after clear");
}
SourceRequestValues expected_sky(SourceRequestValues before){
    check(before.count==0&&before.append&&before.append.offset()==0,"fixture requests are not initialized");
    for(std::size_t i=0;i<sky_name.size();++i)before.names[i]=sky_name[i];
    before.names[sky_name.size()]=0;
    before.records[0].name=before.append;before.records[0].role=2;
    before.count=1;before.append=before.append.advanced(static_cast<std::uint32_t>(sky_name.size()+1));
    before.records[1].name={};return before;
}
void completed_clear(const std::filesystem::path& root){
    Fixture f(root);const auto unrelated=other(f.platform);const auto original=f.requests().snapshot();Observe trace(f);
    endpoint(produce_source_requests_through_sky_clear_42c7b7(f.platform,bounded(sky_script),&trace));
    check(sky_words(f.platform)==SkyWords{},"425880 did not clear exactly the ten output words");
    check(other(f.platform)==unrelated,"clear changed guards, PMD state or native readiness");
    check(f.requests().snapshot()==expected_sky(original),"clear altered or overran the exact sky request prefix");
    check(trace.last==0x42c7abu&&trace.events==sky_name.size()+6u,"producer store boundary changed");
    check(!f.requests().track_name(),"L_TRACK was parsed beyond425880");
    check(!f.models().source_loading_state().snapshot().file,"PMD file unexpectedly loaded");
}
void absent_sky(const std::filesystem::path& root){
    Fixture f(root);const auto before=f.requests().snapshot();const auto unchanged=other(f.platform);
    const std::string no_keys="NO_KEY = Value \n-END";
    auto result=produce_source_requests_through_sky_clear_42c7b7(f.platform,bounded(no_keys));
    check(result.next_site==0x42c872u&&result.required_callee==0&&result.reached_return,"no-key branch changed");
    check(f.requests().snapshot()==before&&sky_words(f.platform)==patterned,"no-key branch mutated owners");
    const std::string track="L_TRACK = TrackA \n-END";
    result=produce_source_requests_through_sky_clear_42c7b7(f.platform,bounded(track));
    check(result.next_site==0x42c872u&&result.required_callee==0&&result.reached_return,"no-sky track branch changed");
    check(sky_words(f.platform)==patterned&&other(f.platform)==unchanged,"absent sky cleared or prepared another owner");
    const auto value=f.requests().snapshot();
    check(value.count==1&&value.records[0].role==3&&value.records[0].name.text_snapshot()=="TrackA","actual track producer bypassed");
    check(value.track_name==value.records[0].name&&!value.records[1].name,"track name identity or sentinel changed");
}
void observer_failure(const std::filesystem::path& root){
    Fixture f(root);const auto before=f.requests().snapshot();const auto unchanged=other(f.platform);Observe cut(f,true);
    bool caught=false;
    try{(void)produce_source_requests_through_sky_clear_42c7b7(f.platform,bounded(sky_script),&cut);}
    catch(const Interrupted&){caught=true;}
    check(caught&&cut.last==0x42c7abu,"last producer store did not propagate observer exception");
    check(f.requests().snapshot()==expected_sky(before),"observer exception rolled back a completed request store");
    check(sky_words(f.platform)==patterned&&other(f.platform)==unchanged,"observer exception executed425880");
    // A fresh independent actual owner proves the successful completed path;
    // this is not a fabricated resume token or a replay over partial state.
    Fixture success(root);Observe observed(success);
    endpoint(produce_source_requests_through_sky_clear_42c7b7(success.platform,bounded(sky_script),&observed));
    check(observed.last==0x42c7abu&&sky_words(success.platform)==SkyWords{},"successful observer prevented completed clear");
}
void host_admission(const std::filesystem::path& root){
    RaceSessionPlatform missing(SourceColdConstructionOnly{},root);seed(missing);
    bool rejected=false;
    try{(void)produce_source_requests_through_sky_clear_42c7b7(missing,bounded(sky_script));}
    catch(const FormatError&){rejected=true;}
    const auto h=missing.host_readiness();
    check(rejected&&!h.assets&&!h.audio_device&&!h.audio_stream&&!h.window_input&&sky_words(missing)==patterned,
          "missing scene triggered preparation or a source write");
    bool still_missing=false;try{(void)missing.source_scene_resources();}catch(const FormatError&){still_missing=true;}
    check(still_missing,"adapter constructed a substitute scene");
    Fixture f(root);const auto original=f.requests().snapshot();const auto unchanged=other(f.platform);Observe observed(f);
    std::exception_ptr failure;
    std::thread wrong([&]{try{(void)produce_source_requests_through_sky_clear_42c7b7(f.platform,bounded(sky_script),&observed);}
                          catch(...){failure=std::current_exception();}});
    wrong.join(); //No fixture read before join; final owner destruction stays on creating thread.
    bool wrong_thread=false;if(failure){try{std::rethrow_exception(failure);}catch(const std::logic_error&){wrong_thread=true;}}
    check(wrong_thread&&observed.events==0,"wrong-thread call reached producer");
    check(f.requests().snapshot()==original&&sky_words(f.platform)==patterned&&other(f.platform)==unchanged,
          "wrong-thread rejection changed an owner");
    Fixture uninitialized(root,false);const auto empty=uninitialized.requests().snapshot();Observe no_store(uninitialized);
    bool null_append=false;
    try{(void)produce_source_requests_through_sky_clear_42c7b7(uninitialized.platform,bounded(sky_script),&no_store);}
    catch(const SourceRequestError&){null_append=true;}
    check(null_append&&no_store.events==0&&uninitialized.requests().snapshot()==empty&&sky_words(uninitialized.platform)==patterned,
          "adapter initialized requests for its caller");
    // PreviewHostEntry eagerly opens native audio. No private flag mutation or
    // substitute platform is used to fake that negative case in this CPU suite.
    // Its guard is pinned statically; a real preview-entry receiving remains UNRUN.
}
void two_worlds(const std::filesystem::path& root){
    Fixture first(root),second(root);const auto second_requests=second.requests().snapshot();
    const auto second_words=sky_words(second.platform);const auto second_other=other(second.platform);
    check(&first.models()!=&second.models() && &first.requests()!=&second.requests(),"independent owners alias");
    endpoint(produce_source_requests_through_sky_clear_42c7b7(first.platform,bounded(sky_script)));
    check(sky_words(first.platform)==SkyWords{}&&first.requests().snapshot().count==1,"first world did not complete clear");
    check(second.requests().snapshot()==second_requests&&sky_words(second.platform)==second_words&&other(second.platform)==second_other,
          "adapter crossed canonical platform ownership");
}
}
int main(int argc,char** argv)try{
    if(argc!=3)throw std::runtime_error("usage: source_sky_request_prefix_contracts case retail-root");
    const std::string_view selected=argv[1];const std::filesystem::path root=argv[2];
    if(selected=="completed_clear")completed_clear(root);
    else if(selected=="absent_sky")absent_sky(root);
    else if(selected=="observer_failure")observer_failure(root);
    else if(selected=="host_admission")host_admission(root);
    else if(selected=="two_worlds")two_worlds(root);
    else throw std::runtime_error("unknown receiving case");
    std::cout<<"case="<<selected<<" status=PASS scope=sky_clear_prefix full42C740=0 full42D070=0 native=0\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
