#include "sgn/vehicle_segments.hpp"
#include "sgn/model_resources.hpp"
#include <bit>
#include <cstring>
#include <fstream>
#include <iostream>
namespace {
using namespace sgn;
using namespace sgn::retail;
using Record=std::array<std::uint32_t,4096>;
struct State {
    VehicleParameters parameters{};VehicleState physics{};VehiclePresentationState presentation{};
    VehicleAnchorState anchors{};VehicleSegments segments{};VehicleMatrix basis{};contact::Vector4 effect{};
    std::array<std::array<ModelTransform,6>,4> runs{};
};
static_assert(sizeof(ModelTransform)==80);
static_assert(sizeof(VehicleSegments)==288);
template<class T>void read(std::ifstream& f,T& x){if(!f.read(reinterpret_cast<char*>(&x),sizeof(x)))throw std::runtime_error("truncated segments corpus");}
void transfer(Record& r,State& s,bool loading) {
    auto f=[&](float& x,std::size_t at){if(loading)x=std::bit_cast<float>(r[at/4]);else r[at/4]=std::bit_cast<std::uint32_t>(x);};
    auto v=[&](auto& x,std::size_t at){f(x.x,at);f(x.y,at+4);f(x.z,at+8);f(x.w,at+12);};
    auto xyz=[&](auto& x,std::size_t at){f(x.x,at);f(x.y,at+4);f(x.z,at+8);};
    f(s.parameters.turbo_max,0xd64);v(s.physics.position,0x1bac);xyz(s.physics.acceleration,0x1cac);
    v(s.physics.basis_x,0x1cbc);xyz(s.physics.up,0x1ccc);v(s.physics.basis_z,0x1cdc);
    f(s.physics.turbo_rpm,0x2254);f(s.physics.body_heave,0x288c);
    if(loading)s.presentation.field_3134=r[0x3134/4];else r[0x3134/4]=s.presentation.field_3134;
    v(s.anchors.position,0x3138);v(s.anchors.forward,0x3148);f(s.anchors.extent,0x3158);xyz(s.effect,0x327c);
    if(loading)s.effect.opaque=r[0x3288/4];else r[0x3288/4]=s.effect.opaque;
    for(unsigned i=0;i<16;++i){f(s.physics.orientation[i],0x1bcc+i*4);f(s.basis[i],0x3300+i*4);}
    for(unsigned i=0;i<6;++i){auto& item=s.segments.items[i];v(item.angles,0x315c+i*48);v(item.velocity,0x316c+i*48);v(item.direction,0x317c+i*48);}
    for(unsigned i=0;i<4;++i){auto* data=r.data()+(0x3400+i*0x200)/4;
        if(loading)std::memcpy(s.runs[i].data(),data,480);else std::memcpy(data,s.runs[i].data(),480);}
}
}
int main(int argc,char** argv)try {
    if(argc!=2)throw std::runtime_error("expected segments corpus");
    std::ifstream f(argv[1],std::ios::binary);std::array<char,8> magic{};read(f,magic);
    if(std::string_view(magic.data(),8)!="SGNSEG01")throw std::runtime_error("bad segments magic");
    std::uint32_t count{};read(f,count);if(count==0||count>20000)throw std::runtime_error("bad segments case count");
    State s;Record previous{};std::uint32_t previous_op=99;std::size_t mismatches{},words{},masked{},continuous{};
    for(std::uint32_t n=0;n<count;++n){
        std::uint32_t op{};read(f,op);if(op>3)throw std::runtime_error("invalid segments operation");
        Record r{},expected{};read(f,r);read(f,expected);const auto input=r;
        const auto lod=r[0x3d00/4],step=r[0x3d14/4];if(lod>=4||step>=16)throw std::runtime_error("segments selection or step outside domain");
        for(unsigned i=0;i<4;++i)if(r[0x3d04/4+i]>=4)throw std::runtime_error("segments run mapping outside domain");
        if(op!=3&&step!=0)throw std::runtime_error("unexpected segments step");
        const auto run=r[0x3d04/4+lod];
        if(op==3&&step!=0){
            if(previous_op!=3||previous[0x3d14/4]+1!=step)throw std::runtime_error("broken segments step order");
            previous[0x3d14/4]=step;
            if(previous!=r)throw std::runtime_error("broken continuous native segments state");++continuous;
        }else transfer(r,s,true);
        if(op!=1)update_vehicle_segments(s.parameters,s.physics,s.presentation,s.anchors,s.segments);
        if(op!=0)present_vehicle_segments(s.physics,s.presentation,s.anchors,s.segments,s.basis,
            s.presentation.field_3134?std::span<ModelTransform>{s.runs[run]}:std::span<ModelTransform>{},s.effect);
        transfer(r,s,false);previous=r;previous_op=op;
        for(std::size_t i=0;i<r.size();++i){
            bool undefined=false;
            if(op!=0&&s.presentation.field_3134)for(unsigned j=0;j<6;++j)if(i==(0x340c+run*0x200+j*80)/4)undefined=true;
            if(undefined){++masked;if(r[i]!=input[i])throw std::runtime_error("native undefined W retention violated");continue;}
            ++words;if(r[i]!=expected[i]){if(mismatches<20)std::cerr<<"case="<<n<<" op="<<op<<" offset="<<std::hex<<i*4<<" actual="<<r[i]<<" expected="<<expected[i]<<std::dec<<'\n';++mismatches;}
        }
    }
    if(f.peek()!=std::char_traits<char>::eof())throw std::runtime_error("trailing segments corpus data");
    std::cout<<"cases="<<count<<" words="<<words<<" source_undefined_W="<<masked<<" continuous_native_steps="<<continuous<<" mismatches="<<mismatches<<" tolerance_ulp=0\n";
    return mismatches?1:0;
}catch(const std::exception& e){std::cerr<<"segments oracle: "<<e.what()<<'\n';return 2;}
