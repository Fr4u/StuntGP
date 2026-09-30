#include "sgn/source_outer_control.hpp"
#include "sgn/source_scheduler_state.hpp"
#include "sgn/retail_frontend_clock.hpp"
#include <array>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace sgn::retail;
namespace {
void require(bool v,const char* s){if(!v)throw std::runtime_error(s);}
struct Counter final:FrontendPerformanceCounter {
    std::uint64_t sample{};unsigned reads{};
    std::uint64_t counter() override{++reads;return sample;}
    std::uint64_t frequency() override{throw std::runtime_error("unexpected per-outer frequency read");}
};
// RECEIVING ONLY. This records the production algorithm's child scheduling.
// Input/audio/world/transition ARE NOT qualified by these recorder methods.
// Clock sampling and advancement below are the actual production source ports.
struct Calls {
    RetailTickClock words{100,100,10};Counter counter;
    std::uint32_t fixed_a{},fixed_b{},tick{};
    std::optional<std::uint32_t> ff,missing;
    std::vector<unsigned> trace;
    unsigned fail_at{},iterations{};
    void event(unsigned e){trace.push_back(e);if(e==fail_at)throw std::runtime_error("injected receiving stop");}
    RetailTickClock& clock(){return words;}
    void numeric_429510(){event(1);}
    void camera_4023f0(){event(2);}
    std::optional<std::uint32_t> input_45f210_40dad0(){event(3);return ff;}
    void clock_442490(){event(4);sample_frontend_clock_442490(words,fixed_a,fixed_b,counter);}
    void iteration_442838(){event(5);++iterations;advance_retail_tick(tick,words);}
    void audio_446110_462280(){event(6);}
    std::optional<std::uint32_t> transition_441dc0(){event(7);return missing;}
};
void equal(const Calls& c,std::initializer_list<unsigned> expected){
    require(c.trace==std::vector<unsigned>(expected),"source boundary order differs");
}
}
int main()try{
    unsigned groups=0;
    {
        SourceSchedulerState state;
        require(state.fixed_mode_a==0&&state.fixed_mode_b==0&&state.frequency==0,"PE clock owner init");++groups;
    }
    {
        Calls c;c.counter.sample=100;
        const auto r=detail::source_outer_body_442810(c);
        equal(c,{1,2,3,4,6,7});require(r.entered_iterations==0&&c.counter.reads==1,"zero catchup");++groups;
    }
    {
        Calls c;c.counter.sample=125;
        const auto r=detail::source_outer_body_442810(c);
        equal(c,{1,2,3,4,5,5,5,6,7});require(r.entered_iterations==3&&c.tick==3&&c.words.scheduled==130,"three catchup iterations");++groups;
    }
    for(const auto which:{0u,1u}){
        Calls c;(which==0?c.fixed_a:c.fixed_b)=1;
        const auto r=detail::source_outer_body_442810(c);
        equal(c,{1,2,3,4,5,6,7});require(r.entered_iterations==1&&c.counter.reads==0,"fixed mode uses no OS read");++groups;
    }
    {
        Calls c;c.fixed_a=2;c.fixed_b=2;c.counter.sample=110;
        const auto r=detail::source_outer_body_442810(c);
        require(r.entered_iterations==1&&c.counter.reads==1,"fixed mode exact-one test");++groups;
    }
    for(const auto slot:{0u,1u,2u,3u}){
        Calls c;c.ff=slot;c.counter.sample=1000;
        const auto r=detail::source_outer_body_442810(c);
        equal(c,{1,2,3});require(r.status==SourceOuterStatus::force_feedback_required&&r.blocked_address==0x410a10&&
            r.physical_slot==slot&&r.entered_iterations==0&&c.counter.reads==0&&c.tick==0,"FF must halt all remaining update work");++groups;
    }
    {
        Calls c;c.counter.sample=100;c.missing=0x431810;
        const auto r=detail::source_outer_body_442810(c);
        equal(c,{1,2,3,4,6,7});require(r.status==SourceOuterStatus::scene_child_required&&r.blocked_address==0x431810,"warm source stop boundary");++groups;
    }
    for(const auto stage:{1u,2u,3u,4u,5u,6u,7u}){
        Calls c;c.counter.sample=110;c.fail_at=stage;bool caught=false;
        try{(void)detail::source_outer_body_442810(c);}catch(const std::runtime_error&){caught=true;}
        require(caught&&!c.trace.empty()&&c.trace.back()==stage,"exception executed a later source stage");++groups;
    }
    {
        Calls c;c.counter.sample=401;
        const auto r=detail::source_outer_body_442810(c);
        require(r.entered_iterations==1&&c.words.observed==401&&c.words.scheduled==410,"strict >30 increment discontinuity correction");++groups;
    }
    {
        Calls c;c.counter.sample=400;
        const auto r=detail::source_outer_body_442810(c);
        require(r.entered_iterations==30&&c.words.scheduled==400,"exact30 threshold must not collapse catchup");++groups;
    }
    {
        Calls c;c.counter.sample=100;
        (void)detail::source_outer_body_442810(c);
        c.counter.sample=100;(void)detail::source_outer_body_442810(c);
        require(c.tick==0&&c.trace.size()==12&&c.counter.reads==2,"two source outers may share a simulation tick");++groups;
    }
    std::cout<<"{\"passed\":true,\"groups\":"<<groups<<",\"scope\":\"production ordering algorithm and actual source clock ports only\",\"game_children_qualified\":false,\"native_os_qualified\":false}\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
