#include "sgn/retail_replay_decode.hpp"
#include <bit>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace sgn::retail;
namespace {
constexpr std::size_t record_size=0x2a00;
using Record=std::array<std::uint8_t,record_size>;
template<class T> void load(std::ifstream& stream,T& object) {
    stream.read(reinterpret_cast<char*>(&object),sizeof(object));
    if(!stream)throw std::runtime_error("truncated replay decode corpus");
}
std::uint32_t read(std::ifstream& stream) {std::uint32_t value{};load(stream,value);return value;}
std::uint32_t word(const Record& b,std::size_t at) {
    if(at>record_size-4)throw std::runtime_error("replay decode comparison offset outside record");
    std::uint32_t value{};std::memcpy(&value,b.data()+at,4);return value;
}
// Local test bridge, deliberately independent of the actively owned physics mapper.
void transfer(Record& b,VehicleState& s,VehicleParameters& p,std::array<VehicleWheelAnimation,4>& a,bool reading) {
    auto f=[&](float& v,std::size_t at) {
        if(at>record_size-4)throw std::runtime_error("replay decode mapper offset outside record");
        if(reading)v=std::bit_cast<float>(word(b,at));else std::memcpy(b.data()+at,&v,4);
    };
    auto v=[&](VehicleVector& q,std::size_t at){f(q.x,at);f(q.y,at+4);f(q.z,at+8);f(q.w,at+12);};
    auto xyz=[&](sgn::Vec3& q,std::size_t at){f(q.x,at);f(q.y,at+4);f(q.z,at+8);};
    v(s.position,0x1bac);v(s.previous_position,0x1bbc);
    xyz(s.velocity,0x1c8c);f(s.velocity_w,0x1c98);
    xyz(s.previous_velocity,0x1c9c);f(s.previous_velocity_w,0x1ca8);
    v(s.basis_x,0x1cbc);xyz(s.up,0x1ccc);f(s.basis_y_w,0x1cd8);v(s.basis_z,0x1cdc);f(s.speed,0x1cf0);
    for(std::size_t i=0;i<4;++i) {
        f(p.tyres[i].radius,0xcb8+i*16);
        f(s.wheels[i].acceleration,0x1fe0+i*0x98);f(s.wheels[i].speed,0x1fe4+i*0x98);
        f(a[i].previous_angle,0x2070+i*0x98);f(a[i].current_angle,0x2074+i*0x98);
        for(std::size_t k=0;k<16;++k)f(a[i].rotation[k],0x274c+i*64+k*4);
    }
}
bool excluded(std::uint32_t entry,std::size_t at) {
    // A corpus cannot supply, widen, or disable this mask.
    if((entry==0x419ee0 || entry==3) && (at==0x1bb8 || at==0x1bc8))return true;
    return (entry==0x41a000 || entry==3) && (at==0x1cc8 || at==0x1cd8 || at==0x1ce8);
}
}
int main(int argc,char** argv) try {
    if(argc!=2)throw std::runtime_error("expected replay decode corpus path");
    std::ifstream stream(argv[1],std::ios::binary);std::array<char,8> magic{};load(stream,magic);
    if(std::string_view(magic.data(),8)!="SGNRDEC1")throw std::runtime_error("wrong replay decode corpus");
    const auto count=read(stream);
    if(count==0 || count>10000)throw std::runtime_error("invalid replay decode case count");
    std::size_t words{},mismatches{},excluded_words{},native_policy_words{},composition_steps{},continuous_native_steps{};
    VehicleState state;VehicleParameters parameters;std::array<VehicleWheelAnimation,4> animation{};
    Record previous_native{};std::array<std::uint8_t,44> previous_next{};
    for(std::uint32_t n=0;n<count;++n) {
        const auto entry=read(stream);
        if(entry!=0x419ee0 && entry!=0x41a000 && entry!=0x41a230 && entry!=3)
            throw std::runtime_error("invalid replay decode entry");
        const float phase=std::bit_cast<float>(read(stream));
        std::array<std::uint8_t,44> current{},next{};load(stream,current);load(stream,next);
        Record actual{},expected{};load(stream,actual);load(stream,expected);const Record initial=actual;
        if(entry==3 && composition_steps%16!=0) {
            // Carry live native objects from the previous call, never expected
            // oracle state. The corpus boundary must agree with that result.
            if(actual!=previous_native || current!=previous_next)
                throw std::runtime_error("broken continuous native decoder sequence");
            ++continuous_native_steps;
        } else transfer(actual,state,parameters,animation,true);
        const ReplayFramePair frames{current,next,phase};
        if(entry==0x419ee0 || entry==3)decode_replay_position(frames,state);
        if(entry==0x41a000 || entry==3)decode_replay_basis(frames,state);
        if(entry==0x41a230 || entry==3)decode_replay_wheels(frames,state,parameters,animation);
        transfer(actual,state,parameters,animation,false);
        if(entry==3) {++composition_steps;previous_native=actual;previous_next=next;}
        for(std::size_t at=0;at<record_size;at+=4) {
            const auto value=word(actual,at);
            if(excluded(entry,at)) {
                ++excluded_words;++native_policy_words;
                if(value!=word(initial,at))throw std::runtime_error("native unwritten-W preservation policy failed");
                continue;
            }
            ++words;
            if(value!=word(expected,at)) {
                if(mismatches<16)std::cerr<<"case="<<n<<" entry="<<std::hex<<entry<<" offset="<<at
                    <<" actual="<<value<<" expected="<<word(expected,at)<<std::dec<<'\n';
                ++mismatches;
            }
        }
    }
    if(stream.peek()!=std::char_traits<char>::eof())throw std::runtime_error("trailing replay decode corpus data");
    if(words==0)throw std::runtime_error("no replay decode comparisons");
    std::cout<<"cases="<<count<<" words="<<words<<" excluded_words="<<excluded_words
        <<" native_policy_words="<<native_policy_words<<" mismatches="<<mismatches<<" tolerance_ulp=0"
        <<" continuous_native_steps="<<continuous_native_steps<<'\n';
    return mismatches?1:0;
}catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 2;}
