#include "sgn/source_input_cold_init.hpp"
#include "sgn/race_session.hpp"
#include "sgn/retail_frontend_state.hpp"
#include <algorithm>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace {
using namespace sgn;using namespace sgn::retail;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>auto representation(const T& value){
    const auto* p=reinterpret_cast<const unsigned char*>(&value);return std::vector<unsigned char>(p,p+sizeof(value));
}
template<class T>bool zero(const T& values){return std::all_of(values.begin(),values.end(),[](auto v){return v==0;});}
void seed_physical(RaceSessionState& s){
    for(std::size_t i=0;i<4;++i){
        auto& raw=s.source_input.acquisition.physical[i];auto& extra=s.source_input.physical[i];
        s.menu_input.raw_words[i]={0x12345678,0x87654321};s.menu_input.edges[i]={1,2,3,4,5,6,7,8};
        raw.word_8=11;raw.word_0c=12;raw.word_24=13;raw.word_28=14;raw.words_from_2c.fill(0xabcdef01);
        raw.extra_edges={9,10,11,12,13,14};extra.activity_kind=21;extra.profile_index=22;extra.feedback_mode=2;
        extra.bytes_168_187.fill(31);extra.bytes_188_1a7.fill(32);extra.bytes_1b6_1b7.fill(33);extra.bytes_1d8_1db.fill(34);
        s.frontend_launch.input_bindings[i]={static_cast<std::uint32_t>(i+1),41};
        s.source_input.force_feedback.physical_words[i].fill(0x76543210);
    }
}
void cleared(const RaceSessionState& s){
    for(std::size_t i=0;i<4;++i){
        const auto& raw=s.source_input.acquisition.physical[i];const auto& extra=s.source_input.physical[i];
        require(s.menu_input.raw_words[i].word_0==0&&s.menu_input.raw_words[i].word_4==0,"Session menu raw owner not cleared");
        require(raw.word_8==0&&raw.word_0c==0&&raw.word_24==0&&raw.word_28==0&&zero(raw.words_from_2c),"Session acquisition owner not cleared");
        require(extra.activity_kind==0&&extra.profile_index==0&&extra.feedback_mode==0,"Session physical owner not cleared");
        require(zero(extra.bytes_168_187)&&zero(extra.bytes_188_1a7)&&zero(extra.bytes_1b6_1b7)&&zero(extra.bytes_1d8_1db),"Session opaque tail not cleared");
        require(zero(s.source_input.force_feedback.physical_words[i]),"cold adapter uses another FF owner");
        require(s.frontend_launch.input_bindings[i].vehicle==0xffffu&&s.frontend_launch.input_bindings[i].kind==0,"Session binding reset value differs");
    }
}
}
int main(int argc,char** argv)try{
    require(argc==2,"supply NEW retail asset root");
    RaceSessionLaunch launch;launch.mode=SessionMode::normal_race;launch.master_gain=0;
    auto session=std::make_unique<RaceSession>(argv[1],launch);auto& s=session->state();auto& input=s.source_input;
    unsigned cases=0;
    {
        seed_physical(s);auto& ff=input.force_feedback;
        ff.initialized_576b70=0x87654321;ff.selected_data_4d5b24=0x62d914;
        ff.direction_576b78={17,19};ff.axes_477640={4,8};ff.axes_count_477648=2;ff.gain_47764c=1234;ff.deadband_477650=42;
        ff.parameter_pointer_kind[3][7]=ForceFeedbackParameterPointer::transient_periodic;
        const auto profiles=input.profiles;const auto vehicles=input.vehicle_profiles;
        input.acquisition.source_device_pointer_576b6c=0x576810;input.previous_activity=99;s.menu_input.previous_pause=71;
        s.startup.fields_62d480.fill(0x12345678);s.startup.fields_62dcf0.fill(0x87654321);
        const auto keys=s.startup.fields_62d480;const auto old_keys=s.startup.fields_62dcf0;
        input.acquisition.upstream[9].words.fill(0x97531246);const auto upstream=input.acquisition.upstream[9].words;
        initialize_session_source_physical_input_45ec30(*session);cleared(s);
        require(input.profiles==profiles&&input.vehicle_profiles==vehicles,"Session cold changed profiles");
        require(s.startup.fields_62d480==keys&&s.startup.fields_62dcf0==old_keys,"Session cold changed keyboard history");
        require(input.acquisition.upstream[9].words==upstream&&input.acquisition.source_device_pointer_576b6c==0x576810&&
            input.previous_activity==99&&s.menu_input.previous_pause==71,"Session cold changed external input state");
        require(ff.initialized_576b70==0x87654321&&ff.selected_data_4d5b24==0x62d914&&
            ff.direction_576b78==std::array<std::uint32_t,2>{17,19}&&ff.axes_477640==std::array<std::uint32_t,2>{4,8}&&
            ff.axes_count_477648==2&&ff.gain_47764c==1234&&ff.deadband_477650==42&&
            ff.parameter_pointer_kind[3][7]==ForceFeedbackParameterPointer::transient_periodic,"Session cold erased separate FF state");
        ++cases;std::cout<<"PASS actual-session-cold-borrows-one-physical-FF-owner\n";
    }
    {
        seed_physical(s);const auto before=representation(input);const auto menu=representation(s.menu_input);
        const auto frontend=representation(s.frontend_launch);std::string rejected;
        std::thread worker([&]{try{initialize_session_source_physical_input_45ec30(*session);}
            catch(const std::logic_error& e){rejected=e.what();}catch(...){rejected="wrong exception";}});worker.join();
        require(rejected=="race world and platform belong to the creating game thread","cold adapter omitted owner-thread guard");
        require(representation(input)==before&&representation(s.menu_input)==menu&&representation(s.frontend_launch)==frontend,
            "cold adapter mutated a foreign thread's world");
        ++cases;std::cout<<"PASS foreign-thread-rejected-before-first-source-write\n";
    }
    {
        input.force_feedback.initialized_576b70=1;initialize_session_source_physical_input_45ec30(*session);cleared(s);
        seed_physical(s);initialize_session_source_physical_input_45ec30(*session);cleared(s);
        require(input.force_feedback.initialized_576b70==1,"explicit repeated cold invocation cleared FF global");
        ++cases;std::cout<<"PASS repeated-explicit-cold-does-not-become-a-once-guard\n";
    }
    {
        auto platform=session->shared_platform();auto alias=session->shared_state();
        auto frontend=std::make_unique<FrontendState>(alias,platform);std::weak_ptr<RaceSessionPlatform> weak=platform;
        auto* address=&input;input.force_feedback.physical_words[2][7]=0x12345678;input.physical[3].bytes_1d8_1db[3]=91;
        session.reset();platform.reset();alias.reset();
        require(!weak.expired()&&&frontend->world().source_input==address,"frontend replaced persistent FF input owner");
        const auto& retained=frontend->world().source_input;
        require(retained.force_feedback.physical_words[2][7]==0x12345678&&retained.physical[3].bytes_1d8_1db[3]==91,
            "Session release synthesized cold reset");
        frontend.reset();require(weak.expired(),"FF/cold owners formed lifetime cycle");
        ++cases;std::cout<<"PASS frontend-retains-FF-and-cold-tail-without-reset\n";
    }
    std::cout<<"source_input_cold_session_cases="<<cases<<" physical_effects=0 GPU_runs=0 original_execution_cases=0\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
