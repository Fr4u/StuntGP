#include "sgn/vehicle_tick_input.hpp"
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
void apply(Record& b,unsigned entry) {
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
    auto transfer=[&](bool read) {for(unsigned j=0;j<6;++j) {
        auto bytes=std::span(b).subspan(j*cs,cs);
        contact::oracle::transfer(bytes,(*parameters)[j],(*physics)[j],races[j],(*contacts)[j],read);
        restart_oracle::transfer(bytes,restarts[j],read);tick_oracle::transfer(bytes,states[j],read);
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
    std::array<std::string_view,6> names{};
    for(unsigned j=0;j<6;++j) {
        const char* name=reinterpret_cast<const char*>(b.data()+ctx+0x100+j*100);
        const void* stop=std::memchr(name,0,100);
        if(stop==nullptr)throw std::runtime_error("vehicle input name terminator");
        names[j]=std::string_view(name,static_cast<const char*>(stop)-name);
    }
    const VehicleTickInputContext context{u(b,ctx+0x10),i(b,ctx+8),
        {f(b,ctx+0x18),f(b,ctx+0x1c),f(b,ctx+0x20)},controls,current,names,text};
    const auto ticks=u(b,ctx+0x3c);
    if(ticks==0 || ticks>1024)throw std::runtime_error("vehicle input tick count");
    for(unsigned tick=0;tick<ticks;++tick) {
    if(entry==0x40c500)analog_steering_input(parameter,physical,races[current]);
    else if(entry==0x40c730)analog_vehicle_input(parameter,physical,races[current],context);
    else if(entry==0x40c710)mixed_vehicle_input(parameter,physical,context);
    else if(entry==0x40d160)digital_vehicle_input(parameter,physical,context);
    else if(entry==0x40d6b0)update_low_energy_notice(physical,states[current],context);
    else if(entry==0x40d790)dispatch_vehicle_input(parameter,physical,races[current],states[current],context);
    else throw std::runtime_error("unknown vehicle input entry");
    }
    transfer(false);controllers(false);put(b,ctx+0x38,controls.current);
}
void read(std::ifstream& in,void* out,std::size_t n){if(!in.read(static_cast<char*>(out),static_cast<std::streamsize>(n)))throw std::runtime_error("short vehicle tick fixture");}
unsigned word(std::ifstream& in){unsigned x;read(in,&x,4);return x;}
}
int main(int argc,char** argv) {try {
    if(argc!=2)throw std::runtime_error("expected vehicle tick corpus");
    std::ifstream in(argv[1],std::ios::binary);char magic[8];read(in,magic,8);
    if(std::string(magic,8)!="SGNIN002")throw std::runtime_error("vehicle tick corpus schema");
    const auto count=word(in);if(count==0 || count>20000)throw std::runtime_error("vehicle tick corpus count");
    std::uint64_t exact=0,errors=0,ticks=0;
    for(unsigned n=0;n<count;++n) {
        const auto entry=word(in);
        Record actual(size),expected(size);read(in,actual.data(),size);read(in,expected.data(),size);
        ticks+=u(actual,ctx+0x3c);
        apply(actual,entry);
        for(unsigned at=0;at<size;at+=4) {
            if(u(actual,at)==u(expected,at)){++exact;continue;}
            if(errors++<24)std::cerr<<"case "<<n<<" entry "<<std::hex<<entry<<" offset "<<at<<" expected "<<u(expected,at)<<" actual "<<u(actual,at)<<std::dec<<'\n';
        }
    }
    if(in.peek()!=std::char_traits<char>::eof())throw std::runtime_error("trailing vehicle tick corpus");
    std::cout<<"{\"cases\":"<<count<<",\"ticks\":"<<ticks<<",\"exact_words\":"<<exact<<",\"errors\":"<<errors<<",\"allowed_ulp\":0,\"excluded_words\":0}\n";
    return errors?1:0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
