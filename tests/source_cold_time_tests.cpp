// Written receiving fixtures. Session has NOT compiled or executed this file.
// Scripted external results exercise source C++; they do not prove Win32/original parity.
#include "sgn/source_cold_time.hpp"
#include "sgn/vehicle_math.hpp"
#include <array>
#include <cstring>
#include <iostream>
#include <new>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

using namespace sgn::retail;
namespace {
using Word=std::uint32_t;
void require(bool value,const char* why) { if(!value) throw std::runtime_error(why); }
struct ZoneReply {
    Word status{};
    std::optional<SourceTimeZoneInformation> replacement;
    std::optional<std::int32_t> bias_write;
};
struct Fixture final : SourceColdTimePlatform {
    SourceSystemTime local{1970,1,4,1,0,0,0,0},utc{1970,1,4,1,0,0,0,0};
    std::optional<std::string> tz;
    std::vector<ZoneReply> zones;
    std::vector<SourceNameConversion> conversions;
    std::string calls;
    std::size_t zone_index{},conversion_index{};
    bool allocation_fails{};
    void get_local_time(SourceSystemTime& out) override { calls+='L'; out=local; }
    void get_system_time(SourceSystemTime& out) override { calls+='U'; out=utc; }
    Word get_time_zone_information(SourceTimeZoneInformation& out) override {
        calls+='Z'; require(zone_index<zones.size(),"unexpected time-zone query");
        const auto& reply=zones[zone_index++];
        if(reply.replacement) out=*reply.replacement;
        if(reply.bias_write) out.bias=*reply.bias_write;
        return reply.status;
    }
    const char* read_tz_46c764() override { calls+='E'; return tz?tz->c_str():nullptr; }
    std::unique_ptr<char[]> allocate_tz_copy(std::size_t bytes) override {
        calls+='A'; return allocation_fails?nullptr:std::unique_ptr<char[]>(new(std::nothrow) char[bytes]);
    }
    SourceNameConversion convert_zone_name(Word page,const std::array<char16_t,32>&,
        std::array<char,64>& out) override {
        calls+='N'; require(page==0,"unexpected source code page");
        out[0]='X'; out[1]='Y'; out[2]=0; // Deliberate partial output; tail must survive.
        return conversion_index<conversions.size()?conversions[conversion_index++]:SourceNameConversion{3,false};
    }
};
void prepare_utc(SourceColdTimeState& state) {
    state.timezone_initialized=1; state.timezone_seconds=0; state.daylight=0;
}
template<class Function> void expect_boundary(Function&& function,Word instruction) {
    bool reached=false;
    try { function(); } catch(const SourceTimeBoundaryError& error) {
        reached=true; require(error.instruction()==instruction,"wrong rejected source read");
    }
    require(reached,"missing explicit source-domain rejection");
}
void initial_words() {
    SourceColdTimeState state;
    require(state.timezone_seconds==28800&&state.daylight==1&&state.dst_bias_seconds==0xfffff1f0u,"PE timezone words");
    require(state.cached_dst==0&&state.cached_utc==SourceSystemTime{}&&state.timezone_initialized==0,"PE caches");
    require(state.start.year==0xffffffffu&&state.end.year==0xffffffffu&&!state.cached_tz,"PE transition/cache owners");
    require(std::string(state.standard_name.data())=="PST"&&std::string(state.daylight_name.data())=="PDT","PE names");
}
void dates_and_signed_wrap() {
    const std::array<SourceSystemTime,5> dates{{{1970,1,0,1,0,0,0,0},{2000,2,0,29,12,34,56,0},
        {2038,1,0,19,3,14,7,0},{2038,1,0,19,3,14,8,0},{2038,12,0,31,23,59,59,0}}};
    // Independent Gregorian UTC timestamp witnesses (Python datetime, witness JSON).
    const std::array<Word,5> expected{0u,0x38bbbcf0u,0x7fffffffu,0x80000000u,0x81c94affu};
    SourceColdTimeState state; prepare_utc(state); Fixture f;
    for(std::size_t i=0;i<dates.size();++i)
        require(convert_source_local_time_468374(state,f,dates[i],0)==expected[i],"date/wrap witness");
    state.timezone_initialized=0;
    require(convert_source_local_time_468374(state,f,{1969,0,0,0,0,0,0,0},0)==0xffffffffu,"lower year gate before table");
    require(convert_source_local_time_468374(state,f,{2039,0,0,0,0,0,0,0},0)==0xffffffffu,"upper year gate before table");
    require(f.calls.empty()&&state.timezone_initialized==0,"invalid year initialized timezone");
}
void cache_and_call_order() {
    SourceColdTimeState state; Fixture f;
    f.zones={{0,SourceTimeZoneInformation{},{}},{0,SourceTimeZoneInformation{},{}}};
    Word output=123;
    require(read_source_time_4632a7(state,f,&output)==0&&output==0,"epoch/output pointer");
    require(f.calls=="LUZEZNN"&&state.timezone_initialized==1,"local/UTC/outerTZ/CRT/innerTZ order");
    const auto first_cache=state.cached_utc;
    f.calls.clear(); f.local.second=1; f.utc.second=59; f.utc.milliseconds=999; f.utc.day_of_week=6;
    require(read_source_time_4632a7(state,f)==1&&f.calls=="LU","seconds/ms/dayOfWeek invalidated cache");
    require(state.cached_utc==first_cache,"cache hit incorrectly copied UTC bytes");
    ++f.utc.minute;
    SourceTimeZoneInformation changed; changed.bias=600;
    f.zones.push_back({0,changed,{}}); f.calls.clear();
    require(read_source_time_4632a7(state,f)==1&&f.calls=="LUZ","minute refresh/lazy cache contract");
    require(state.cached_utc==f.utc&&state.retained_zone.bias==0&&state.timezone_seconds==0,"outer query rewrote retained CRT zone");
    // Each of the other compared fields independently invalidates the minute cache.
    for(unsigned field=0;field<4;++field) {
        if(field==0) ++f.utc.hour;
        if(field==1) ++f.utc.day;
        if(field==2) ++f.utc.month;
        if(field==3) ++f.utc.year;
        f.zones.push_back({0,{}, {}}); f.calls.clear();
        (void)read_source_time_4632a7(state,f);
        require(f.calls=="LUZ"&&state.cached_utc==f.utc,"UTC comparison missing");
    }
}
void outer_dst_status_gate() {
    struct Case { Word status; std::uint16_t month; std::int32_t bias; Word dst; };
    for(const auto test:std::array<Case,6>{{{0,4,-60,0},{1,4,-60,0},{2,0,-60,0},
        {2,4,0,0},{2,4,-60,1},{0xffffffffu,4,-60,0xffffffffu}}}) {
        SourceColdTimeState state; prepare_utc(state); Fixture f;
        SourceTimeZoneInformation zone; zone.daylight_date.month=test.month; zone.daylight_bias=test.bias;
        f.zones.push_back({test.status,zone,{}});
        const Word value=read_source_time_4632a7(state,f);
        require(state.cached_dst==test.dst,"outer DST predicate");
        require(value==(test.dst==1?0xfffff1f0u:0u),"explicit DST=1 must add bias even daylight=0");
    }
}
void errors_retained_writes_and_once() {
    SourceColdTimeState state; Fixture f;
    state.start.year=55; state.end.year=66; state.retained_zone.bias=700;
    f.zones={{0,{},{}},{0xffffffffu,{},999}};
    require(read_source_time_4632a7(state,f)==28800,"inner timezone failure erased PE bias");
    require(state.retained_zone.bias==999&&state.have_os_rules==0&&state.timezone_initialized==1,"failure output/once state");
    require(state.start.year==0xffffffffu&&state.end.year==0xffffffffu,"entry invalidations lost");
    require(std::string(state.standard_name.data())=="PST"&&f.calls=="LUZEZ","failure must skip name conversions");
    f.calls.clear(); ++f.utc.minute; f.zones.push_back({0,SourceTimeZoneInformation{}, {}});
    require(read_source_time_4632a7(state,f)==28800&&f.calls=="LUZ","failed tzset retried automatically");
}
void timezone_names_and_bias() {
    SourceColdTimeState state; Fixture f; SourceTimeZoneInformation zone;
    zone.bias=60; zone.standard_bias=30; zone.standard_date.month=10;
    zone.daylight_bias=-60; zone.daylight_date.month=4;
    f.zones.push_back({2,zone,{}}); f.conversions={{2,true},{0,false}};
    state.standard_name[63]='S'; state.daylight_name[63]='D';
    initialize_source_timezone_469b1f(state,f);
    require(state.timezone_seconds==5400&&state.dst_bias_seconds==0xffffeae8u&&state.daylight==1,"bias difference arithmetic");
    require(state.standard_name[0]==0&&state.daylight_name[0]==0&&state.standard_name[1]=='Y',"failure/default byte0 handling");
    require(state.standard_name[63]=='S'&&state.daylight_name[63]=='D',"failure cleared untouched name tails");
    f.zones.push_back({0,SourceTimeZoneInformation{}, {}}); f.conversions.clear(); f.conversion_index=0;
    initialize_source_timezone_469b1f(state,f);
    require(state.standard_name[63]==0&&state.daylight_name[63]==0&&state.standard_name[0]=='X',"success termination");
    require(state.daylight==0&&state.dst_bias_seconds==0,"no daylight rules must clear daylight/bias");
}
void tz_parser_and_retained_bias() {
    struct Case { const char* text; Word offset; Word daylight; const char* name; };
    for(const auto test:std::array<Case,6>{{{"UTC0",0,0,""},{"CET-1CEST",0xfffff1f0u,67,"CES"},
        {"PST8:30:15PDT",30615,80,"PDT"},{"ABC+2:05:09XYZ",7509,88,"XYZ"},
        {"ABC +2XYZ",7200,32," +2"},{"ABC4294967297X",3600,88,"X"}}}) {
        SourceColdTimeState state; Fixture f; f.tz=test.text;
        initialize_source_timezone_469b1f(state,f);
        require(state.timezone_seconds==test.offset&&state.daylight==test.daylight,"source TZ parser/byte flag");
        require(state.dst_bias_seconds==0xfffff1f0u&&std::string(state.daylight_name.data())==test.name,"TZ retained bias/name");
        require(state.cached_tz&&std::strcmp(state.cached_tz.get(),test.text)==0&&f.calls=="EA","TZ copy/OS separation");
    }
}
void tz_empty_same_and_allocation_failure() {
    SourceColdTimeState state; Fixture f; f.tz="";
    ensure_source_timezone_469b0a(state,f);
    require(state.timezone_initialized==1&&state.timezone_seconds==28800&&f.calls=="E","empty TZ retention");
    f.tz="UTC0"; initialize_source_timezone_469b1f(state,f);
    state.start.year=3; state.end.year=4; state.start.day=7; state.end.day=8;
    const char* identity=state.cached_tz.get(); f.calls.clear();
    initialize_source_timezone_469b1f(state,f);
    require(f.calls=="E"&&identity==state.cached_tz.get(),"same TZ reallocated");
    require(state.start.year==0xffffffffu&&state.end.year==0xffffffffu&&state.start.day==7&&state.end.day==8,"same TZ entry write set");
    f.tz="CET-1CEST"; f.allocation_fails=true; f.calls.clear(); state.timezone_initialized=0;
    ensure_source_timezone_469b0a(state,f);
    require(!state.cached_tz&&state.timezone_seconds==0&&state.timezone_initialized==1&&f.calls=="EA","allocation null/old release/once");
    f.allocation_fails=false; f.tz="AB";
    expect_boundary([&]{initialize_source_timezone_469b1f(state,f);},0x469cb7u);
    require(state.cached_tz&&std::string(state.standard_name.data())=="AB","short TZ rejection lost earlier writes");
}
void dst_normal_dates_rules_and_unwritten_read() {
    SourceColdTimeState state; prepare_utc(state); state.daylight=1; Fixture f;
    require(convert_source_local_time_468374(state,f,{2024,6,0,15,0,0,0,0},0xffffffffu)==0x666ccb70u,"legacy US summer DST");
    require(state.start.day==97&&state.end.day==300&&state.start.milliseconds==7200000&&state.end.milliseconds==3600000,"April/October transition table");
    require(convert_source_local_time_468374(state,f,{2024,1,0,15,0,0,0,0},0xffffffffu)==0x65a47580u,"legacy US winter DST");
    state.have_os_rules=1; state.start.year=state.end.year=0xffffffffu;
    state.retained_zone.daylight_date={0,3,0,5,2,0,0,0};
    state.retained_zone.standard_date={0,10,0,5,1,0,0,0};
    state.dst_bias_seconds=0xffffe3e0u; //-7200, previous-day normalization
    require(convert_source_local_time_468374(state,f,{2024,6,0,15,0,0,0,0},0xffffffffu)==0x666cbd60u,"OS relative DST");
    require(state.start.day==90&&state.end.day==299&&state.end.milliseconds==82800000u,"fifth weekday/negative end normalization");
    state.start.year=state.end.year=0xffffffffu; state.dst_bias_seconds=90000;
    (void)convert_source_local_time_468374(state,f,{2024,6,0,15,0,0,0,0},0xffffffffu);
    require(state.end.day==301&&state.end.milliseconds==7200000u,"positive end normalization");
    state.dst_bias_seconds=0xfffff1f0u;
    // OS absolute dates choose explicit mode, but use local year, not TZI year.
    state.have_os_rules=1; state.start.year=state.end.year=0xffffffffu;
    state.retained_zone.daylight_date={2001,10,6,1,2,0,0,0};
    state.retained_zone.standard_date={2001,4,6,1,2,0,0,0};
    require(convert_source_local_time_468374(state,f,{2024,1,0,15,0,0,0,0},0xffffffffu)==0x65a46770u,"southern DST");
    require(state.start.year==124&&state.start.day==274&&state.end.day==91,"absolute rules/local year");
    // Reached undefined original stack read: preserve caches and RNG, no invented minute.
    state.have_os_rules=0; state.start.year=state.end.year=0xffffffffu;
    f.local={2024,4,0,7,2,0,0,0}; f.utc=f.local; f.zones.push_back({0xffffffffu,{}, {}});
    VehicleRandom random; random.state=0x12345678u;
    expect_boundary([&]{initialize_source_random_time_42f1a0(state,random,f);},0x469efbu);
    require(random.state==0x12345678u&&state.cached_dst==0xffffffffu&&state.cached_utc==f.utc,"rejection seeded or rolled back UTC");
    require(state.start.year==124&&state.end.year==124&&state.start.day==97,"rejection rolled back transitions");
    Word output=0x98765432u;
    expect_boundary([&]{(void)read_source_time_4632a7(state,f,&output);},0x469efbu);
    require(output==0x98765432u,"rejected child wrote caller output");
}
void seed_same_owner_including_error_sentinel() {
    SourceColdTimeState state; Fixture f; f.local.year=2039;
    f.zones.push_back({0,{}, {}});
    VehicleRandom random,other; random.state=12; other.state=987;
    initialize_source_random_time_42f1a0(state,random,f);
    require(random.state==0xffffffffu&&other.state==987,"time error sentinel must seed SAME supplied RNG");
    require(state.cached_utc==f.utc&&state.timezone_initialized==0&&f.calls=="LUZ","invalid year outer-cache effect");
    (void)random.signed_value(1.0f);
    require(random.state==0x00235ac6u,"existing RNG first state after sentinel");
    prepare_utc(state); f.local={1970,1,0,1,0,0,0,0};
    initialize_source_random_time_42f1a0(state,random,f);
    require(random.state==0,"epoch seed is zero, not an implicit default seed");
}
}
int main() {
    try {
        initial_words(); dates_and_signed_wrap(); cache_and_call_order(); outer_dst_status_gate();
        errors_retained_writes_and_once(); timezone_names_and_bias(); tz_parser_and_retained_bias();
        tz_empty_same_and_allocation_failure(); dst_normal_dates_rules_and_unwritten_read();
        seed_same_owner_including_error_sentinel();
        std::cout<<"10 source cold-time receiving groups passed\n";
        return 0;
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
