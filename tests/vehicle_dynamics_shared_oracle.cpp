#include "sgn/vehicle_dynamics_shared.hpp"
#include "sgn/retail_replay_dynamics.hpp"
#include "vehicle_oracle_state.hpp"
#include <algorithm>
#include <bit>
#include <cstring>
#include <fstream>
#include <iostream>

namespace {
using namespace sgn::retail;
constexpr unsigned car_size=0x7800,context=2*car_size,record_size=context+0x100;
using Record=std::array<std::uint8_t,record_size>;
unsigned u(const Record& b,unsigned at){unsigned value;std::memcpy(&value,b.data()+at,4);return value;}
float f(const Record& b,unsigned at){return std::bit_cast<float>(u(b,at));}
void put(Record& b,unsigned at,unsigned value){std::memcpy(b.data()+at,&value,4);}
VehicleVector vector(const Record& b,unsigned at){return {f(b,at),f(b,at+4),f(b,at+8),f(b,at+12)};}
void apply(Record& b) {
    const auto mode=u(b,context+0x40);if(mode>1)throw std::runtime_error("unknown shared dynamics sequence");
    auto first=std::span(b).first(car_size),second=std::span(b).subspan(car_size,car_size);
    VehicleParameters p,ghost_p;VehicleState s,ghost;oracle::transfer(first,p,s,true);oracle::transfer(second,ghost_p,ghost,true);
    VehicleSupport support;support.body_position=vector(b,0x7270);
    for(unsigned j=0;j<4;++j){support.local[j]=vector(b,0x73b4+j*0x4c);support.current[j]=vector(b,0x73c4+j*0x4c);}
    const EngineContext engine{std::bit_cast<std::int32_t>(u(b,0x2a20)),u(b,0x2a24)!=0,f(b,0x2a28),f(b,0x2a2c)};
    const VehicleDynamicsContext dynamics{engine,f(b,0x2a08),f(b,0x2a44),{f(b,0x2a38),f(b,0x2a3c),f(b,0x2a40)},0,u(b,0x2a34)};
    VehicleRandom random{u(b,0x2a30)};VehicleDynamicsWorkspace workspace;
    for(unsigned j=0;j<16;++j)workspace.steering[j]=f(b,context+j*4);
    auto legacy_state=s;auto legacy_parameters=p;auto legacy_random=random;
    vehicle_dynamics_core(p,s,support,dynamics,random,workspace);
    vehicle_dynamics_core(legacy_parameters,legacy_state,support,dynamics,legacy_random);
    std::array<std::uint8_t,car_size> legacy{};std::copy(first.begin(),first.end(),legacy.begin());
    oracle::transfer(legacy,legacy_parameters,legacy_state,false);oracle::transfer(first,p,s,false);
    if(!std::equal(first.begin(),first.end(),legacy.begin()) || random.state!=legacy_random.state)
        throw std::runtime_error("legacy/shared prefix result changed");
    put(b,0x2a30,random.state);
    if(mode==1){replay_tyre_response(ghost_p,ghost,workspace.steering);oracle::transfer(second,ghost_p,ghost,false);}
    for(unsigned j=0;j<16;++j)put(b,context+j*4,std::bit_cast<unsigned>(workspace.steering[j]));
}
void read(std::ifstream& in,void* data,std::size_t size){if(!in.read(static_cast<char*>(data),static_cast<std::streamsize>(size)))throw std::runtime_error("short shared dynamics corpus");}
unsigned word(std::ifstream& in){unsigned value;read(in,&value,4);return value;}
}
int main(int argc,char** argv) {
    try {
        if(argc!=2)throw std::runtime_error("expected shared dynamics corpus");std::ifstream in(argv[1],std::ios::binary);
        char magic[8];read(in,magic,8);if(std::string(magic,8)!="SGNDS001")throw std::runtime_error("shared dynamics schema");
        const auto count=word(in);if(count==0 || count>20000)throw std::runtime_error("shared dynamics count");
        std::uint64_t words=0;unsigned errors=0;
        for(unsigned n=0;n<count;++n) {
            Record actual{},expected{};read(in,actual.data(),actual.size());read(in,expected.data(),expected.size());apply(actual);
            for(unsigned at=0;at<actual.size();at+=4) {
                ++words;if(u(actual,at)==u(expected,at))continue;
                if(errors++<24)std::cerr<<"case "<<n<<" offset "<<std::hex<<at<<" actual "<<u(actual,at)<<" expected "<<u(expected,at)<<'\n';
            }
        }
        if(in.peek()!=std::char_traits<char>::eof())throw std::runtime_error("trailing shared dynamics corpus");
        std::cout<<"{\"cases\":"<<count<<",\"exact_words\":"<<words<<",\"legacy_exact_words\":"<<std::uint64_t(count)*(car_size/4)
                 <<",\"errors\":"<<errors<<",\"allowed_ulp\":0,\"excluded_words\":0}\n";
        return errors?1:0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}
}
