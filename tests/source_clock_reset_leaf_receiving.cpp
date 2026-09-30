// Authored CPU receiving of the actual 442100 leaf and retained platform owner.
// Scripted successful results / explicit provider interruptions; no native QPC.
#include "sgn/race_session.hpp"
#include "sgn/source_outer_scheduler.hpp"
#include <array>
#include <cstdint>
#include <exception>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace {
using namespace sgn;
using namespace sgn::retail;
using ClockWords=std::array<std::uint32_t,4>;
unsigned checks=0;
void require(bool condition,const char* reason){++checks;if(!condition)throw std::runtime_error(reason);}
ClockWords words(const RaceSessionState& state){
    return {state.tick_clock.scheduled,state.tick_clock.observed,state.tick_clock.increment,state.source_scheduler.frequency};
}
void seed_clock(RaceSessionState& state,ClockWords value={0x49,0x4f,0x53,0x47}){
    state.tick_clock={value[0],value[1],value[2]};state.source_scheduler.frequency=value[3];
}
struct Read {
    std::uint64_t value;
    ClockWords before;
    bool fail{};
};
struct CounterFailure final:std::runtime_error {
    CounterFailure():std::runtime_error("authored counter interruption"){}
};
struct Counter final:FrontendPerformanceCounter {
    const RaceSessionState& state;
    const std::vector<Read> script;
    unsigned calls=0,frequency_calls=0;
    Counter(const RaceSessionState& owner,std::vector<Read> reads):state(owner),script(std::move(reads)){}
    std::uint64_t counter() override {
        require(calls<script.size(),"extra counter acquisition");
        const auto& read=script[calls];
        require(words(state)==read.before,"clock store was delayed, reordered or applied to another world");
        ++calls;
        if(read.fail)throw CounterFailure();
        return read.value;
    }
    std::uint64_t frequency() override {
        ++frequency_calls;
        throw std::runtime_error("442100 must not request frequency");
    }
};
struct Vector {
    std::array<std::uint64_t,2> counter;
    std::array<std::uint32_t,2> expected;
};
constexpr std::array<Vector,6> vectors{{
    {{0x1234567800000011ull,0x9abcdef000000013ull},{0x11,0x13}},
    {{0x11111111fffffffeull,0x2222222200000002ull},{0xfffffffe,2}},
    {{0x0000000000000000ull,0xffffffff00000000ull},{0,0}},
    {{0x000000017fffffffull,0x000000027fffffffull},{0x7fffffff,0x7fffffff}},
    {{0x1234567880000000ull,0x23456789ffffffffull},{0x80000000,0xffffffff}},
    {{0x0000000200000064ull,0x0000000300000003ull},{100,3}}
}};
void invoke(bool platform_binding,RaceSessionPlatform& platform,Counter& counter){
    if(platform_binding)reset_platform_source_clock_442100(platform,counter);
    else reset_frontend_clock_442100(platform.state().tick_clock,counter);
}
void check_no_backend(RaceSessionPlatform& platform){
    const auto ready=platform.host_readiness();
    require(!ready.assets&&!ready.audio_device&&!ready.audio_stream&&!ready.window_input,
        "clock reset prepared a backend, assets or window input");
}
}

int main(int argc,char** argv)try{
    require(argc==2,"supply NEW retail root");
    auto platform=std::make_shared<RaceSessionPlatform>(SourceColdConstructionOnly{},argv[1],0.0f);
    auto& state=platform->state();auto* identity=&state;
    auto alias=std::shared_ptr<RaceSessionState>(platform,identity);
    std::weak_ptr<RaceSessionPlatform> lifetime=platform;
    state.source_scheduler.fixed_mode_a=191;state.source_scheduler.fixed_mode_b=193;
    state.simulation_tick=197;state.frame_pause=199;state.demo_ticks=211;state.hud_block=223;
    state.rail.tube_glass_texture=7;
    const auto retained=[&]{
        require(&platform->state()==identity&&alias.get()==identity,
            "clock reset changed the world identity");
        require(!alias.owner_before(platform)&&!platform.owner_before(alias),"world alias changed its control block");
        require(state.source_scheduler.fixed_mode_a==191&&state.source_scheduler.fixed_mode_b==193&&
            state.simulation_tick==197&&state.frame_pause==199&&state.demo_ticks==211&&state.hud_block==223,
            "reset changed another source clock or scene word");
        require(state.rail.tube_glass_texture==7u&&!state.rail.spline,"reset executed rail operations");
        check_no_backend(*platform);
    };
    unsigned successes=0,faults=0,repeats=0,wrong_threads=0;
    for(const bool bound:{false,true})for(const auto& vector:vectors){
        seed_clock(state);const auto before=words(state);
        Counter source(state,{{vector.counter[0],before},
            {vector.counter[1],{vector.expected[0],before[1],before[2],before[3]}}});
        invoke(bound,*platform,source);
        require(words(state)==ClockWords{vector.expected[0],vector.expected[1],before[2],before[3]},
            "counter low DWORD or retained increment/frequency mismatch");
        require(source.calls==2&&source.frequency_calls==0,"reset did not perform exactly C,C");
        retained();++successes;
    }
    // No outer-entry increment domain guard belongs to this leaf.
    for(const auto increment:{0u,0xffffffffu}){
        seed_clock(state,{73,79,increment,0xf1234567u});
        Counter source(state,{{17,{73,79,increment,0xf1234567u}},
            {19,{17,79,increment,0xf1234567u}}});
        reset_platform_source_clock_442100(*platform,source);
        require(words(state)==ClockWords{17,19,increment,0xf1234567u},"reset normalized an unrelated clock field");
        require(source.calls==2&&source.frequency_calls==0,"retention case requested frequency");
        retained();++successes;
    }
    for(const bool bound:{false,true})for(unsigned failure=0;failure<2;++failure){
        seed_clock(state);const auto before=words(state);
        Counter source(state,{{0xabcdef0100000011ull,before,failure==0},
            {0xabcdef0200000013ull,{17,before[1],before[2],before[3]},failure==1}});
        bool failed=false;
        try{invoke(bound,*platform,source);}catch(const CounterFailure&){failed=true;}
        const ClockWords expected=failure==0?before:ClockWords{17,before[1],before[2],before[3]};
        require(failed&&source.calls==failure+1&&source.frequency_calls==0,"counter interruption was swallowed or execution continued");
        require(words(state)==expected,"counter interruption lost or rolled back the reached prefix");
        retained();++faults;
        // Explicit standalone re-invocation is permitted after a provider throw.
        Counter resumed(state,{{23,expected},{29,{23,expected[1],expected[2],expected[3]}}});
        invoke(bound,*platform,resumed);
        require(words(state)==ClockWords{23,29,expected[2],expected[3]}&&resumed.calls==2&&resumed.frequency_calls==0,
            "leaf invented a once flag or retained failure state");
        retained();++repeats;
    }
    for(const bool bound:{false,true}){
        seed_clock(state);
        Counter source(state,{{17,{73,79,83,71}},{19,{17,79,83,71}},
            {23,{17,19,83,71}},{29,{23,19,83,71}}});
        invoke(bound,*platform,source);invoke(bound,*platform,source);
        require(source.calls==4&&source.frequency_calls==0&&words(state)==ClockWords{23,29,83,71},
            "two explicit leaf calls were deduplicated or shared one sampled value");
        retained();repeats+=2;
    }
    // A second actual platform is independent; creating it performs no source clock call.
    auto other=std::make_shared<RaceSessionPlatform>(SourceColdConstructionOnly{},argv[1],0.0f);
    seed_clock(other->state(),{101,103,107,109});
    auto* other_identity=&other->state();
    // Verify the adapter before and after the one allowed Session attachment.
    const auto wrong_thread=[&]{
        const auto before=words(state);Counter source(state,{});std::exception_ptr error;
        std::thread worker([&]{try{reset_platform_source_clock_442100(*platform,source);}catch(...){error=std::current_exception();}});
        worker.join();
        bool rejected=false;
        if(error)try{std::rethrow_exception(error);}catch(const std::logic_error& e){
            rejected=std::string_view(e.what())=="race world and platform belong to the creating game thread";
        }
        require(rejected&&source.calls==0&&source.frequency_calls==0&&words(state)==before,
            "platform admission did not precede all clock access");
        retained();++wrong_threads;
    };
    wrong_thread();
    const auto before_attachment=words(state);
    {
        RaceSession session(SourceColdAttachmentOnly{},platform);
        require(&session.state()==identity&&words(session.state())==before_attachment,"Session attachment reset or copied the platform clock");
        Counter source(state,{{31,before_attachment},{37,{31,before_attachment[1],before_attachment[2],before_attachment[3]}}});
        reset_platform_source_clock_442100(*platform,source);
        require(words(session.state())==ClockWords{31,37,before_attachment[2],before_attachment[3]}&&
            source.calls==2&&source.frequency_calls==0,"attached Session did not observe the same source owner");
        retained();++successes;wrong_thread();
    }
    require(words(state)==ClockWords{31,37,before_attachment[2],before_attachment[3]}&&!lifetime.expired(),
        "Session teardown discarded retained clock storage");
    require(&other->state()==other_identity&&words(other->state())==ClockWords{101,103,107,109},"reset changed another platform");
    check_no_backend(*other);retained();
    std::cout<<"CLOCK_RESET_LEAF_PASS vectors=6 successful_calls="<<successes<<" fault_cases="<<faults
        <<" repeated_calls="<<repeats<<" wrong_thread_cases="<<wrong_threads<<" checks="<<checks
        <<" QPF_calls=0 native_QPC_calls=0 original_runs=0 emulator_runs=0 native_backend_runs=0 GPU_runs=0 callers_attached=0\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
