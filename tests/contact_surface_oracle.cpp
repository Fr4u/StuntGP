#include "sgn/contact_surface.hpp"
#include <bit>
#include <cstring>
#include <fstream>
#include <iostream>
namespace {
using namespace sgn::retail::contact;
using Record=std::array<std::uint8_t,0x2200>;
std::uint32_t u(const Record& b,unsigned at){std::uint32_t v;std::memcpy(&v,b.data()+at,4);return v;}
void u(Record& b,unsigned at,std::uint32_t v){std::memcpy(b.data()+at,&v,4);}
float f(const Record& b,unsigned at){return std::bit_cast<float>(u(b,at));}
void apply(Record& b,unsigned entry){
    player::Controller controller{};surface::State state{};sgn::retail::VehicleState vehicle{};sgn::retail::VehicleRaceState race{};
    const auto selected=u(b,0x80);if(selected>=4)throw std::runtime_error("surface controller index");
    const auto at=0x100+selected*0x74c;
    auto transfer=[&](bool reading){
        auto word=[&](std::uint32_t& v,unsigned a){if(reading)v=u(b,a);else u(b,a,v);};
        auto integer=[&](std::int32_t& v,unsigned a){if(reading)v=std::bit_cast<std::int32_t>(u(b,a));else u(b,a,std::bit_cast<std::uint32_t>(v));};
        auto vector=[&](auto& v,unsigned a){if(reading){v.x=f(b,a);v.y=f(b,a+4);v.z=f(b,a+8);}else{u(b,a,std::bit_cast<std::uint32_t>(v.x));u(b,a+4,std::bit_cast<std::uint32_t>(v.y));u(b,a+8,std::bit_cast<std::uint32_t>(v.z));}};
        word(controller.camera.mode,at+4);vector(controller.camera.position,at+8);vector(controller.camera.velocity,at+0xc4);vector(controller.camera.angular_velocity,at+0xd8);
        integer(vehicle.mass_override,0x1f10);vector(vehicle.position,0x1f00);word(state.saved_controller_mode,0x1f14);word(race.opposing_direction_ticks,0x1f24);word(race.unsafe_contact_ticks,0x1f28);word(race.respawn_requested,0x1f2c);vector(race.respawn_heading,0x1f30);
    };
    transfer(true);if(u(b,0x1f18))state.object_mode=std::bit_cast<std::int32_t>(u(b,0x1f1c));
    switch(entry){
    case 0x408e90:u(b,0x1f20,surface::attach_controller(controller,state,vehicle));break;
    case 0x408f90:surface::detach_controller(controller,state,vehicle);break;
    case 0x418510:surface::check_direction(race,{f(b,0x1f40),f(b,0x1f44),f(b,0x1f48)},std::bit_cast<std::int32_t>(u(b,0x1f50)));break;
    default:throw std::runtime_error("surface entry");
    }
    transfer(false);
}
void read(std::ifstream& in,void* p,std::size_t n){if(!in.read(static_cast<char*>(p),static_cast<std::streamsize>(n)))throw std::runtime_error("short surface corpus");}
std::uint32_t read_u(std::ifstream& in){std::uint32_t v;read(in,&v,4);return v;}
}
int main(int argc,char** argv){
    try{
        if(argc!=2)throw std::runtime_error("expected surface corpus");std::ifstream in(argv[1],std::ios::binary);char magic[8];read(in,magic,8);
        if(std::string(magic,8)!="SGNCS001")throw std::runtime_error("surface corpus header");auto count=read_u(in);if(count>10000)throw std::runtime_error("surface corpus count");
        unsigned errors=0;std::uint64_t exact=0;
        for(unsigned n=0;n<count;++n){auto entry=read_u(in);Record actual{},expected{};read(in,actual.data(),actual.size());read(in,expected.data(),expected.size());apply(actual,entry);
            for(unsigned at=0;at<actual.size();at+=4){if(u(actual,at)==u(expected,at)){++exact;continue;}
                if(errors++<24)std::cerr<<"case "<<n<<" entry "<<std::hex<<entry<<" offset "<<at<<" expected "<<u(expected,at)<<" actual "<<u(actual,at)<<std::dec<<'\n';}
        }
        if(in.peek()!=std::char_traits<char>::eof())throw std::runtime_error("trailing surface corpus");
        std::cout<<"{\"cases\":"<<count<<",\"exact_words\":"<<exact<<",\"errors\":"<<errors<<",\"allowed_ulp\":0}\n";return errors?1:0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
