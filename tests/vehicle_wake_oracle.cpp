#include "sgn/vehicle_wake.hpp"
#include "vehicle_oracle_state.hpp"
#include "retail_race_state.hpp"
#include <bit>
#include <cstring>
#include <fstream>
#include <iostream>

namespace {
using namespace sgn::retail;
constexpr unsigned car_size=0x7800,context=6*car_size,record_size=context+0x100;
using Record=std::array<std::uint8_t,record_size>;
unsigned u(const Record& b,unsigned at){unsigned value;std::memcpy(&value,b.data()+at,4);return value;}
void apply(Record& b,unsigned entry) {
    const auto current=u(b,context),ticks=u(b,context+8);
    if(current>=6 || ticks==0 || ticks>512)throw std::runtime_error("wake corpus current/tick domain");
    std::array<VehicleParameters,6> parameters;std::array<VehicleState,6> physics;std::array<VehicleRaceState,6> race;
    for(unsigned i=0;i<6;++i) {
        auto bytes=std::span(b).subspan(i*car_size,car_size);oracle::transfer(bytes,parameters[i],physics[i],true);race_oracle::transfer(bytes,race[i],true);
    }
    VehiclePositionHistory history;
    for(unsigned i=0;i<8;++i) {
        const auto at=current*car_size+0x1f2c+i*20;auto& sample=history.samples[i];
        sample.position={std::bit_cast<float>(u(b,at)),std::bit_cast<float>(u(b,at+4)),std::bit_cast<float>(u(b,at+8)),std::bit_cast<float>(u(b,at+12))};
        sample.radius=std::bit_cast<float>(u(b,at+16));
    }
    history.ticks=u(b,current*car_size+0x1fcc);
    std::array<VehicleWakeVehicle,6> vehicles={{{physics[0],race[0]},{physics[1],race[1]},{physics[2],race[2]},
        {physics[3],race[3]},{physics[4],race[4]},{physics[5],race[5]}}};
    for(unsigned tick=0;tick<ticks;++tick) {
        if(entry==0x40e080)apply_vehicle_wake(vehicles,current,history);
        else if(entry==0x40e290)update_vehicle_position_history(vehicles,current,history,std::bit_cast<std::int32_t>(u(b,context+4)));
        else throw std::runtime_error("unknown wake entry");
    }
    for(unsigned i=0;i<6;++i) {
        auto bytes=std::span(b).subspan(i*car_size,car_size);oracle::transfer(bytes,parameters[i],physics[i],false);race_oracle::transfer(bytes,race[i],false);
    }
    for(unsigned i=0;i<8;++i) {
        const auto at=current*car_size+0x1f2c+i*20;const auto& sample=history.samples[i];
        const std::array<float,5> fields={sample.position.x,sample.position.y,sample.position.z,sample.position.w,sample.radius};
        std::memcpy(b.data()+at,fields.data(),20);
    }
    std::memcpy(b.data()+current*car_size+0x1fcc,&history.ticks,4);
}
void read(std::ifstream& in,void* data,std::size_t size){if(!in.read(static_cast<char*>(data),static_cast<std::streamsize>(size)))throw std::runtime_error("short wake corpus");}
unsigned word(std::ifstream& in){unsigned value;read(in,&value,4);return value;}
}
int main(int argc,char** argv) {
    try {
        if(argc!=2)throw std::runtime_error("expected wake corpus");std::ifstream in(argv[1],std::ios::binary);
        char magic[8];read(in,magic,8);if(std::string(magic,8)!="SGNWK001")throw std::runtime_error("wake corpus schema");
        const auto count=word(in);if(count==0 || count>20000)throw std::runtime_error("wake corpus count");
        std::uint64_t words=0;unsigned errors=0;
        for(unsigned n=0;n<count;++n) {
            const auto entry=word(in);Record actual{},expected{};read(in,actual.data(),actual.size());read(in,expected.data(),expected.size());apply(actual,entry);
            for(unsigned at=0;at<actual.size();at+=4) {
                ++words;if(u(actual,at)==u(expected,at))continue;
                if(errors++<24)std::cerr<<"case "<<n<<" entry "<<std::hex<<entry<<" offset "<<at<<" actual "<<u(actual,at)<<" expected "<<u(expected,at)<<'\n';
            }
        }
        if(in.peek()!=std::char_traits<char>::eof())throw std::runtime_error("trailing wake corpus");
        std::cout<<"{\"cases\":"<<count<<",\"exact_words\":"<<words<<",\"errors\":"<<errors<<",\"allowed_ulp\":0,\"excluded_words\":0}\n";
        return errors?1:0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}
}
