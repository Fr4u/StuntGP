#include "sgn/retail_replay_dynamics.hpp"
#include "vehicle_oracle_state.hpp"
#include <bit>
#include <cstring>
#include <fstream>
#include <iostream>

namespace {
using namespace sgn::retail;
using Record=std::array<std::uint8_t,0x7900>;
std::uint32_t u(const Record& b,unsigned at){std::uint32_t value;std::memcpy(&value,b.data()+at,4);return value;}
float f(const Record& b,unsigned at){return std::bit_cast<float>(u(b,at));}
void apply(Record& b,unsigned entry) {
    VehicleParameters parameters;VehicleState state;oracle::transfer(b,parameters,state,true);
    VehicleMatrix shared{};for(unsigned i=0;i<16;++i)shared[i]=f(b,0x7800+i*4);
    switch(entry) {
    case 0x419b80:replay_tyre_response(parameters,state,shared);break;
    case 0x419e30:replay_contact_state(state);break;
    case 0x419e90:replay_braking_signals(state);break;
    case 0x4062b0:align_airborne_vehicle(state,{f(b,0x7840),f(b,0x7844),f(b,0x7848)});break;
    default:throw std::runtime_error("unknown replay dynamics entry");
    }
    oracle::transfer(b,parameters,state,false);
}
void read(std::ifstream& in,void* data,std::size_t size){if(!in.read(static_cast<char*>(data),static_cast<std::streamsize>(size)))throw std::runtime_error("short replay dynamics corpus");}
unsigned word(std::ifstream& in){unsigned value;read(in,&value,4);return value;}
}
int main(int argc,char** argv) {
    try {
        if(argc!=2)throw std::runtime_error("expected replay dynamics corpus");std::ifstream in(argv[1],std::ios::binary);
        char magic[8];read(in,magic,8);if(std::string(magic,8)!="SGNRD001")throw std::runtime_error("replay dynamics schema");
        const auto count=word(in);if(count==0 || count>20000)throw std::runtime_error("replay dynamics count");
        std::uint64_t words=0;unsigned errors=0;
        for(unsigned n=0;n<count;++n) {
            const auto entry=word(in);Record actual{},expected{};read(in,actual.data(),actual.size());read(in,expected.data(),expected.size());apply(actual,entry);
            for(unsigned at=0;at<actual.size();at+=4) {
                ++words;if(u(actual,at)==u(expected,at))continue;
                if(errors++<24)std::cerr<<"case "<<n<<" entry "<<std::hex<<entry<<" offset "<<at<<" actual "<<u(actual,at)<<" expected "<<u(expected,at)<<'\n';
            }
        }
        if(in.peek()!=std::char_traits<char>::eof())throw std::runtime_error("trailing replay dynamics corpus");
        std::cout<<"{\"cases\":"<<count<<",\"exact_words\":"<<words<<",\"errors\":"<<errors<<",\"allowed_ulp\":0,\"excluded_words\":0}\n";
        return errors?1:0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}
}
