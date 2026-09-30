#include "world_template_fixture.hpp"
#include "sgn/vehicle_presentation.hpp"
#include <bit>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace sgn::retail;
namespace {
constexpr std::size_t size=0xb000,model_at=0x7800,parts_at=0x7900,transforms_at=0x9900,globals_at=0xa300;
using Record=std::array<std::uint8_t,size>;
template<class T> void load(std::ifstream& s,T& v) {s.read(reinterpret_cast<char*>(&v),sizeof(v));if(!s)throw std::runtime_error("truncated presentation corpus");}
std::uint32_t read(std::ifstream& s){std::uint32_t v{};load(s,v);return v;}
std::uint32_t word(const Record& b,std::size_t at) {
    if(at>size-4)throw std::runtime_error("presentation comparison outside record");
    std::uint32_t value{};std::memcpy(&value,b.data()+at,4);return value;
}
void put(Record& b,std::size_t at,std::uint32_t value) {
    if(at>size-4)throw std::runtime_error("presentation mapper outside record");
    std::memcpy(b.data()+at,&value,4);
}
std::optional<std::size_t> handle(std::uint32_t value){return value==0xffffffffu?std::nullopt:std::optional<std::size_t>(value);}
void transfer(Record& b,VehicleState& s,VehicleParameters& p,VehiclePresentationState& visual,
              VehicleModelStorage& model,std::array<VehicleWheelAnimation,4>& wheels,bool reading) {
    auto u=[&](std::uint32_t& x,std::size_t at){if(reading)x=word(b,at);else put(b,at,x);};
    auto f=[&](float& x,std::size_t at){if(reading)x=std::bit_cast<float>(word(b,at));else put(b,at,std::bit_cast<std::uint32_t>(x));};
    auto i=[&](std::int32_t& x,std::size_t at){if(reading)x=std::bit_cast<std::int32_t>(word(b,at));else put(b,at,std::bit_cast<std::uint32_t>(x));};
    auto v=[&](VehicleVector& x,std::size_t at){f(x.x,at);f(x.y,at+4);f(x.z,at+8);f(x.w,at+12);};
    auto matrix=[&](VehicleMatrix& x,std::size_t at){for(unsigned k=0;k<16;++k)f(x[k],at+k*4);};
    auto h=[&](std::optional<std::size_t>& x,std::size_t at){if(reading)x=handle(word(b,at));else {if(x && *x>=0xffffffffull)throw std::runtime_error("test handle overflow");put(b,at,x?static_cast<std::uint32_t>(*x):0xffffffffu);}};
    u(visual.field_3134,0x3134);u(visual.field_33c8,0x33c8);u(visual.field_3508,0x3508);
    u(visual.field_3924,0x3924);u(visual.field_3928,0x3928);i(visual.idle_hold,0x7614);i(visual.idle_interval,0x7618);
    h(model.model,0x26b0);h(model.shadow,0x26b4);v(model.position,0x26b8);v(model.scale,0x26c8);
    v(model.target,0x26d8);v(model.up,0x26e8);for(unsigned k=0;k<4;++k)u(model.template_tail[k],0x26f8+k*4);
    u(model.field_2708,0x2708);matrix(model.body_rotation,0x270c);
    f(p.com_a,0x224);f(p.com_b,0x228);f(p.wheelbase,0x230);f(p.track_front,0x234);f(p.track_rear,0x238);
    v(s.position,0x1bac);v(s.basis_x,0x1cbc);v(s.basis_z,0x1cdc);
    f(s.up.x,0x1ccc);f(s.up.y,0x1cd0);f(s.up.z,0x1cd4);f(s.speed,0x1cf0);i(s.contact_class,0x1fd8);
    for(unsigned k=0;k<4;++k) {
        f(p.tyres[k].radius,0xcb8+k*16);f(s.wheels[k].speed,0x1fe4+k*0x98);
        f(s.suspension[k],0x2240+k*4);v(visual.wheel_centers[k],0x201c+k*0x98);
        f(wheels[k].previous_angle,0x2070+k*0x98);f(wheels[k].current_angle,0x2074+k*0x98);matrix(wheels[k].rotation,0x274c+k*64);
    }
}
bool cold_word(std::size_t at) {
    if(at==0x3134 || at==0x33c8 || at==0x3508 || at==0x3924 || at==0x3928 || at==0x7614 || at==0x7618)return true;
    if(at>=0x26b0 && at<0x284c)return true;
    for(unsigned i=0;i<4;++i)if((at>=0x201c+i*0x98 && at<0x202c+i*0x98) || at==0x2070+i*0x98 || at==0x2074+i*0x98)return true;
    return false;
}
bool unwritten(std::uint32_t entry,std::size_t at) {
    if(entry!=0x414460 && entry!=1)return false;
    for(unsigned i=0;i<4;++i)if(at==0x2028+i*0x98)return true;
    return false;
}
}
int main(int argc,char** argv) try {
    if(argc==2 && std::string_view(argv[1])=="--contracts") {
        std::array<VehiclePresentationState,8> p{};std::array<VehicleModelStorage,8> m{};
        std::array<std::array<VehicleWheelAnimation,4>,8> w{};unsigned rejected{};
        p[0].field_3134=73;m[0].field_2708=89;w[0][0].current_angle=17;
        for(unsigned field=0;field<3;++field) {
            const std::span ps(p.data(),field==0?7:8);const std::span ms(m.data(),field==1?7:8);const std::span ws(w.data(),field==2?7:8);
            try{initialize_cold_presentation({ps,ms,ws},0,1,sgn::retail::fixture::cold_world_template);}catch(const std::invalid_argument&){++rejected;}
            if(p[0].field_3134!=73 || m[0].field_2708!=89 || w[0][0].current_angle!=17)
                throw std::runtime_error("invalid cold span mutated state");
        }
        if(rejected!=3)throw std::runtime_error("invalid cold span was accepted");
        std::cout<<"cold_span_contracts=3 passed=1\n";return 0;
    }
    if(argc!=2)throw std::runtime_error("expected presentation corpus");
    std::ifstream stream(argv[1],std::ios::binary);std::array<char,8> magic{};load(stream,magic);
    if(std::string_view(magic.data(),8)!="SGNPRES1")throw std::runtime_error("invalid presentation corpus");
    const auto count=read(stream);if(count==0 || count>10000)throw std::runtime_error("invalid presentation case count");
    std::size_t words{},mismatches{},policy_words{},cold_words{};
    for(std::uint32_t n=0;n<count;++n) {
        const auto entry=read(stream);
        if(entry!=0x413da0 && entry!=0x4166f0 && entry!=0x414370 && entry!=0x414460 && entry!=0x4137b0 && entry!=1 && entry!=0)
            throw std::runtime_error("invalid presentation entry");
        const unsigned slots=entry==0?8:1;
        std::vector<Record> actual(slots),expected(slots),initial(slots);
        std::array<VehiclePresentationState,8> presentation{};std::array<VehicleModelStorage,8> models{};
        std::array<std::array<VehicleWheelAnimation,4>,8> wheels{};
        std::array<VehicleState,8> physics{};std::array<VehicleParameters,8> parameters{};
        for(unsigned slot=0;slot<slots;++slot) {
            load(stream,actual[slot]);load(stream,expected[slot]);initial[slot]=actual[slot];
            transfer(actual[slot],physics[slot],parameters[slot],presentation[slot],models[slot],wheels[slot],true);
        }
        auto& b=actual[0];
        if(entry==0)initialize_cold_presentation({presentation,models,wheels},handle(word(b,globals_at+4)),handle(word(b,globals_at+8)),sgn::retail::fixture::cold_world_template);
        else {
            auto real=[&](std::size_t at){return std::bit_cast<float>(word(b,at));};
            auto xyz=[&](std::size_t at){return sgn::Vec3{real(at),real(at+4),real(at+8)};};
            VehiclePresentationEffects effects{word(b,0x226c),word(b,0x328c),word(b,0x33cc),xyz(0x32b0),xyz(0x33f0)};
            std::array<std::uint16_t,128> flags{};std::array<sgn::Vec3,32> scale{};
            for(unsigned i=0;i<128;++i)flags[i]=static_cast<std::uint16_t>(word(b,parts_at+i*64));
            for(unsigned i=0;i<32;++i)scale[i]=xyz(transforms_at+i*80+16);
            const auto lod=word(b,model_at+0x5c);if(lod>=4)throw std::runtime_error("invalid presentation LOD");
            const unsigned base=lod*32;
            auto part=[&](unsigned index) {
                const auto tr=word(b,parts_at+(base+index)*64+8);
                if(tr>=scale.size())throw std::runtime_error("invalid presentation transform index");
                return VehicleEffectPart{flags[base+index],scale[tr]};
            };
            VehicleRandom random{word(b,globals_at)};
            if(entry==0x413da0 || entry==1)prepare_vehicle_presentation(presentation[0],effects,
                {{{flags[base+11],flags[base+12],flags[base+13],flags[base+14],flags[base+15],flags[base+16]}},part(19),part(20)});
            if(entry==0x4166f0)update_vehicle_effect_parts(effects,part(19),part(20));
            if(entry==0x414370 || entry==1)animate_vehicle_wheels(parameters[0],physics[0],wheels[0]);
            if(entry==0x414460 || entry==1)locate_vehicle_wheels(parameters[0],physics[0],presentation[0]);
            if(entry==0x4137b0 || entry==1)idle_vehicle_suspension(physics[0],presentation[0],random);
            put(b,globals_at,random.state);
            for(unsigned i=0;i<128;++i)put(b,parts_at+i*64,(word(b,parts_at+i*64)&0xffff0000u)|flags[i]);
            for(unsigned i=0;i<32;++i) {
                put(b,transforms_at+i*80+16,std::bit_cast<std::uint32_t>(scale[i].x));
                put(b,transforms_at+i*80+20,std::bit_cast<std::uint32_t>(scale[i].y));
                put(b,transforms_at+i*80+24,std::bit_cast<std::uint32_t>(scale[i].z));
            }
        }
        for(unsigned slot=0;slot<slots;++slot) {
            transfer(actual[slot],physics[slot],parameters[slot],presentation[slot],models[slot],wheels[slot],false);
            for(std::size_t at=0;at<size;at+=4) {
                if(entry==0 && !cold_word(at))continue;
                if(unwritten(entry,at)) {
                    ++policy_words;
                    if(word(actual[slot],at)!=word(initial[slot],at))throw std::runtime_error("wheel-center W policy failed");
                    continue;
                }
                ++words;if(entry==0)++cold_words;
                if(word(actual[slot],at)!=word(expected[slot],at)) {
                    if(mismatches<16)std::cerr<<"case="<<n<<" entry="<<std::hex<<entry<<" slot="<<slot<<" offset="<<at<<" actual="<<word(actual[slot],at)<<" expected="<<word(expected[slot],at)<<std::dec<<'\n';
                    ++mismatches;
                }
            }
        }
    }
    if(stream.peek()!=std::char_traits<char>::eof())throw std::runtime_error("trailing presentation corpus data");
    if(words==0)throw std::runtime_error("no presentation comparisons");
    std::cout<<"cases="<<count<<" words="<<words<<" native_policy_words="<<policy_words<<" cold_owned_words="<<cold_words<<" mismatches="<<mismatches<<" tolerance_ulp=0\n";
    return mismatches?1:0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}
