// New host ownership tests. Source behavior is exercised by the separate
// dispatcher receiver; these guards must run before source writes/OS calls.
#include "sgn/source_input_dispatch.hpp"
#include "sgn/race_session.hpp"
#include "sgn/retail_frontend_state.hpp"
#include <algorithm>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace {
using namespace sgn::retail;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
unsigned cases{};
template<class F> void check(const char* name,F action){action();++cases;std::cout<<"PASS "<<name<<'\n';}
struct Keys final:MenuKeyboardAcquisition {
    unsigned calls{};
    std::array<std::uint8_t,256> next{};
    void acquire_45ec00(std::span<std::uint8_t,256> target) override {
        ++calls;std::copy(next.begin(),next.end(),target.begin());
    }
};
// Object representations are compared only against the same unchanged live
// objects across a rejected call; padding is never used as a source value.
template<class T> auto representation(const T& value){
    const auto* first=reinterpret_cast<const unsigned char*>(&value);
    return std::vector<unsigned char>(first,first+sizeof(value));
}
auto affected_source(const RaceSessionState& state){
    auto result=representation(state.source_input);
    const auto append=[&](const auto& value){
        const auto bytes=representation(value);result.insert(result.end(),bytes.begin(),bytes.end());
    };
    append(state.startup.fields_62d480);append(state.startup.fields_62dcf0);
    append(state.menu_input);append(state.frontend_launch.input_bindings);
    append(state.ui.second_pause);append(state.frame_pause);append(state.current_slot);
    append(state.normal_finish.confirm_pressed);append(state.normal_finish.confirm_consumed);
    append(state.pause.raw_activity);append(state.simulation_tick);
    return result;
}
}

int main(int argc,char** argv)try{
    require(argc==2,"supply the NEW project's retail data root");
    RaceSessionLaunch launch;launch.mode=SessionMode::normal_race;launch.master_gain=0;
    auto session=std::make_unique<RaceSession>(argv[1],launch);
    auto& state=session->state();auto& input=state.source_input;
    Keys keys;MenuDirectInputRegistry registry;
    check("foreign-storage-rejected-before-acquisition-and-source-writes",[&]{
        SourceInputStorage foreign;
        input.previous_activity=71;foreign.previous_activity=93;
        state.ui.second_pause=0xabcdef01u;state.startup.fields_62dcf0[0]=0x11223344u;
        const auto before=affected_source(state),other_before=representation(foreign);
        bool rejected=false;
        try{(void)acquire_dispatch_session_input(*session,foreign,keys,registry);}
        catch(const sgn::FormatError& e){
            rejected=std::string(e.what())=="Session source input belongs to a different world";
        }
        require(rejected&&keys.calls==0,"foreign storage was accepted or acquired input");
        require(affected_source(state)==before&&representation(foreign)==other_before,
            "foreign storage rejection mutated source owners");
    });
    check("both-overloads-reject-foreign-thread-before-source-writes",[&]{
        const auto before=affected_source(state);
        std::array<std::string,2> errors;
        std::thread worker([&]{
            for(unsigned i=0;i<2;++i){
                try{
                    if(i==0)(void)acquire_dispatch_session_input(*session,input,keys,registry);
                    else (void)acquire_dispatch_session_input(*session,keys,registry);
                }catch(const std::logic_error& e){errors[i]=e.what();}
                catch(...){errors[i]="unexpected exception";}
            }
        });
        worker.join();
        for(const auto& error:errors)
            require(error=="race world and platform belong to the creating game thread","wrong thread guard");
        require(keys.calls==0&&affected_source(state)==before,"wrong-thread call changed source or polled");
    });
    check("session-owned-overload-borrows-live-profile-and-keyboard-history",[&]{
        // Explicit fixture seed, not application startup or a source45EC30 call.
        initialize_keyboard_upstream_projection_4249b0(input.acquisition);
        state.scene_mode=0;state.frame_pause=0;state.current_slot=5;
        state.menu_input.raw_words={};state.menu_input.previous_pause=0;
        state.startup.fields_62d480.fill(0);state.startup.fields_62dcf0.fill(0);
        for(auto& binding:state.frontend_launch.input_bindings)binding={0xffffu,0};
        state.frontend_launch.input_bindings[0]={0,99};
        state.vehicles.startup[0].field_2280=1;
        state.vehicles.physics[0].input={};state.vehicles.physics[0].input.throttle=17;
        state.normal_finish.confirm_pressed=0;state.normal_finish.confirm_consumed=0;
        state.pause.raw_activity=0;input.physical[0].feedback_mode=0;
        auto* profile=reinterpret_cast<unsigned char*>(input.profiles[0].data());
        profile[0]=0x51; // Deliberately differs from the PE default; no cached profile.
        menu_keyboard_bytes(state.startup)[0x51]=0x83;keys.next[0x51]=0x80;
        const auto tick=state.simulation_tick;
        require(acquire_dispatch_session_input(*session,keys,registry).status==SourceInputStatus::completed,
            "Session-owned input failed");
        require(keys.calls==1&&state.vehicles.physics[0].input.throttle==255,
            "Session profile or keyboard storage was not used");
        require(state.vehicles.restart[0].previous_input.throttle==17&&
            reinterpret_cast<const unsigned char*>(state.startup.fields_62dcf0.data())[0x51]==0x83,
            "current and previous input owners diverged");
        require(input.vehicle_profiles[0][0]==0x51&&state.simulation_tick==tick,
            "profile was stale or acquisition advanced simulation");
    });
    check("frontend-retains-same-input-owner-after-session-release",[&]{
        auto platform=session->shared_platform();
        auto alias=session->shared_state();
        auto frontend=std::make_unique<FrontendState>(alias,platform);
        auto* address=&input;input.previous_activity=123;
        input.acquisition.physical[2].word_8=456;input.vehicle_profiles[4][3]=79;
        std::weak_ptr<RaceSessionPlatform> weak=platform;
        session.reset();alias.reset();platform.reset();
        require(!weak.expired() && &frontend->world().source_input==address,"input owner died or moved with Session");
        auto& retained=frontend->world().source_input;
        require(retained.previous_activity==123&&retained.acquisition.physical[2].word_8==456&&
            retained.vehicle_profiles[4][3]==79,"Session release reset retained input");
        retained.previous_activity=321;
        require(frontend->shared_platform()->state().source_input.previous_activity==321,"frontend has a shadow input bank");
        frontend.reset();require(weak.expired(),"input lifetime has an ownership cycle");
        // No Session or input reference is accessed after final release.
    });
    std::cout<<"source_input_ownership_cases="<<cases<<" GPU_runs=0 original_execution_cases=0\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
