// Authored receiving for the NEW platform binding. Main compiles/runs it.
// Counter values are an explicit OS-boundary fixture, not display timing.
#include "sgn/race_session.hpp"
#include "sgn/source_input_cold_init.hpp"
#include "sgn/source_outer_scheduler.hpp"
#include <array>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {
using namespace sgn;using namespace sgn::retail;
void require(bool value,const char* why){if(!value)throw std::runtime_error(why);}
using ClockWords=std::array<std::uint32_t,4>;
ClockWords clock_words(const RaceSessionState& s){
    return {s.source_scheduler.frequency,s.tick_clock.increment,s.tick_clock.scheduled,s.tick_clock.observed};
}
void seed_clock(RaceSessionState& s){
    s.source_scheduler.frequency=71;s.tick_clock={73,79,83};
}
std::vector<std::uint32_t> physical_words(const RaceSessionState& s){
    std::vector<std::uint32_t> result;
    for(std::size_t i=0;i<4;++i){
        const auto& raw=s.source_input.acquisition.physical[i];const auto& extra=s.source_input.physical[i];
        result.insert(result.end(),{s.menu_input.raw_words[i].word_0,s.menu_input.raw_words[i].word_4,
            raw.word_8,raw.word_0c,raw.word_24,raw.word_28,extra.activity_kind,extra.profile_index,extra.feedback_mode,
            s.frontend_launch.input_bindings[i].vehicle,s.frontend_launch.input_bindings[i].kind});
        result.insert(result.end(),raw.words_from_2c.begin(),raw.words_from_2c.end());
        result.insert(result.end(),extra.bytes_168_187.begin(),extra.bytes_168_187.end());
        result.insert(result.end(),extra.bytes_188_1a7.begin(),extra.bytes_188_1a7.end());
        result.insert(result.end(),extra.bytes_1b6_1b7.begin(),extra.bytes_1b6_1b7.end());
        result.insert(result.end(),extra.bytes_1d8_1db.begin(),extra.bytes_1d8_1db.end());
        result.insert(result.end(),s.source_input.force_feedback.physical_words[i].begin(),s.source_input.force_feedback.physical_words[i].end());
    }
    return result;
}
void seed_physical(RaceSessionState& s){
    for(std::size_t i=0;i<4;++i){
        auto& raw=s.source_input.acquisition.physical[i];auto& extra=s.source_input.physical[i];
        s.menu_input.raw_words[i]={101,103};raw.word_8=107;raw.word_0c=109;raw.word_24=113;raw.word_28=127;
        raw.words_from_2c.fill(131);extra.activity_kind=137;extra.profile_index=139;extra.feedback_mode=149;
        extra.bytes_168_187.fill(151);extra.bytes_188_1a7.fill(157);extra.bytes_1b6_1b7.fill(163);extra.bytes_1d8_1db.fill(167);
        s.frontend_launch.input_bindings[i]={173,179};s.source_input.force_feedback.physical_words[i].fill(181);
    }
}
void require_cleared(const RaceSessionState& s){
    const auto values=physical_words(s);
    // Every checked cell is zero except each original +18 vehicle sentinel.
    constexpr std::size_t per_slot=11+79+32+32+2+4+8;
    require(values.size()==4*per_slot,"physical observation extent");
    for(std::size_t i=0;i<values.size();++i)
        require(values[i]==(i%per_slot==9?0xffffu:0u),"cold binding did not reach the actual scattered physical owners");
}
struct CounterFailure final:std::runtime_error{CounterFailure():std::runtime_error("receiving counter interruption") {}};
struct Counter final:FrontendPerformanceCounter {
    RaceSessionState& state;const ClockWords before;
    std::uint64_t hz_word;std::array<std::uint64_t,2> values;
    std::vector<char> calls;unsigned fail_at=99;
    Counter(RaceSessionState& s,std::uint64_t f,std::array<std::uint64_t,2> v):state(s),before(clock_words(s)),hz_word(f),values(v){}
    std::uint64_t frequency() override{
        require(calls.empty()&&clock_words(state)==before,"frequency acquisition order or premature source write");
        calls.push_back('F');if(fail_at==0)throw CounterFailure();return hz_word;
    }
    std::uint64_t counter() override{
        require(calls.size()==1||calls.size()==2,"counter duplicated or called before frequency");
        const auto index=calls.size()-1;const auto low=static_cast<std::uint32_t>(hz_word);
        require(clock_words(state)==ClockWords{low,low/60u,index?static_cast<std::uint32_t>(values[0]):before[2],before[3]},
            "clock writes do not precede the next distinct counter read in source order");
        calls.push_back('C');if(fail_at==index+1)throw CounterFailure();return values[index];
    }
};
void require_counter_result(const RaceSessionState& s,const Counter& c){
    const auto low=static_cast<std::uint32_t>(c.hz_word);
    require(c.calls==std::vector<char>{'F','C','C'}&&clock_words(s)==ClockWords{low,low/60u,
        static_cast<std::uint32_t>(c.values[0]),static_cast<std::uint32_t>(c.values[1])},"clock low DWORD/division/read result");
}
template<class F>void wrong_thread(F action){
    std::string error;
    std::thread worker([&]{try{action();}catch(const std::logic_error& e){error=e.what();}catch(...){error="wrong exception";}});
    worker.join();require(error=="race world and platform belong to the creating game thread","wrong-thread binding admission");
}
}

int main(int argc,char** argv)try{
    require(argc==2,"supply NEW retail asset root");
    // Exactly one actual platform. No eager Session, second world or input HWND.
    auto platform=std::make_shared<RaceSessionPlatform>(SourceColdConstructionOnly{},argv[1],0.0f);
    auto& s=platform->state();auto* identity=&s;
    auto alias=std::shared_ptr<RaceSessionState>(platform,identity);
    std::weak_ptr<RaceSessionPlatform> lifetime=platform;
    unsigned cases=0;
    s.source_scheduler.fixed_mode_a=191;s.source_scheduler.fixed_mode_b=193;s.simulation_tick=197;
    s.source_input.previous_activity=199;s.menu_input.previous_pause=211;
    s.source_input.force_feedback.initialized_576b70=223;
    s.startup.fields_62d480.fill(227);s.startup.fields_62dcf0.fill(229);
    const auto keys=s.startup.fields_62d480;const auto old_keys=s.startup.fields_62dcf0;
    const auto profiles=s.source_input.profiles;const auto vehicle_profiles=s.source_input.vehicle_profiles;
    const auto retained=[&]{
        const auto ready=platform->host_readiness();
        require(!ready.assets&&!ready.audio_device&&!ready.audio_stream&&!ready.window_input,
            "clock/input binding prepared an unrelated host backend");
        require(&platform->state()==identity&&alias.get()==identity&&!alias.owner_before(platform)&&!platform.owner_before(alias),"platform/world alias identity changed");
        require(s.source_scheduler.fixed_mode_a==191&&s.source_scheduler.fixed_mode_b==193&&s.simulation_tick==197&&
            s.source_input.previous_activity==199&&s.menu_input.previous_pause==211&&s.source_input.force_feedback.initialized_576b70==223&&
            s.startup.fields_62d480==keys&&s.startup.fields_62dcf0==old_keys&&s.source_input.profiles==profiles&&
            s.source_input.vehicle_profiles==vehicle_profiles,"cold binding changed a separate retained owner");
    };
    seed_clock(s);seed_physical(s);const auto physical_before_clock=physical_words(s);
    Counter clock(s,0x1234567800000259ull,{0x11111111fffffffeull,0x2222222200000002ull});
    initialize_platform_source_clock_434840(*platform,clock);require_counter_result(s,clock);
    require(physical_words(s)==physical_before_clock,"clock entry initialized physical input");retained();++cases;

    const auto time_before_input=clock_words(s);
    initialize_platform_source_physical_input_45ec30(*platform);require_cleared(s);
    require(clock_words(s)==time_before_input&&clock.calls.size()==3&&!platform->session_attachment_claimed(),
        "physical entry reset/reread clock or created a Session attachment");retained();++cases;

    // Explicit re-invocation is allowed by the existing leaves. No once flag.
    seed_clock(s);Counter sub_sixty_counter(s,0xffffffff0000003bull,{0x3333333300000003ull,0x4444444400000005ull});
    initialize_platform_source_clock_434840(*platform,sub_sixty_counter);require_counter_result(s,sub_sixty_counter);
    require(s.tick_clock.increment==0,"small low-DWORD frequency was clamped or replaced");
    seed_physical(s);initialize_platform_source_physical_input_45ec30(*platform);require_cleared(s);retained();++cases;

    // Retain the real reached source prefix if the explicit OS boundary fails.
    for(unsigned stop=0;stop<3;++stop){
        seed_clock(s);Counter interrupted(s,0x7654321000000259ull,{0xabcdef0100000011ull,0xabcdef0200000013ull});interrupted.fail_at=stop;
        bool rejected=false;try{initialize_platform_source_clock_434840(*platform,interrupted);}catch(const CounterFailure&){rejected=true;}
        const ClockWords expected=stop==0?interrupted.before:ClockWords{601,10,stop==2?17u:interrupted.before[2],interrupted.before[3]};
        require(rejected&&interrupted.calls.size()==stop+1&&clock_words(s)==expected,"counter failure rolled back or continued the source prefix");
        require_cleared(s);retained();
    }
    ++cases;

    seed_clock(s);seed_physical(s);const auto old_clock=clock_words(s);const auto old_physical=physical_words(s);
    Counter blocked(s,600,{1,2});
    wrong_thread([&]{initialize_platform_source_clock_434840(*platform,blocked);});
    wrong_thread([&]{initialize_platform_source_physical_input_45ec30(*platform);});
    require(blocked.calls.empty()&&clock_words(s)==old_clock&&physical_words(s)==old_physical,"wrong-thread pre-Session binding touched I/O or state");retained();++cases;

    // One cold attachment only. Attachment itself invokes neither source leaf.
    {
        const auto attached_clock=clock_words(s);const auto attached_physical=physical_words(s);
        {
            RaceSession session(SourceColdAttachmentOnly{},platform);
            require(session.readiness()==RaceSessionReadiness::source_attachment_only,"clock/input receiving activated a prepared race Session");
            require(session.shared_platform().get()==platform.get()&&session.shared_state().get()==identity&&
                !session.shared_state().owner_before(platform)&&!platform.owner_before(session.shared_state()),"Session created a second platform/world owner");
            require(clock_words(s)==attached_clock&&physical_words(s)==attached_physical,"Session attachment synthesized cold initialization");
            seed_clock(s);Counter through_session(s,0xfedcba98000004b0ull,{0xaaaaaaaa0000001full,0xbbbbbbbb00000025ull});
            initialize_session_source_clock_434840(session,through_session);require_counter_result(s,through_session);
            seed_physical(s);initialize_session_source_physical_input_45ec30(session);require_cleared(s);
            const auto guarded_clock=clock_words(s);const auto guarded_physical=physical_words(s);Counter denied(s,1200,{31,37});
            wrong_thread([&]{initialize_session_source_clock_434840(session,denied);});
            wrong_thread([&]{initialize_session_source_physical_input_45ec30(session);});
            require(denied.calls.empty()&&clock_words(s)==guarded_clock&&physical_words(s)==guarded_physical,"Session delegation bypassed platform thread guard");
            retained();
        }
        require(clock_words(s)==ClockWords{1200,20,31,37},"attachment teardown reset initialized clock");require_cleared(s);retained();
    }
    const auto retry_clock=clock_words(s);const auto retry_physical=physical_words(s);
    bool retry_rejected=false;
    try{RaceSession second(SourceColdAttachmentOnly{},platform);}
    catch(const FormatError& error){
        require(std::string_view(error.what())=="this platform already claimed its single session attachment","second cold attachment rejected at wrong boundary");
        retry_rejected=true;
    }
    require(retry_rejected&&platform->session_attachment_claimed()&&clock_words(s)==retry_clock&&physical_words(s)==retry_physical,
        "second cold attachment reset or replaced retained clock/input");retained();
    ++cases;
    alias.reset();platform.reset();require(lifetime.expired(),"cold binding retained a hidden platform owner");
    std::cout<<"passed=1 cold_clock_input_platform_cases="<<cases
        <<" actual_platforms=1 source_body_replacements=0 WindowInput4249B0=0 cold_main_activation=0 GPU_runs=0\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
