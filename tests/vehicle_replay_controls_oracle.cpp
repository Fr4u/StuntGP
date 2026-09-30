#include "sgn/retail_replay_controls.hpp"
#include "contact_solver_state.hpp"
#include <bit>
#include <cstring>
#include <fstream>
#include <iostream>

namespace {
using namespace sgn::retail;
using Record=std::array<std::uint8_t,0x7900>;
std::uint32_t u(const Record& b,unsigned at){std::uint32_t x;std::memcpy(&x,b.data()+at,4);return x;}
float f(const Record& b,unsigned at){return std::bit_cast<float>(u(b,at));}
void apply(Record& b,unsigned entry) {
    VehicleParameters p;VehicleState s;VehicleRaceState race;contact::solver::State contact;
    contact::oracle::transfer(b,p,s,race,contact,true);
    const ReplayFramePair frames{std::span<const std::uint8_t,44>(b.data()+0x7800,44),
        std::span<const std::uint8_t,44>(b.data()+0x7830,44),f(b,0x7860)};
    switch(entry) {
    case 0x41a330:decode_replay_steering(frames,s);break;
    case 0x41a3c0:decode_replay_suspension(frames,s);break;
    case 0x41a430:decode_replay_loads(frames,s,contact,f(b,0x7864));break;
    case 0x41a8d0:decode_replay_turbo(frames,s);break;
    case 0x41a7e0:decode_replay_gear(frames,s);break;
    case 0x41a9b0:decode_replay_auxiliary(frames,s);break;
    case 0x41a900:decode_replay_brake(frames,s);break;
    case 0x41a370:decode_replay_rpm(frames,s);break;
    case 0x41a930:decode_replay_throttle(frames,s);break;
    case 0x41a9e0:decode_replay_shock(frames,contact);break;
    case 0x41aa60:decode_replay_surface(frames,contact);break;
    default:throw std::runtime_error("unknown replay control entry");
    }
    contact::oracle::transfer(b,p,s,race,contact,false);
}
void read(std::ifstream& in,void* out,std::size_t n){if(!in.read(static_cast<char*>(out),static_cast<std::streamsize>(n)))throw std::runtime_error("short replay control corpus");}
unsigned word(std::ifstream& in){unsigned x;read(in,&x,4);return x;}
}
int main(int argc,char** argv) {
    try {
        if(argc!=2)throw std::runtime_error("expected replay control corpus");std::ifstream in(argv[1],std::ios::binary);
        char magic[8];read(in,magic,8);if(std::string(magic,8)!="SGNRI001")throw std::runtime_error("replay control schema");
        const auto count=word(in);if(count==0 || count>20000)throw std::runtime_error("replay control count");
        std::uint64_t words=0;unsigned errors=0;
        for(unsigned n=0;n<count;++n) {
            const auto entry=word(in);Record actual{},expected{};read(in,actual.data(),actual.size());read(in,expected.data(),expected.size());
            apply(actual,entry);
            for(unsigned at=0;at<actual.size();at+=4) {
                ++words;if(u(actual,at)==u(expected,at))continue;
                if(errors++<24)std::cerr<<"case "<<n<<" entry "<<std::hex<<entry<<" offset "<<at<<" actual "<<u(actual,at)<<" expected "<<u(expected,at)<<'\n';
            }
        }
        if(in.peek()!=std::char_traits<char>::eof())throw std::runtime_error("trailing replay control corpus");
        std::cout<<"{\"cases\":"<<count<<",\"exact_words\":"<<words<<",\"errors\":"<<errors<<",\"allowed_ulp\":0,\"excluded_words\":0}\n";
        return errors?1:0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}
}
