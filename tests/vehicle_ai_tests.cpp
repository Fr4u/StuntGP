#include "sgn/vehicle_ai.hpp"
#include "sgn/clock.hpp"
#include <bit>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
using namespace sgn::retail;
void require(bool condition,const char* message) { if(!condition) throw std::runtime_error(message); }
AiField racers() {
    AiField field{};
    for(std::size_t i=0;i<6;++i) {
        auto& s=field[i];s.active=1;s.route_progress=float(i*10);s.speed=70;
        s.right={1,0,0};s.forward={0,0,1};s.route_right={1,0,0};
        s.position={0,0,float(i*1000)};s.passing_target=(i+1)%6;
    }
    return field;
}
struct TacticFixture {
    AiField field=racers();AiTactics tactics{};
    std::vector<std::uint32_t> trace;
    TacticFixture() {
        for(auto& s:field) {s.gear=2;s.max_steering=40;s.up_y=1;s.driving_factor=1;}
        tactics.world_up={0,1,0};tactics.course_length=tactics.spline_length=50000;
    }
    void tick(std::uint64_t tick_index) {
        // Synthetic changing observations test scheduler independence. This
        // fixture is not a reconstruction of race initialization or motion.
        for(std::size_t i=0;i<6;++i) {
            auto& s=field[i];s.position.z=float(tick_index*4+i*500);
            s.position.x=float(int((tick_index+i*17)%400)-200);
            s.route_progress=float((tick_index+i*5)%1000)*0.1f;
            s.speed=60+float((tick_index+i*3)%20);s.contact_class=(tick_index%80<2)?1:0;
        }
        tactics.target=field[0].position;tactics.target.z+=2000;
        tactics.target_speed=100;tactics.response_scale=1;
        ai_follow_rival(field,0,tactics);ai_pass_rival(field,0,tactics);
        ai_yield_to_rival(field,0,tactics);ai_steer_to_target(field,0,tactics);
        ai_recover_stalled(field,0);
        for(float x:{tactics.target.x,tactics.target.y,tactics.target.z,tactics.target_speed,
            tactics.response_scale,field[0].steering,field[0].throttle,field[0].passing_offset})
            trace.push_back(std::bit_cast<std::uint32_t>(x));
        for(auto n:{field[0].following_ticks,field[0].passing_ticks,field[0].reverse_ticks,field[0].gear})
            trace.push_back(std::bit_cast<std::uint32_t>(n));
        trace.push_back(field[0].passing_target?static_cast<std::uint32_t>(*field[0].passing_target):0xffffffffu);
    }
};
}
int main() {
    try {
        auto field=racers();
        require(ai_select_ahead(field,0)==1,"ahead selector must use next eligible progress");
        require(ai_select_ahead(field,5)==0,"ahead selector must wrap");
        require(ai_select_behind(field,0)==5,"behind selector must wrap");
        field[1].active=2;require(ai_select_ahead(field,0)==2,"active must equal exactly one");
        field[2].route_progress=30;require(ai_select_ahead(field,0)==2,"first slot must win ties");
        for(auto& s:field)s.active=0;
        require(!ai_select_ahead(field,0) && !ai_select_behind(field,0),"empty opponent set must return no slot");
        bool rejected=false;AiTactics a{};
        try {ai_pass_rival(field,0,a);}catch(const std::domain_error&){rejected=true;}
        require(rejected,"missing rival must fail explicitly");
        field=racers();field[1].route_progress=std::numeric_limits<float>::quiet_NaN();rejected=false;
        try {(void)ai_select_ahead(field,0);}catch(const std::domain_error&){rejected=true;}
        require(rejected,"non-finite progress must fail explicitly");
        field=racers();field[0].contact_class=1;field[0].passing_ticks=40;field[0].following_ticks=3;
        ai_pass_rival(field,0,a);require(field[0].passing_ticks==40,"nonzero contact class must leave passing state alone");
        ai_follow_rival(field,0,a);require(field[0].following_ticks==2,"following countdown occurs before contact gate");
        field=racers();field[0].passing_offset=-123;field[1].position.z=499;
        ai_choose_passing_side(field,0);require(field[0].passing_offset==-123,"near rival preserves prior passing side");
        field[1].position.z=500;ai_choose_passing_side(field,0);require(field[0].passing_offset==500,"500-unit side selection boundary");
        field=racers();field[0].speed=0;field[0].throttle=1;field[0].gear=2;field[0].reverse_ticks=299;
        ai_recover_stalled(field,0);require(field[0].gear==2 && field[0].reverse_ticks==300,"reverse must wait through tick 300");
        ai_recover_stalled(field,0);require(field[0].gear==0 && field[0].reverse_ticks==0,"reverse must start on tick 301");
        TacticFixture reference;for(std::uint64_t tick=0;tick<600;++tick)reference.tick(tick);
        unsigned schedules=0;
        auto check_schedule=[&](std::int64_t numerator,std::int64_t denominator) {
            TacticFixture fixture;sgn::TickClock clock;std::int64_t previous=0;
            for(std::int64_t frame=1;frame<=10*numerator/denominator;++frame) {
                const auto now=frame*1'000'000'000*denominator/numerator;
                clock.advance(std::chrono::nanoseconds(now-previous),[&](auto tick){fixture.tick(tick);});previous=now;
            }
            clock.advance(std::chrono::nanoseconds(10'000'000'000-previous),[&](auto tick){fixture.tick(tick);});
            require(clock.ticks()==600 && fixture.trace==reference.trace,"display schedule changed tactical AI state");++schedules;
        };
        for(auto hz:{24,30,50,59,60,75,90,100,120,144,165,240,360,500,1000,2000})check_schedule(hz,1);
        for(auto numerator:{24000,30000,60000,120000,144000})check_schedule(numerator,1001);
        std::cout<<"AI selection, boundaries, countdown and invalid-input contracts passed; "<<schedules
                 <<" display schedules produced bit-identical 600-tick tactical traces\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
