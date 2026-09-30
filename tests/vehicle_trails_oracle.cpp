#include "sgn/vehicle_trails.hpp"
#include <bit>
#include <cstring>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>

using namespace sgn::retail;
namespace {
constexpr std::size_t size=0x8000,model_at=0x7800,scratch_at=0x7840,view_at=0x7880,projection_at=0x78c0,tick_at=0x7900;
using Record=std::array<std::uint8_t,size>;
template<class T> void load(std::ifstream& s,T& v){s.read(reinterpret_cast<char*>(&v),sizeof(v));if(!s)throw std::runtime_error("truncated trail corpus");}
std::uint32_t read(std::ifstream& s){std::uint32_t v{};load(s,v);return v;}
std::uint32_t word(const Record& b,std::size_t at){if(at>size-4)throw std::runtime_error("trail offset outside record");std::uint32_t v{};std::memcpy(&v,b.data()+at,4);return v;}
void put(Record& b,std::size_t at,std::uint32_t v){if(at>size-4)throw std::runtime_error("trail offset outside record");std::memcpy(b.data()+at,&v,4);}
float real(const Record& b,std::size_t at){return std::bit_cast<float>(word(b,at));}
void transfer(Record& b,VehicleState& s,VehicleParameters& p,VehicleTrailStorage& trails,VehicleTrailInputs& input,
              VehicleTrailModel& model,VehicleMatrix& scratch,VehicleTrailProjection& projection,bool reading) {
    auto u=[&](std::uint32_t& x,std::size_t at){if(reading)x=word(b,at);else put(b,at,x);};
    auto f=[&](float& x,std::size_t at){if(reading)x=real(b,at);else put(b,at,std::bit_cast<std::uint32_t>(x));};
    auto v=[&](VehicleVector& x,std::size_t at){f(x.x,at);f(x.y,at+4);f(x.z,at+8);f(x.w,at+12);};
    auto m=[&](VehicleMatrix& x,std::size_t at){for(unsigned k=0;k<16;++k)f(x[k],at+k*4);};
    f(p.height,0x244);f(s.speed,0x1cf0);f(s.body_heave,0x288c);v(s.position,0x1bac);v(s.basis_x,0x1cbc);v(s.basis_z,0x1cdc);
    f(s.up.x,0x1ccc);f(s.up.y,0x1cd0);f(s.up.z,0x1cd4);u(input.field_28a0,0x28a0);
    for(unsigned k=0;k<64;++k)v(trails.speed[k],0x351c+k*16);
    u(s.camera_effects.values[2],0x391c);u(s.camera_effects.values[3],0x3920);
    constexpr std::array<std::size_t,2> ar{0x2d14,0x2f2c},br{0x28d0,0x2aec};
    constexpr std::array<std::size_t,2> ai{0x2d04,0x2f1c},bi{0x28c0,0x2adc};
    constexpr std::array<std::size_t,2> ac{0x2f14,0x312c},bc{0x2ad0,0x2cec},bf{0x28bc,0x2ad8};
    for(unsigned j=0;j<2;++j) {
        v(input.pair_a[j],ai[j]);v(input.pair_b[j],bi[j]);u(input.pair_b_available[j],bf[j]);
        u(trails.cursor_a[j],ac[j]);u(trails.cursor_b[j],bc[j]);
        u(s.camera_effects.values[4+j],ac[j]+4);u(s.camera_effects.values[6+j],bc[j]+4);
        for(unsigned k=0;k<32;++k){v(trails.pair_a[j][k],ar[j]+k*16);v(trails.pair_b[j][k],br[j]+k*16);}
    }
    v(model.base_forward,model_at);v(model.base_up,model_at+16);f(model.part23_position_z,model_at+32);
    m(scratch,scratch_at);m(projection.world_view,view_at);
    f(projection.projection_x,projection_at);f(projection.projection_y,projection_at+4);
    f(projection.center_x,projection_at+8);f(projection.center_y,projection_at+12);
    f(projection.scale_x,projection_at+16);f(projection.scale_y,projection_at+20);f(projection.clip_near,projection_at+24);
}
std::set<std::size_t> mask(std::uint32_t entry,const Record& b) {
    std::set<std::size_t> result;
    auto ring=[&](std::size_t at,std::size_t cursor,std::uint32_t capacity) {
        const auto next=(word(b,cursor)+1u)%capacity;
        result.insert(scratch_at+12);result.insert(scratch_at+28);
        result.insert(at+capacity*16+12);result.insert(at+next*16+12);
    };
    if((entry==0x414c60 || entry==1) && word(b,tick_at)>=20 &&
       double(real(b,0x1cf0))-std::bit_cast<float>(std::uint32_t{0x42855555})>=0)
        ring(0x351c,0x391c,16);
    if(word(b,tick_at)>=60 && word(b,0x28a0)!=0) {
        if(entry==0x414f90 || entry==1){ring(0x2d14,0x2f14,5);ring(0x2f2c,0x312c,5);}
        if(entry==0x4152d0 || entry==1){ring(0x28d0,0x2ad0,9);ring(0x2aec,0x2cec,9);}
    }
    return result;
}
}
int main(int argc,char** argv) try {
    if(argc!=2)throw std::runtime_error("expected trail corpus");
    std::ifstream stream(argv[1],std::ios::binary);std::array<char,8> magic{};load(stream,magic);
    if(std::string_view(magic.data(),8)!="SGNTRAL1")throw std::runtime_error("invalid trail corpus");
    const auto count=read(stream);if(count==0 || count>10000)throw std::runtime_error("invalid trail case count");
    VehicleState physics;VehicleParameters parameters;VehicleTrailStorage storage;VehicleTrailInputs inputs;
    VehicleTrailModel model;VehicleMatrix scratch{};VehicleTrailProjection projection;
    const auto settings=initial_vehicle_trail_settings();
    std::size_t words{},mismatches{},policy_words{},composition_steps{},continuous_steps{};Record previous{};
    for(std::uint32_t n=0;n<count;++n) {
        const auto entry=read(stream);if(entry!=0x414c60 && entry!=0x414f90 && entry!=0x4152d0 && entry!=1)
            throw std::runtime_error("invalid trail entry");
        Record actual{},expected{};load(stream,actual);load(stream,expected);const Record initial=actual;
        const auto excluded=mask(entry,initial);
        if(entry==1 && composition_steps%16!=0) {
            put(previous,tick_at,word(actual,tick_at));
            if(actual!=previous)throw std::runtime_error("broken continuous native trail sequence");
            ++continuous_steps;
        }else transfer(actual,physics,parameters,storage,inputs,model,scratch,projection,true);
        const VehicleTrailBindings binding{physics,parameters,storage,scratch,inputs,model,projection,settings,word(actual,tick_at)};
        if(entry==0x414c60 || entry==1)update_vehicle_speed_trail(binding);
        if(entry==0x414f90 || entry==1)update_vehicle_pair_a_trails(binding);
        if(entry==0x4152d0 || entry==1)update_vehicle_pair_b_trails(binding);
        transfer(actual,physics,parameters,storage,inputs,model,scratch,projection,false);
        if(entry==1){++composition_steps;previous=actual;}
        for(std::size_t at=0;at<size;at+=4) {
            if(excluded.contains(at)) {
                ++policy_words;
                if(word(actual,at)!=word(initial,at))throw std::runtime_error("trail W preservation failed");
                continue;
            }
            ++words;
            if(word(actual,at)!=word(expected,at)) {
                if(mismatches<16)std::cerr<<"case="<<n<<" entry="<<std::hex<<entry<<" offset="<<at<<" actual="<<word(actual,at)<<" expected="<<word(expected,at)<<std::dec<<'\n';
                ++mismatches;
            }
        }
    }
    if(stream.peek()!=std::char_traits<char>::eof())throw std::runtime_error("trailing trail corpus data");
    if(words==0)throw std::runtime_error("no trail comparisons");
    std::cout<<"cases="<<count<<" words="<<words<<" native_policy_words="<<policy_words<<" continuous_native_steps="<<continuous_steps<<" mismatches="<<mismatches<<" tolerance_ulp=0\n";
    return mismatches?1:0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}
