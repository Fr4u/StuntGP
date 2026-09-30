#include "sgn/vehicle.hpp"
#include "sgn/clock.hpp"
#include <bit>
#include <cmath>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

namespace {
using namespace sgn;
using namespace sgn::retail;
void expect(bool yes,const char* why){if(!yes)throw std::runtime_error(why);}
void rejects(const std::function<void()>& f){bool rejected=false;try{f();}catch(const FormatError&){rejected=true;}expect(rejected,"malformed vehicle config accepted");}
std::string replaced(std::string s,const std::string& a,const std::string& b){auto pos=s.find(a);expect(pos!=s.npos,"fixture text anchor absent");s.replace(pos,a.size(),b);return s;}
struct EngineFixture {
    // Deterministic bench inputs. This is not a retail race/spawn initializer.
    VehicleParameters p;VehicleState s;EngineContext context{0,true,0,0};std::vector<std::uint32_t> trace;
    explicit EngineFixture(VehicleParameters source):p(std::move(source)) {
        s.rpm=p.rpm_idle;s.gear=1;s.clutch=1;s.torque=250;s.energy=s.energy_capacity=100;
        s.fuel=p.fuel_capacity;s.effective_mass=p.mass;s.torque_curve=0;
        for(auto& w:s.wheels)w.contact_countdown=8;
        build_engine_curve(p,s);
    }
    void step(std::uint64_t tick) {
        s.speed=static_cast<float>(tick%180)*0.3f;s.throttle=(tick/120)%3==2?0.0f:1.0f;
        s.turbo_active=(tick%200)>150?1:0;
        for(auto& w:s.wheels){w.speed=s.speed;w.acceleration=0;}
        engine_tick(p,s,context);
        auto f=[&](float v){expect(std::isfinite(v),"non-finite engine trajectory");trace.push_back(std::bit_cast<std::uint32_t>(v));};
        for(float v:{s.rpm,s.torque,s.engine_output,s.clutch,s.turbo_rpm,s.turbo_force,s.energy,s.fuel})f(v);
        for(const auto& w:s.wheels)f(w.acceleration);
        trace.push_back(static_cast<std::uint32_t>(s.gear));trace.push_back(static_cast<std::uint32_t>(s.downshift_ticks));
        trace.push_back(static_cast<std::uint32_t>(s.gear_changed));trace.push_back(static_cast<std::uint32_t>(s.turbo_active));
    }
};
}
int main(int argc,char** argv) {
    try {
        if(argc!=2)throw std::runtime_error("usage: vehicle_tests original-assets-root");
        AssetStore assets(argv[1]);auto raw=assets.resource("config/vehicles/car16.cfg");
        const std::string config(reinterpret_cast<const char*>(raw.data()),raw.size());auto p=decode_vehicle_parameters(config);
        rejects([&]{decode_vehicle_parameters(replaced(config," Mass            = 1250.000000"," Mass            = 0"));});
        rejects([&]{decode_vehicle_parameters(replaced(config," ShiftTime       = 0.500000"," ShiftTime       = 0"));});
        rejects([&]{decode_vehicle_parameters(replaced(config," RpmMax          = 12250.000000"," RpmMax          = nan"));});
        rejects([&]{decode_vehicle_parameters(replaced(config," DriveMode       = 0"," DriveMode       = 9"));});
        rejects([&]{decode_vehicle_parameters(replaced(config," Mass            = 1250.000000"," Mass = 1250\n Mass = 1250"));});
        rejects([&]{decode_vehicle_parameters(config.substr(0,config.find("-GEARBOX")));});
        rejects([&]{decode_vehicle_parameters(replaced(config,"0.275000, 0.287999,","0.275000,"));});
        rejects([&]{decode_vehicle_parameters(config+"\n-DIMENSIONS[] =\n");});
        for(const auto* extra:{"0","-1","+.5","-.5","1e-3","+1e-3"})
            rejects([&]{decode_vehicle_parameters(replaced(config," AnalogSteeringInputEnv =",std::string(extra)+",\n AnalogSteeringInputEnv ="));});
        for(const auto* invalid:{"0","-1"}) {
            rejects([&]{decode_vehicle_parameters(replaced(config," TrackFront      = 126.599998",std::string(" TrackFront = ")+invalid));});
            rejects([&]{decode_vehicle_parameters(replaced(config," TrackRear       = 180.000000",std::string(" TrackRear = ")+invalid));});
        }
        for(auto indices:{std::array<std::int32_t,2>{65,0},{0,65},{-1,0},{0,-1}}) {
            VehicleState state;state.stunt.phase_indices[0]=indices[0];state.stunt.phase_indices[1]=indices[1];
            state.stunt.history_and_balance.fill(7);const auto before=state.stunt.history_and_balance;
            bool rejected=false;
            try{update_phase_history(state,{0,-1,0});}catch(const std::domain_error&){rejected=true;}
            expect(rejected && state.stunt.history_and_balance==before,"bad stunt history mutated state");
            if(indices[0]>64 || indices[1]>64) {
                rejected=false;try{save_phase_history(state);}catch(const std::domain_error&){rejected=true;}
                expect(rejected && state.stunt.history_and_balance==before,"bad stunt save mutated state");
            }
        }
        EngineFixture reference(p);for(std::uint64_t tick=0;tick<600;++tick)reference.step(tick);
        unsigned schedules=0;
        auto verify_schedule=[&](std::int64_t numerator,std::int64_t denominator) {
            EngineFixture fixture(p);TickClock clock;std::int64_t previous=0;
            for(std::int64_t frame=1;frame<=10*numerator/denominator;++frame){auto now=frame*1'000'000'000*denominator/numerator;
                clock.advance(std::chrono::nanoseconds(now-previous),[&](auto tick){fixture.step(tick);});previous=now;}
            clock.advance(std::chrono::nanoseconds(10'000'000'000-previous),[&](auto tick){fixture.step(tick);});
            expect(clock.ticks()==600 && fixture.trace==reference.trace,"display cadence changed real engine state");++schedules;
        };
        for(auto hz:{24,30,50,59,60,75,90,100,120,144,165,240,360,500,1000})verify_schedule(hz,1);
        for(auto numerator:{24000,30000,60000,120000,144000})verify_schedule(numerator,1001);
        std::cout<<"vehicle config fault cases=18; actual engine trajectories bit-identical across "<<schedules<<" presentation schedules; 600 ticks each\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
