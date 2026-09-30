// Authored receiving; UNRUN by its author. Production bindings use one actual
// RaceSessionPlatform. Scripted services below are explicit receiving inputs.
#include "sgn/race_session.hpp"
#include "sgn/retail_frontend_state.hpp"
#include "sgn/source_cold_time.hpp"
#include "sgn/source_random_seed.hpp"
#include <algorithm>
#include <atomic>
#include <bit>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <span>
#include <thread>
#include <utility>
#include <vector>

using namespace sgn;
using namespace sgn::retail;
namespace {
unsigned checks{};
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
struct Pe {
    std::vector<std::uint8_t> bytes;
    std::uint32_t base{},sections{},count{};
    explicit Pe(const std::filesystem::path& path){
        std::ifstream input(path,std::ios::binary);check(bool(input),"pinned PE cannot be read");
        bytes.assign(std::istreambuf_iterator<char>(input),{});
        check((file_word(0)&0xffffu)==0x5a4d,"PE MZ");
        const auto nt=file_word(0x3c),opt=nt+24;
        check(file_word(nt)==0x4550&&(file_word(nt+4)&0xffffu)==0x14c,"PE signature/machine");
        check((file_word(opt)&0xffffu)==0x10b,"PE32 optional header");
        base=file_word(opt+28);check(base==0x400000,"PE preferred base");
        count=file_word(nt+4)>>16;sections=opt+(file_word(nt+20)&0xffffu);
    }
    std::uint32_t file_word(std::size_t at) const{
        if(at>bytes.size()||bytes.size()-at<4)throw std::runtime_error("PE file bounds");
        return std::uint32_t(bytes[at])|(std::uint32_t(bytes[at+1])<<8)|
            (std::uint32_t(bytes[at+2])<<16)|(std::uint32_t(bytes[at+3])<<24);
    }
    std::uint8_t byte(std::uint32_t va) const{
        check(va>=base,"PE VA before image");const auto rva=va-base;
        for(std::uint32_t i=0;i<count;++i){
            const auto at=std::size_t(sections)+40u*i;
            const auto virtual_size=file_word(at+8),start=file_word(at+12),raw_size=file_word(at+16);
            if(rva>=start&&rva-start<std::max(virtual_size,raw_size)){
                const auto delta=rva-start;if(delta>=raw_size)return 0;
                const auto offset=std::size_t(file_word(at+20))+delta;
                if(offset>=bytes.size())throw std::runtime_error("PE raw section bounds");
                return bytes[offset];
            }
        }
        throw std::runtime_error("PE VA is unmapped");
    }
    std::uint32_t word(std::uint32_t va) const{
        return std::uint32_t(byte(va))|(std::uint32_t(byte(va+1))<<8)|
            (std::uint32_t(byte(va+2))<<16)|(std::uint32_t(byte(va+3))<<24);
    }
    std::uint16_t half(std::uint32_t va) const{return static_cast<std::uint16_t>(byte(va)|(std::uint32_t(byte(va+1))<<8));}
};
std::array<std::uint16_t,8> fields(const SourceSystemTime& t){return {t.year,t.month,t.day_of_week,t.day,t.hour,t.minute,t.second,t.milliseconds};}
void compare_time(const Pe& pe,const SourceSystemTime& t,std::uint32_t va){
    const auto values=fields(t);for(unsigned i=0;i<8;++i)check(values[i]==pe.half(va+i*2u),"PE SYSTEMTIME field mismatch");
}
void initial_time(const Pe& pe,const RaceSessionPlatform& owner){
    const auto& s=owner.state();const auto& t=s.source_cold_time;
    check(s.random.state==pe.word(0x4959c0),"initial shared RNG PE value");
    for(const auto pair:std::array<std::pair<std::uint32_t,std::uint32_t>,13>{{
        {t.cached_dst,0x636580},{t.timezone_seconds,0x496150},{t.daylight,0x496154},{t.dst_bias_seconds,0x496158},
        {t.start.year,0x4961e8},{t.start.day,0x4961ec},{t.start.milliseconds,0x4961f0},
        {t.end.year,0x4961f8},{t.end.day,0x4961fc},{t.end.milliseconds,0x496200},
        {t.have_os_rules,0x6365f8},{t.timezone_initialized,0x6366b0},{t.code_page,0x636840}}})
        check(pair.first==pe.word(pair.second),"PE time scalar mismatch");
    compare_time(pe,t.cached_utc,0x636588);
    for(unsigned i=0;i<64;++i){
        check(static_cast<unsigned char>(t.standard_name[i])==pe.byte(0x49615c+i),"PE standard name");
        check(static_cast<unsigned char>(t.daylight_name[i])==pe.byte(0x49619c+i),"PE daylight name");
    }
    const auto& z=t.retained_zone;
    check(std::bit_cast<std::uint32_t>(z.bias)==pe.word(0x636600),"PE TZI bias");
    for(unsigned i=0;i<32;++i){
        check(z.standard_name[i]==pe.half(0x636604+i*2u),"PE TZI standard name");
        check(z.daylight_name[i]==pe.half(0x636658+i*2u),"PE TZI daylight name");
    }
    compare_time(pe,z.standard_date,0x636644);compare_time(pe,z.daylight_date,0x636698);
    check(std::bit_cast<std::uint32_t>(z.standard_bias)==pe.word(0x636654),"PE TZI standard bias");
    check(std::bit_cast<std::uint32_t>(z.daylight_bias)==pe.word(0x6366a8),"PE TZI daylight bias");
    check(pe.word(0x6366ac)==0&&!t.cached_tz,"PE cached TZ is null");
    check(pe.word(0x4961dc)==0x49615c&&pe.word(0x4961e0)==0x49619c,"source name aliases changed");
}
// Invocation-local diagnostic bytes; no second runtime time/cache owner.
std::string time_snapshot(const SourceColdTimeState& t){
    std::string result;
    const auto scalar=[&](auto value){const auto raw=std::bit_cast<std::array<char,sizeof(value)>>(value);result.append(raw.data(),raw.size());};
    const auto time=[&](const SourceSystemTime& value){for(auto item:fields(value))scalar(item);};
    scalar(t.cached_dst);time(t.cached_utc);scalar(t.timezone_seconds);scalar(t.daylight);scalar(t.dst_bias_seconds);
    result.append(t.standard_name.data(),t.standard_name.size());result.append(t.daylight_name.data(),t.daylight_name.size());
    scalar(t.start.year);scalar(t.start.day);scalar(t.start.milliseconds);scalar(t.end.year);scalar(t.end.day);scalar(t.end.milliseconds);
    scalar(t.have_os_rules);const auto& z=t.retained_zone;scalar(z.bias);
    for(auto c:z.standard_name)scalar(c);time(z.standard_date);scalar(z.standard_bias);
    for(auto c:z.daylight_name)scalar(c);time(z.daylight_date);scalar(z.daylight_bias);
    scalar(reinterpret_cast<std::uintptr_t>(t.cached_tz.get()));scalar(t.timezone_initialized);scalar(t.code_page);
    if(t.cached_tz)result.append(t.cached_tz.get());return result;
}
void no_backends(RaceSessionPlatform& owner){
    const auto h=owner.host_readiness();check(!h.assets&&!h.audio_device&&!h.audio_stream&&!h.window_input,"time binding prepared an unrelated backend");
}
std::shared_ptr<RaceSessionPlatform> fresh(const std::filesystem::path& missing){
    return std::make_shared<RaceSessionPlatform>(SourceColdConstructionOnly{},missing,0.0f);
}
struct Reply {std::uint32_t status{};SourceTimeZoneInformation zone{};};
struct Fixture final : SourceColdTimePlatform {
    SourceSystemTime local{1970,1,4,1,0,0,0,0},utc{1970,1,4,1,0,0,0,0};
    std::optional<std::string> tz;
    std::vector<Reply> replies;
    std::size_t reply_cursor{};
    std::string calls;
    void get_local_time(SourceSystemTime& out) override{calls+='L';out=local;}
    void get_system_time(SourceSystemTime& out) override{calls+='U';out=utc;}
    std::uint32_t get_time_zone_information(SourceTimeZoneInformation& out) override{
        calls+='Z';check(reply_cursor<replies.size(),"unexpected fixture TZI query");
        const auto& r=replies[reply_cursor++];out=r.zone;return r.status;
    }
    const char* read_tz_46c764() override{calls+='E';return tz?tz->c_str():nullptr;}
    std::unique_ptr<char[]> allocate_tz_copy(std::size_t n) override{calls+='A';return std::make_unique<char[]>(n);}
    SourceNameConversion convert_zone_name(std::uint32_t,const std::array<char16_t,32>&,std::array<char,64>&) override{
        calls+='N';throw std::runtime_error("name conversion is outside this fixture's path");
    }
};
void seed_time_lifetime(const Pe& pe,const std::filesystem::path& missing){
    auto owner=fresh(missing);auto& s=owner->state();auto* const time_identity=&s.source_cold_time;auto* const rng_identity=&s.random;
    initial_time(pe,*owner);no_backends(*owner);const auto initial=time_snapshot(s.source_cold_time);
    for(const auto seed:std::array<std::uint32_t,5>{0,1,0x80000000u,0xffffffffu,0x12345678u}){
        seed_platform_source_random_42f190(*owner,seed);
        check(s.random.state==seed&&time_snapshot(s.source_cold_time)==initial,"explicit platform seed drew RNG or changed time state");
    }
    const char* cache_identity{};
    {
        Fixture api;api.tz="UTC0";api.replies={{0,{}}};
        initialize_platform_source_random_time_42f1a0(*owner,api);
        check(api.calls=="LUZEA"&&s.random.state==0,"pre-Session time query/seed count and epoch value");
        check(s.source_cold_time.timezone_initialized==1&&s.source_cold_time.timezone_seconds==0,"CRT cache not initialized on actual world");
        cache_identity=s.source_cold_time.cached_tz.get();
        check(cache_identity&&cache_identity!=api.tz->c_str()&&std::strcmp(cache_identity,"UTC0")==0,"world does not own its retained TZ copy");
    }
    check(std::strcmp(cache_identity,"UTC0")==0,"service destruction invalidated world cache");
    auto retained=std::shared_ptr<RaceSessionState>(owner,&s);const auto before_attach=time_snapshot(s.source_cold_time);
    std::string expected_retained;std::uint32_t expected_rng{};
    {
        FrontendState frontend(retained,owner);RaceSession session(SourceColdAttachmentOnly{},owner);
        check(&session.state().source_cold_time==time_identity&&&session.state().random==rng_identity,"session copied time/RNG owners");
        check(!retained.owner_before(owner)&&!owner.owner_before(retained)&&&frontend.world()==&session.state(),"platform/world control blocks differ");
        check(time_snapshot(s.source_cold_time)==before_attach&&s.random.state==0,"attachment reset cold values");
        Fixture api;api.tz="CET-1CEST";api.local.second=1;api.utc.second=59;api.utc.milliseconds=999;
        initialize_session_source_random_time_42f1a0(session,api);
        check(api.calls=="LU"&&s.random.state==1,"Session delegate repeated calls or refreshed seconds-only cache");
        check(time_snapshot(s.source_cold_time)==before_attach,"new service or Session delegate reset retained CRT cache");
        api.calls.clear();api.utc.minute=1;api.replies={{0,{}}};
        initialize_platform_source_random_time_42f1a0(*owner,api);
        check(api.calls=="LUZ"&&s.random.state==1,"minute refresh must be separate from CRT cache initialization");
        check(s.source_cold_time.cached_tz.get()==cache_identity&&std::strcmp(cache_identity,"UTC0")==0,"UTC refresh replaced TZ cache identity");
        const auto before_seed=time_snapshot(s.source_cold_time);
        seed_session_source_random_42f190(session,0xffffffffu);
        check(s.random.state==0xffffffffu&&time_snapshot(s.source_cold_time)==before_seed,"Session seed delegate changed cache or discarded bits");
        const auto rng_before=s.random.state;api.calls.clear();std::atomic<unsigned> rejected{};
        std::thread foreign([&]{
            try{seed_platform_source_random_42f190(*owner,7);}catch(const std::logic_error&){++rejected;}
            try{initialize_platform_source_random_time_42f1a0(*owner,api);}catch(const std::logic_error&){++rejected;}
            try{seed_session_source_random_42f190(session,8);}catch(const std::logic_error&){++rejected;}
            try{initialize_session_source_random_time_42f1a0(session,api);}catch(const std::logic_error&){++rejected;}
        });foreign.join();
        check(rejected==4&&api.calls.empty()&&s.random.state==rng_before&&time_snapshot(s.source_cold_time)==before_seed,"foreign call reached service/state");
        no_backends(*owner);check(session.readiness()==RaceSessionReadiness::source_attachment_only,"time binding published scene readiness");
        expected_retained=time_snapshot(s.source_cold_time);expected_rng=s.random.state;
    }
    check(time_snapshot(s.source_cold_time)==expected_retained&&s.random.state==expected_rng,"Session destruction reset retained time/RNG values");
    const auto after_destroy=time_snapshot(s.source_cold_time);const auto seed_after=s.random.state;
    bool duplicate=false;try{RaceSession another(SourceColdAttachmentOnly{},owner);}catch(const FormatError&){duplicate=true;}
    check(duplicate&&time_snapshot(s.source_cold_time)==after_destroy&&s.random.state==seed_after,"rejected second attachment changed time/RNG");
    check(s.source_cold_time.cached_tz.get()==cache_identity&&&retained->source_cold_time==time_identity,"Session destruction replaced the time cache");
    std::weak_ptr<RaceSessionPlatform> weak=owner;owner.reset();
    check(!weak.expired()&&retained->source_cold_time.cached_tz.get()==cache_identity,"world alias failed to retain time allocation owner");
    retained.reset();check(weak.expired(),"time binding retained an ownership cycle");
}
void error_sentinel(const std::filesystem::path& missing){
    auto owner=fresh(missing);auto& s=owner->state();Fixture api;api.local.year=2039;api.replies={{0,{}}};
    seed_platform_source_random_42f190(*owner,0x11223344u);
    initialize_platform_source_random_time_42f1a0(*owner,api);
    check(s.random.state==0xffffffffu&&api.calls=="LUZ","time error sentinel was suppressed or successful time invented");
    check(s.source_cold_time.timezone_initialized==0&&!s.source_cold_time.cached_tz,"invalid year initialized CRT cache");
    const auto saved=time_snapshot(s.source_cold_time);
    RaceSession session(SourceColdAttachmentOnly{},owner);api.calls.clear();
    initialize_session_source_random_time_42f1a0(session,api);
    check(s.random.state==0xffffffffu&&api.calls=="LU"&&time_snapshot(s.source_cold_time)==saved,"Session error path differs or reset cache");
    no_backends(*owner);
}
void unwritten_minute(const std::filesystem::path& missing){
    auto owner=fresh(missing);auto& s=owner->state();auto& t=s.source_cold_time;
    // Explicit prior-state fixture for the reached legacy DST fallback. No
    // production adapter creates these values or supplies missing tm fields.
    t.timezone_initialized=1;t.timezone_seconds=0;t.daylight=1;
    Fixture api;api.local={2024,4,0,7,2,37,42,123};api.utc=api.local;api.replies={{0xffffffffu,{}}};
    seed_platform_source_random_42f190(*owner,0x12345678u);bool failed=false;
    try{initialize_platform_source_random_time_42f1a0(*owner,api);}catch(const SourceTimeBoundaryError& error){failed=error.instruction()==0x469efbu;}
    check(failed&&s.random.state==0x12345678u&&api.calls=="LUZ","unwritten tm.minute was invented or boundary seeded RNG");
    check(t.cached_dst==0xffffffffu&&t.cached_utc==api.utc&&t.start.year==124&&t.end.year==124&&t.start.day==97,"boundary discarded earlier cache writes");
    const auto prior=time_snapshot(t);RaceSession session(SourceColdAttachmentOnly{},owner);api.calls.clear();failed=false;
    try{initialize_session_source_random_time_42f1a0(session,api);}catch(const SourceTimeBoundaryError& error){failed=error.instruction()==0x469efbu;}
    check(failed&&api.calls=="LU"&&s.random.state==0x12345678u&&time_snapshot(t)==prior,"Session boundary changed error/cache/RNG semantics");
    no_backends(*owner);
}
// This observer records actual Win32 calls; it is not a second source cache.
struct NativeTrace final : SourceColdTimePlatform {
    Win32SourceColdTimePlatform actual;
    SourceSystemTime observed_local{},observed_utc{};
    SourceTimeZoneInformation observed_zone{};
    std::uint32_t observed_zone_status{};
    std::string calls;
    explicit NativeTrace(std::string snapshot):actual(std::optional<std::string>{std::move(snapshot)}){}
    void get_local_time(SourceSystemTime& out) override{calls+='L';actual.get_local_time(out);observed_local=out;}
    void get_system_time(SourceSystemTime& out) override{calls+='U';actual.get_system_time(out);observed_utc=out;}
    std::uint32_t get_time_zone_information(SourceTimeZoneInformation& out) override{
        calls+='Z';observed_zone_status=actual.get_time_zone_information(out);observed_zone=out;return observed_zone_status;
    }
    const char* read_tz_46c764() override{calls+='E';return actual.read_tz_46c764();}
    std::unique_ptr<char[]> allocate_tz_copy(std::size_t n) override{calls+='A';return actual.allocate_tz_copy(n);}
    SourceNameConversion convert_zone_name(std::uint32_t cp,const std::array<char16_t,32>& in,std::array<char,64>& out) override{
        calls+='N';return actual.convert_zone_name(cp,in,out);
    }
};
void actual_platform_native(const Pe& pe,const std::filesystem::path& missing,const std::string& snapshot){
    check(snapshot=="UTC0","native receiving requires explicit UTC0 fixture; it does not discover CRT TZ");
    auto owner=fresh(missing);initial_time(pe,*owner);auto& s=owner->state();NativeTrace api(snapshot);
    initialize_platform_source_random_time_42f1a0(*owner,api);
    const auto& local=api.observed_local;
    check(local.year>=1970&&local.year<=2038,"actual OS date is outside this native receiving witness domain");
    const std::chrono::year_month_day date{std::chrono::year{static_cast<int>(local.year)},
        std::chrono::month{static_cast<unsigned>(local.month)},std::chrono::day{static_cast<unsigned>(local.day)}};
    check(date.ok(),"actual local calendar date invalid");
    // Independent Gregorian witness is receiving-only, never source arithmetic.
    const auto days=(std::chrono::sys_days{date}-std::chrono::sys_days{std::chrono::year{1970}/1/1}).count();
    const std::int64_t seconds=std::int64_t(days)*86400+std::int64_t(local.hour)*3600+std::int64_t(local.minute)*60+local.second;
    auto expected=static_cast<std::uint32_t>(seconds);
    const auto expected_dst=api.observed_zone_status==0xffffffffu?0xffffffffu:
        (api.observed_zone_status==2u&&api.observed_zone.daylight_date.month!=0&&api.observed_zone.daylight_bias!=0?1u:0u);
    check(s.source_cold_time.cached_dst==expected_dst,"cached DST differs from actual observed native TZI result");
    if(expected_dst==1u)expected-=3600u; //source adds retained -3600 even with UTC0 daylight=0
    check(s.random.state==expected&&api.calls=="LUZEA","actual platform time seed/call order differs from captured native sample");
    check(s.source_cold_time.cached_utc==api.observed_utc&&s.source_cold_time.timezone_initialized==1,"native time was not retained by world");
    check(s.source_cold_time.timezone_seconds==0&&s.source_cold_time.daylight==0&&s.source_cold_time.cached_tz&&
          std::strcmp(s.source_cold_time.cached_tz.get(),"UTC0")==0,"explicit native TZ fixture/cache mismatch");
    const auto retained=time_snapshot(s.source_cold_time);const auto seed=s.random.state;auto* identity=&s.source_cold_time;
    {RaceSession session(SourceColdAttachmentOnly{},owner);
        check(&session.state().source_cold_time==identity&&time_snapshot(s.source_cold_time)==retained&&s.random.state==seed,"attachment reset actual native result");
        no_backends(*owner);}
    check(time_snapshot(s.source_cold_time)==retained&&s.random.state==seed,"destruction reset actual native cache");
    std::cout<<"actual_native_seed="<<seed<<" local_year="<<local.year<<" retained_dst="<<s.source_cold_time.cached_dst<<'\n';
}
}
int main(int argc,char** argv)try{
    if(argc!=2&&argc!=4)throw std::runtime_error("expected pinned PE path, optionally --native-tz UTC0");
    const Pe pe(argv[1]);const auto missing=std::filesystem::path(argv[1]).parent_path()/"__missing_seed_time_platform_assets__";
    check(!std::filesystem::exists(missing),"receiving missing asset directory unexpectedly exists");
    if(argc==4){check(std::string_view(argv[2])=="--native-tz","unsupported native receiving option");actual_platform_native(pe,missing,argv[3]);}
    else{seed_time_lifetime(pe,missing);error_sentinel(missing);unwritten_minute(missing);}
    std::cout<<"source_seed_time_actual_platform checks="<<checks<<" scene_ready=0 full_cold=0\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
