#include "sgn/vehicle_tick_routes.hpp"
#include "vehicle_ai_automatic_state.hpp"
#include <map>
#include "vehicle_tick_state.hpp"
#include "vehicle_restart_state.hpp"
#include "contact_solver_state.hpp"
#include <bit>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>

namespace {
using namespace sgn::retail;
using Record=std::vector<std::uint8_t>;
constexpr unsigned size=0x30000,cs=0x7800,ca=0x2d000,ctx=0x2f000;
std::uint32_t u(const Record& b,unsigned at){std::uint32_t x;std::memcpy(&x,b.data()+at,4);return x;}
void put(Record& b,unsigned at,std::uint32_t x){std::memcpy(b.data()+at,&x,4);}
float f(const Record& b,unsigned at){return std::bit_cast<float>(u(b,at));}
std::int32_t i(const Record& b,unsigned at){return std::bit_cast<std::int32_t>(u(b,at));}
void apply(Record& b,unsigned entry,SplineSet& spline_set) {
    const auto current=u(b,ctx);
    if(current>=6 || u(b,ctx+0x38)>=4)throw std::runtime_error("vehicle tick fixture identity");
    const char* text=reinterpret_cast<const char*>(b.data()+ctx+0x40);
    if(std::memchr(text,0,64)==nullptr)throw std::runtime_error("vehicle tick notice terminator");
    auto parameters=std::make_unique<std::array<VehicleParameters,6>>();
    auto physics=std::make_unique<std::array<VehicleState,6>>();
    auto contacts=std::make_unique<std::array<contact::solver::State,6>>();
    std::array<VehicleRaceState,6> races{};
    std::array<VehicleRestartState,6> restarts{};
    std::array<VehicleTickState,6> states{};
    std::array<VehicleRouteAssistState,6> assists{};
    std::array<AiAutomaticMemory,6> ai{};
    std::array<VehicleAudioState,6> audio{};
    auto transfer=[&](bool read) {for(unsigned j=0;j<6;++j) {
        auto bytes=std::span(b).subspan(j*cs,cs);
        contact::oracle::transfer(bytes,(*parameters)[j],(*physics)[j],races[j],(*contacts)[j],read);
        restart_oracle::transfer(bytes,restarts[j],read);tick_oracle::transfer(bytes,states[j],read);
        automatic_oracle::transfer(bytes,ai[j],read);
        if(read){assists[j].blend=f(b,j*cs+0x265c);audio[j].field_771c=u(b,j*cs+0x771c);audio[j].field_7720=u(b,j*cs+0x7720);}
        else put(b,j*cs+0x265c,std::bit_cast<std::uint32_t>(assists[j].blend));
        // The older frozen race fixture predates the ranking consumer and
        // deliberately does not transfer24AC. Its runtime owner is still race.
        if(read)races[j].place=u(b,j*cs+0x24ac);else put(b,j*cs+0x24ac,races[j].place);
    }};
    transfer(true);
    contact::player::Controls controls;controls.current=u(b,ctx+0x38);
    auto controllers=[&](bool read) {for(unsigned j=0;j<4;++j) {
        const auto at=ca+j*0x74c;auto& c=controls.controllers[j];
        if(read) {
            c.active=i(b,at);c.vehicle=u(b,at+0xac);
            std::memcpy(c.notices.data(),b.data()+at+0x4ac,sizeof(c.notices));
        }else {
            put(b,at,std::bit_cast<std::uint32_t>(c.active));put(b,at+0xac,c.vehicle);
            std::memcpy(b.data()+at+0x4ac,c.notices.data(),sizeof(c.notices));
        }
    }};
    controllers(true);
    for(const auto& c:controls.controllers) {
        if(c.vehicle>=6)throw std::runtime_error("vehicle tick controller vehicle identity");
        for(const auto& notice:c.notices)if(notice.source_vehicle>=6)throw std::runtime_error("vehicle tick notice vehicle identity");
    }
    auto& physical=(*physics)[current];const auto& parameter=(*parameters)[current];
    AiAutomaticGlobals globals;globals.initialized=u(b,ctx+0x120);
    std::array<SplineView,3> views;
    for(unsigned j=0;j<3;++j) {
        const auto view=spline_set.find(u(b,ctx+0x100+j*4));
        if(!view)throw std::runtime_error("vehicle route fixture spline ID");
        views[j]=*view;globals.route_distances[j]=f(b,ctx+0x110+j*4);
    }
    globals.routes=views;
    globals.target_speed=f(b,ctx+0x124);globals.response_scale=f(b,ctx+0x128);globals.braking_allowed=u(b,ctx+0x12c);
    globals.target={f(b,ctx+0x130),f(b,ctx+0x134),f(b,ctx+0x138)};
    VehicleRouteAssistSettings settings;settings.phase=u(b,ctx+8);
    settings.unit_gravity={f(b,ctx+0x18),f(b,ctx+0x1c),f(b,ctx+0x20)};settings.upgraded_skill_threshold=f(b,ctx+0x28);
    for(unsigned j=0;j<4;++j){settings.steering_gain[j]=f(b,ctx+0x140+j*4);settings.bend_speed[j]=f(b,ctx+0x150+j*4);}
    settings.mass_override_gain=f(b,ctx+0x160);
    const VehicleRouteAssistContext context{globals,ai[current],audio[current],f(b,current*cs+0x1b8c),settings};
    const auto ticks=u(b,ctx+0x3c);
    if(ticks==0 || ticks>1024)throw std::runtime_error("vehicle route tick count");
    const sgn::Vec3 target{f(b,ctx+0x170),f(b,ctx+0x174),f(b,ctx+0x178)};const float gain=f(b,ctx+0x24);
    for(unsigned tick=0;tick<ticks;++tick) {
        if(entry==0x405be0)limit_vehicle_route_speed(parameter,physical,gain);
        else if(entry==0x4054b0)steer_vehicle_route_target(parameter,physical,ai[current],target,gain,settings);
        else if(entry==0x406020)steer_vehicle_override_target(parameter,physical,target,gain,settings.unit_gravity);
        else if(entry==0x405610)guide_vehicle_route(parameter,physical,assists[current],context,gain);
        else if(entry==0x406140)guide_vehicle_override(parameter,physical,context,gain);
        else if(entry==0x405c80)update_vehicle_route_braking(parameter,physical,races[current],assists[current],context);
        else if(entry==0x406190)update_vehicle_route_guidance(parameter,physical,races[current],assists[current],context);
        else throw std::runtime_error("unknown vehicle route entry");
    }
    for(unsigned j=0;j<3;++j)put(b,ctx+0x110+j*4,std::bit_cast<std::uint32_t>(globals.route_distances[j]));
    transfer(false);controllers(false);put(b,ctx+0x38,controls.current);
}
void read(std::ifstream& in,void* out,std::size_t n){if(!in.read(static_cast<char*>(out),static_cast<std::streamsize>(n)))throw std::runtime_error("short vehicle tick fixture");}
unsigned word(std::ifstream& in){unsigned x;read(in,&x,4);return x;}
}
int main(int argc,char** argv) {try {
    if(argc!=3)throw std::runtime_error("expected vehicle tick corpus");
    std::ifstream in(argv[1],std::ios::binary);char magic[8];read(in,magic,8);
    if(std::string(magic,8)!="SGNRT001")throw std::runtime_error("vehicle tick corpus schema");
    const auto count=word(in);if(count==0 || count>20000)throw std::runtime_error("vehicle tick corpus count");
    std::uint64_t exact=0,errors=0,ticks=0;
    std::map<std::string,std::unique_ptr<SplineSet>> models;
    for(unsigned n=0;n<count;++n) {
        const auto entry=word(in);
        Record actual(size),expected(size);read(in,actual.data(),size);read(in,expected.data(),size);
        ticks+=u(actual,ctx+0x3c);
        const char* raw_name=reinterpret_cast<const char*>(actual.data()+ctx+0x80);
        const void* stop=std::memchr(raw_name,0,64);
        if(stop==nullptr)throw std::runtime_error("route model name terminator");
        const std::string name(raw_name,static_cast<const char*>(stop));
        if(name.empty() || name.find_first_of("/\\\\:")!=std::string::npos || name.find("..")!=std::string::npos)throw std::runtime_error("route model name");
        if(!models.contains(name)) {
            std::ifstream model(std::filesystem::path(argv[2])/name,std::ios::binary|std::ios::ate);
            const auto length=model.tellg();if(length<0 || length>1024ll*1024*1024)throw std::runtime_error("route model size");
            sgn::Bytes bytes(static_cast<std::size_t>(length));model.seekg(0);
            if(!model.read(reinterpret_cast<char*>(bytes.data()),length))throw std::runtime_error("route model read");
            models.emplace(name,std::make_unique<SplineSet>(std::move(bytes)));
        }
        apply(actual,entry,*models.at(name));
        for(unsigned at=0;at<size;at+=4) {
            if(u(actual,at)==u(expected,at)){++exact;continue;}
            if(errors++<24)std::cerr<<"case "<<n<<" entry "<<std::hex<<entry<<" offset "<<at<<" expected "<<u(expected,at)<<" actual "<<u(actual,at)<<std::dec<<'\n';
        }
    }
    if(in.peek()!=std::char_traits<char>::eof())throw std::runtime_error("trailing vehicle tick corpus");
    std::cout<<"{\"cases\":"<<count<<",\"ticks\":"<<ticks<<",\"exact_words\":"<<exact<<",\"errors\":"<<errors<<",\"allowed_ulp\":0,\"excluded_words\":0}\n";
    return errors?1:0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
