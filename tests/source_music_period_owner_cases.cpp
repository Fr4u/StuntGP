// Source-only receiving cases. Real RaceSessionPlatform, owned fake API;
// neither the installed WinMM backend nor an HWND is used by these cases.
#include "sgn/source_music_period_host.hpp"
#include "sgn/race_session_platform.hpp"
#include "sgn/race_session.hpp"
#include <array>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <new>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <tuple>

namespace { thread_local bool deny_allocations{}; }
void* operator new(std::size_t bytes) {
    if (deny_allocations) throw std::bad_alloc();
    if (void* p = std::malloc(bytes == 0 ? 1 : bytes)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

namespace {
using namespace sgn::retail;
void require(bool value, const char* why) { if (!value) throw std::runtime_error(why); }
struct Event { char operation{}; std::uint32_t period{}; };
struct Calls final : SourceMusicPeriodCalls {
    const std::thread::id thread{std::this_thread::get_id()};
    std::array<Event,64> events{};
    std::array<std::uint32_t,16> end_results{};
    std::size_t count{}, end_index{};
    std::uint32_t begin_result{};
    bool deny_after_begin{}, end_saw_committed_begin{}, reentry_rejected{};
    std::shared_ptr<RaceSessionPlatform>* release_in_begin{};
    std::shared_ptr<RaceSessionPlatform>* reenter_in_begin{};
    std::shared_ptr<const SourceMusicPeriodReport> observer;
    void event(char kind,std::uint32_t period) noexcept {
        if (std::this_thread::get_id()!=thread) std::_Exit(92);
        if (count==events.size()) std::terminate();
        events[count++]={kind,period};
    }
    std::uint32_t begin(std::uint32_t period) noexcept override {
        event('B',period);
        if (reenter_in_begin) {
            try { (void)end_owned_music_period(*reenter_in_begin,period); }
            catch(const std::logic_error&) { reentry_rejected=true; }
        }
        if (release_in_begin) release_in_begin->reset();
        if (deny_after_begin) deny_allocations=true;
        return begin_result;
    }
    std::uint32_t end(std::uint32_t period) noexcept override {
        event('E',period);
        if (observer) end_saw_committed_begin=observer->snapshot().begin_calls!=0;
        if (end_index==end_results.size()) std::terminate();
        return end_results[end_index++];
    }
};
struct Fixture {
    std::shared_ptr<RaceSessionPlatform> platform=std::make_shared<RaceSessionPlatform>(SourceColdConstructionOnly{},std::filesystem::path{});
    std::shared_ptr<Calls> calls=std::make_shared<Calls>();
    std::shared_ptr<const SourceMusicPeriodReport> report=prepare_receiving_music_period_owner(platform,calls);
    Fixture() { calls->observer=report; }
};
auto source_words(const RaceSessionPlatform& owner) {
    const auto& state=owner.state();const auto& s=state.audio_stream;
    return std::tuple{s.timer_capabilities.minimum(),s.timer_capabilities.maximum(),s.timer_period,
        s.allocated,s.flags,s.result,s.timer,s.file.value,state.audio_maintenance.music.buffer.value};
}

void cold_and_source_words() {
    Fixture f;auto& s=f.platform->state().audio_stream;
    s.timer_capabilities.minimum()=1;s.timer_capabilities.maximum()=255;s.timer_period=7;
    s.allocated=0;s.result=0x89abcdefu;s.flags=0x10203040u;
    const auto before=source_words(*f.platform);const auto ready=f.platform->host_readiness();
    require(!ready.assets&&!ready.audio_device&&!ready.audio_stream&&!ready.window_input,"period preparation created a broader backend");
    require(f.calls->count==0,"preparation made a native-shaped call");
    require(begin_owned_music_period(f.platform,s.timer_period)==0,"begin result changed");
    require(before==source_words(*f.platform),"host ownership changed source words");
    require(f.calls->count==1&&f.report->snapshot().pending_calls==1,"request ended at isolated return");
    f.platform.reset();const auto report=f.report->snapshot();
    require(f.calls->count==2&&f.calls->events[1].operation=='E'&&f.calls->events[1].period==7,"allocated0 host teardown failed to pair the request");
    require(report.finalized&&report.teardown_end_calls==1&&report.unresolved_at_teardown==0,"successful host cleanup report differs");
}
void repeated_requests() {
    Fixture f;begin_owned_music_period(f.platform,3);begin_owned_music_period(f.platform,3);begin_owned_music_period(f.platform,9);
    require(f.report->snapshot().pending_calls==3,"repeated calls were collapsed");
    f.platform.reset();require(f.calls->count==6,"missing repeated cleanup");
    require(f.calls->events[3].period==3&&f.calls->events[4].period==3&&f.calls->events[5].period==9,"cleanup periods/order changed");
}
void explicit_end_once() {
    Fixture f;begin_owned_music_period(f.platform,5);begin_owned_music_period(f.platform,5);
    require(end_owned_music_period(f.platform,5)==0,"explicit End result changed");
    require(f.report->snapshot().pending_calls==1,"explicit End retired wrong number");
    f.platform.reset();require(f.calls->count==4&&f.report->snapshot().teardown_end_calls==1,"explicit End duplicated at teardown");
}
void failed_begin_is_attempt() {
    Fixture f;f.calls->begin_result=97;
    require(begin_owned_music_period(f.platform,0)==97,"failed Begin was trapped or rewritten");
    const auto report=f.report->snapshot();
    require(report.begin_successes==0&&report.begin_failures==1&&report.pending_calls==1,"failed Begin misclassified as acquisition");
    f.platform.reset();require(f.calls->events[1].period==0,"reached failed Begin lost exact pairing argument");
}
void failed_explicit_end() {
    Fixture f;f.calls->end_results[0]=97;begin_owned_music_period(f.platform,2);
    require(end_owned_music_period(f.platform,2)==97&&f.report->snapshot().pending_calls==1,"failed End was marked released");
    f.platform.reset();const auto report=f.report->snapshot();
    require(report.end_failures==1&&report.end_successes==1&&report.teardown_end_calls==1,"bounded host fallback differs");
}
void teardown_failure_visible() {
    Fixture f;f.calls->end_results[0]=97;begin_owned_music_period(f.platform,4);f.platform.reset();
    const auto report=f.report->snapshot();
    require(report.finalized&&report.end_successes==0&&report.unresolved_at_teardown==1,"failed host cleanup hidden");
    require(f.calls->count==2,"teardown retried without bound");
}
void unmatched_period() {
    Fixture f;begin_owned_music_period(f.platform,4);bool rejected=false;
    try { (void)end_owned_music_period(f.platform,8); } catch(const std::logic_error&) { rejected=true; }
    require(rejected&&f.calls->count==1&&f.report->snapshot().pending_calls==1,"unmatched End reached native API");
    f.platform.reset();require(f.calls->events[1].period==4&&f.report->snapshot().unmatched_ends==1,"matching obligation lost");
}
void reservation_failure() {
    Fixture f;bool failed=false;deny_allocations=true;
    try { (void)begin_owned_music_period(f.platform,6); } catch(const std::bad_alloc&) { failed=true; }
    deny_allocations=false;
    require(failed&&f.calls->count==0&&f.report->snapshot().pending_calls==0,"allocation failed after external acquisition");
}
void no_allocation_after_begin() {
    Fixture f;f.calls->deny_after_begin=true;bool failed=false;
    try { (void)begin_owned_music_period(f.platform,6); } catch(const std::bad_alloc&) { failed=true; }
    deny_allocations=false;
    require(!failed&&f.report->snapshot().begin_successes==1&&f.report->snapshot().pending_calls==1,"post-Begin allocation window");
    deny_allocations=true;f.platform.reset();deny_allocations=false;
    require(f.report->snapshot().teardown_end_calls==1,"teardown could not finish without allocation");
}
void retained_owner_during_call() {
    Fixture f;f.calls->release_in_begin=&f.platform;
    require(begin_owned_music_period(f.platform,10)==0,"retained call failed");
    require(!f.platform&&f.calls->count==2&&f.calls->end_saw_committed_begin,"platform died inside Begin before acceptance");
    require(f.report->snapshot().finalized,"last platform release failed to finalize");
}
void binding_and_legacy_guards() {
    Fixture f;bool replaced=false,legacy=false;
    try { (void)prepare_receiving_music_period_owner(f.platform,f.calls); } catch(const std::logic_error&) { replaced=true; }
    try { f.platform->prepare_host_audio(); } catch(const std::runtime_error&) { legacy=true; }
    const auto ready=f.platform->host_readiness();
    require(replaced&&legacy&&!ready.audio_device&&!ready.audio_stream&&f.calls->count==0,"binding guard prepared legacy resources");
}
void foreign_operation_guard() {
    Fixture f;bool rejected=false;
    std::thread worker([&] { try { (void)begin_owned_music_period(f.platform,2); } catch(const std::logic_error&) { rejected=true; } });
    worker.join();require(rejected&&f.calls->count==0,"foreign operation reached backend");
}
void foreign_report_guard() {
    Fixture f;bool rejected=false;
    std::thread worker([&] { try { (void)f.report->snapshot(); } catch(const std::logic_error&) { rejected=true; } });
    worker.join();require(rejected,"foreign report read mutable counters");
}
void missing_owner_guard() {
    auto owner=std::make_shared<RaceSessionPlatform>(SourceColdConstructionOnly{},std::filesystem::path{});bool rejected=false;
    try { (void)begin_owned_music_period(owner,1); } catch(const std::logic_error&) { rejected=true; }
    require(rejected,"unprepared owner reached a backend");
}
void reentry_guard() {
    Fixture f;f.calls->reenter_in_begin=&f.platform;begin_owned_music_period(f.platform,3);
    require(f.calls->reentry_rejected&&f.calls->count==1&&f.report->snapshot().pending_calls==1,"API reentry reached a second native call");
}
[[noreturn]] void foreign_destroy_case() {
    std::set_terminate([] { std::_Exit(91); });
    Fixture f;begin_owned_music_period(f.platform,1);
    std::thread worker([owner=std::move(f.platform)]() mutable { owner.reset(); });
    worker.join();std::_Exit(93);
}
}

int main(int argc,char** argv) {
    if(argc==2&&std::string_view(argv[1])=="--foreign-destroy") foreign_destroy_case();
    if(argc!=1) return 2;
    try {
        cold_and_source_words();repeated_requests();explicit_end_once();failed_begin_is_attempt();
        failed_explicit_end();teardown_failure_visible();unmatched_period();reservation_failure();
        no_allocation_after_begin();retained_owner_during_call();binding_and_legacy_guards();
        foreign_operation_guard();foreign_report_guard();missing_owner_guard();reentry_guard();
        std::cout<<"music period owner receiving: 15 cases passed\n";
        return 0;
    } catch(const std::exception& error) {
        deny_allocations=false;std::cerr<<error.what()<<'\n';return 1;
    }
}
