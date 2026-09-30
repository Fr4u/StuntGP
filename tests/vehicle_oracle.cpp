#include "sgn/vehicle.hpp"
#include "vehicle_oracle_state.hpp"
#include "sgn/vehicle_stunt_pattern.hpp"
#include "sgn/vehicle_math.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {
using namespace sgn::retail;
constexpr std::size_t vehicle_record_size=0x7800,record_size=0xf800,parameter_record_size=0x2a40;
using Record=std::array<std::uint8_t,record_size>;
std::uint32_t word(const Record& b,std::size_t at) { std::uint32_t x; std::memcpy(&x,b.data()+at,4); return x; }
void put(Record& b,std::size_t at,std::uint32_t x) { std::memcpy(b.data()+at,&x,4); }
float real(const Record& b,std::size_t at) { return std::bit_cast<float>(word(b,at)); }
void put_real(Record& b,std::size_t at,float x) { put(b,at,std::bit_cast<std::uint32_t>(x)); }
// This adapter belongs only to the oracle. Runtime state has no guest pointers,
// binary-layout aliasing, or dependency on an x86 image.
using sgn::retail::oracle::transfer;

bool paired_entry(std::uint32_t entry) {
    return entry==0x410dd0 || entry==0x410f50 || entry==0x411140 || entry==0x411280 ||
        entry==0x411410 || entry==0x411330 || entry==0x4118b0 || entry==0x412430 || entry==0x411490 ||
        entry==0x411880 || entry==0x411890 || entry==0x412260 || entry==0x4115c0 || entry==0x413470 || entry==0x413490;
}
void apply(std::uint32_t entry,unsigned index,Record& b) {
    auto matrix=[&](unsigned at){VehicleMatrix m{};for(unsigned j=0;j<16;++j)m[j]=real(b,at+j*4);return m;};
    auto write_matrix=[&](unsigned at,const VehicleMatrix& m){for(unsigned j=0;j<16;++j)put_real(b,at+j*4,m[j]);};
    auto vector=[&](unsigned at){return VehicleVector{real(b,at),real(b,at+4),real(b,at+8),real(b,at+12)};};
    if((entry>=0x44ed40 && entry<=0x44f8e0) || entry==0x40f330) {
        auto m=matrix(0x1bcc);auto left=matrix(0x1c0c);
        switch(entry){
        case 0x44ed40:case 0x44eda0:{float length{};auto v=vehicle_normalize(vector(0x1cbc),&length);
            put_real(b,0x1cbc,v.x);put_real(b,0x1cc0,v.y);put_real(b,0x1cc4,v.z);
            if(entry==0x44eda0)put_real(b,0x2a04,length);return;}
        case 0x44ee10:m=vehicle_identity();break;
        case 0x44ee50:m=vehicle_transpose(left);break;
        case 0x44ef40:vehicle_premultiply(m,left);break;
        case 0x44efd0:{auto right=m;vehicle_multiply_basis(m,right,left);break;}
        case 0x44f050:m=vehicle_basis(vector(0x1cbc),vector(0x1ccc),vector(0x1cdc));break;
        case 0x44f640:vehicle_rotate(m,0,real(b,0x2a00));break;
        case 0x44f6b0:vehicle_rotate(m,1,real(b,0x2a00));break;
        case 0x44f720:vehicle_rotate(m,2,real(b,0x2a00));break;
        case 0x44f790:m=vehicle_rotation(0,real(b,0x2a00));break;
        case 0x44f800:m=vehicle_rotation(1,real(b,0x2a00));break;
        case 0x44f870:m=vehicle_rotation(2,real(b,0x2a00));break;
        case 0x44f8e0:vehicle_orthogonalize(m);break;
        case 0x40f330:vehicle_level_matrix(m,real(b,0x2a00));break;
        default:throw std::runtime_error("unknown vehicle math entry");
        }
        write_matrix(0x1bcc,m);return;
    }
    VehicleParameters p;VehicleState s;transfer(b,p,s,true);
    if(paired_entry(entry)) {
        Record other{};std::copy_n(b.begin()+0x8000,vehicle_record_size,other.begin());
        VehicleParameters other_parameters;VehicleState other_state;transfer(other,other_parameters,other_state,true);
        auto& target=(index&1)?other_state:s;
        switch(entry) {
        case 0x410dd0:reset_balance_counters(target);break;
        case 0x410f50:reset_flip_state(target,s);break;
        case 0x411140:reset_drift_counter(target);break;
        case 0x411280:reset_slipstream_counter(target);break;
        case 0x411410:reset_roof_counter(target);break;
        case 0x411330:reset_inversion_state(target);break;
        case 0x4118b0:reset_phase_history(s);break;
        case 0x412430:reset_air_rotation(target);break;
        case 0x411490:reset_stunt_state(target,s);break;
        case 0x411880:reset_stunt_score_hold(target);break;
        case 0x411890:begin_stunt_landing(s);break;
        case 0x412260:reset_launch_state(s);break;
        case 0x4115c0:begin_stunt_height(s);break;
        case 0x413470:stop_all_wheels(target);break;
        case 0x413490:reset_vehicle_motion(target);break;
        }
        transfer(b,p,s,false);transfer(other,other_parameters,other_state,false);
        std::copy_n(other.begin(),vehicle_record_size,b.begin()+0x8000);return;
    }
    // Extra host context is placed after the numeric block by the corpus writer.
    s.special_mode=static_cast<std::int32_t>(word(b,0x2a0c));
    EngineContext context{static_cast<std::int32_t>(word(b,0x2a20)),word(b,0x2a24)!=0,real(b,0x2a28),real(b,0x2a2c)};
    float a=real(b,0x2a00),c=real(b,0x2a04);
    VehicleRandom random{word(b,0x2a30)};
    VehicleSupport support;support.body_position=vector(0x7270);
    for(unsigned j=0;j<4;++j){support.local[j]=vector(0x73b4+j*0x4c);support.current[j]=vector(0x73c4+j*0x4c);}
    const sgn::Vec3 gravity{real(b,0x2a38),real(b,0x2a3c),real(b,0x2a40)};
    const sgn::Vec3 unit_gravity{real(b,0x2a48),real(b,0x2a4c),real(b,0x2a50)};
    switch(entry) {
    case 0x40bf20:negative_pitch_input(p,s);break;
    case 0x40bfb0:positive_pitch_input(p,s);break;
    case 0x40c040:positive_yaw_input(p,s);break;
    case 0x40c0d0:negative_yaw_input(p,s);break;
    case 0x40c4e0:clear_angular_acceleration(s);break;
    case 0x414940:assist_air_pitch(s,unit_gravity);break;
    case 0x4149e0:assist_air_yaw(s);break;
    case 0x40d090:digital_stunt_input(p,s,word(b,0x2a34),unit_gravity);break;
    case 0x40c5f0:analog_stunt_input(p,s,word(b,0x2a34),unit_gravity);break;
    case 0x4118d0:save_phase_history(s);break;
    case 0x411950:update_phase_history(s,unit_gravity);break;
    case 0x4114e0:accumulate_air_score(s,context.game_mode);break;
    case 0x4121c0:update_stunt_chain_decay(s);break;
    case 0x412270:steer_air_velocity(s,unit_gravity);break;
    case 0x41d1f0:record_stunt_statistic(s.stunt_statistics[index],a);break;
    case 0x41d230:record_stunt_height(s);break;
    case 0x4163e0:set_launch_effect(s,a);break;
    case 0x4116f0:launch_stunt(p,s,word(b,0x2a34));break;
    case 0x406ff0:reset_stunt_meter(s);break;
    case 0x411fa0:fail_stunt_landing(s);break;
    case 0x411c10:{VehicleStuntPattern pattern;
        for(unsigned j=0;j<3;++j){pattern.messages[j]=word(b,0x3a00+j*4);pattern.score_indices[j]=word(b,0x3a0c+j*4);}
        for(unsigned j=0;j<64;++j){pattern.pitch[j]=std::bit_cast<std::int8_t>(b[0x3a18+j]);pattern.yaw[j]=std::bit_cast<std::int8_t>(b[0x3a58+j]);}
        put(b,0x2a54,match_stunt_pattern(s,pattern));break;}
    case 0x40e3a0:distribute_drive(p,s,a);break;
    case 0x40e470:engage_clutch(p,s);break;
    case 0x40e710:{double out=driven_mass(p,s,a);std::memcpy(b.data()+0x2a10,&out,8);break;}
    case 0x40ead0:update_turbo(p,s);break;
    case 0x40eba0:limit_reverse_rpm(p,s);break;
    case 0x40d650:a=limit_low_energy(s,a);break;
    case 0x40f3c0:update_reference_load(s,real(b,0x2a08));break;
    case 0x40f3f0:rolling_resistance(p,s,index,c,a);break;
    case 0x40f4a0:aerodynamic_drag(p,s,a,c,0);break;
    case 0x40f570:attenuate_steering(p,s);break;
    case 0x40f5b0:tyre_friction(p,s,index,a,c);break;
    case 0x40f730:average_grip(s);break;
    case 0x40fb00:airborne_wheel_decay(s);break;
    case 0x40fb30:classify_contacts(s);break;
    case 0x40edd0:aerodynamic_lift(p,s);break;
    case 0x413450:clear_wheel_acceleration(s);break;
    case 0x413430:stop_wheel(s,index);break;
    case 0x40a1d0:build_engine_curve(p,s);break;
    case 0x40c170:shift_up(p,s);break;
    case 0x40c1a0:shift_down(p,s);break;
    case 0x40c1f0:shift_neutral(s);break;
    case 0x40e550:automatic_shift(p,s,context);break;
    case 0x40ec00:consume_engine_energy(p,s,context,a);break;
    case 0x40eca0:consume_turbo_energy(p,s,context,a);break;
    case 0x40e7a0:engine_tick(p,s,context);break;
    case 0x40cb90:digital_throttle(s);break;
    case 0x40c220:analog_throttle(s);break;
    case 0x40cc90:digital_brake(p,s);break;
    case 0x40c320:analog_brake(p,s);break;
    case 0x414ac0:wheel_braking_signal(s,index);break;
    case 0x415630:brake_signal(s);break;
    case 0x40ce60:steering_boost(s);break;
    case 0x40ced0:digital_steering(p,s);break;
    case 0x40cf90:reversed_digital_steering(p,s);break;
    case 0x40d050:digital_manual_shift(p,s);break;
    case 0x40c6c0:analog_manual_shift(p,s);break;
    case 0x4143e0:vehicle_steer_increase(s.steering,p.steering_max,a);break;
    case 0x414420:vehicle_steer_decrease(s.steering,p.steering_max,a);break;
    case 0x4128f0:reset_camera_dependent_effects(s);break;
    case 0x418580:prepare_physical_parameters(p,s);break;
    case 0x414710:update_support_basis(s,support);break;
    case 0x406270:approach_body_speed(s);break;
    case 0x413330:damp_auxiliary_motion(s);break;
    case 0x42f1c0:{double out=random.signed_value(a);std::memcpy(b.data()+0x2a10,&out,8);break;}
    case 0x40ee70:suspension_pose(p,s);break;
    case 0x40efe0:suspension_tick(p,s,word(b,0x2a34),random);break;
    case 0x40f250:acceleration_tilt(p,s);break;
    case 0x40f760:support_rotation(support.body_position,support.current[index],a,s.incremental);break;
    case 0x40f880:wheel_rotation(s,support,index,real(b,0x2a44));break;
    case 0x40fbb0:bank_displacement(p,s,gravity);break;
    case 0x401d70:apply_gravity(s.velocity,gravity,a);break;
    case 0x40fc90:{VehicleDynamicsContext dynamics{context,real(b,0x2a08),real(b,0x2a44),gravity,0,word(b,0x2a34)};
        vehicle_dynamics_core(p,s,support,dynamics,random);break;}
    default:throw std::runtime_error("unknown vehicle oracle entry");
    }
    transfer(b,p,s,false);put_real(b,0x2a00,a);put_real(b,0x2a04,c);
    put(b,0x2a30,random.state);
}
void exact_read(std::ifstream& in,void* dest,std::size_t n) {
    if(!in.read(static_cast<char*>(dest),static_cast<std::streamsize>(n))) throw std::runtime_error("truncated vehicle oracle corpus");
}
std::uint32_t read_word(std::ifstream& in) {std::uint32_t x;exact_read(in,&x,4);return x;}
std::uint32_t ordered(std::uint32_t x) {return (x&0x80000000)?~x:x|0x80000000;}
int export_parameters(const char* root,const char* corpus) {
    sgn::AssetStore assets(root);const auto names=assets.resources("config/vehicles/");
    if(names.size()!=24)throw std::runtime_error("unexpected original vehicle config count");
    std::ofstream out(corpus,std::ios::binary);out.write("SGNCF001",8);
    auto write_word=[&](std::uint32_t x){out.write(reinterpret_cast<const char*>(&x),4);};
    write_word(static_cast<std::uint32_t>(names.size()));
    for(const auto& name:names) {
        const auto raw=assets.resource(name);
        auto p=decode_vehicle_parameters(std::string_view(reinterpret_cast<const char*>(raw.data()),raw.size()));
        VehicleState state;Record record{};transfer(record,p,state,false);
        write_word(static_cast<std::uint32_t>(name.size()));out.write(name.data(),static_cast<std::streamsize>(name.size()));
        write_word(static_cast<std::uint32_t>(raw.size()));out.write(reinterpret_cast<const char*>(raw.data()),static_cast<std::streamsize>(raw.size()));
        out.write(reinterpret_cast<const char*>(record.data()),static_cast<std::streamsize>(parameter_record_size));
    }
    if(!out)throw std::runtime_error("cannot write parameter corpus");
    std::cout<<"exported "<<names.size()<<" original vehicle configurations\n";return 0;
}
int parameters(const char* root,const char* corpus) {
    sgn::AssetStore assets(root);std::ifstream in(corpus,std::ios::binary);char magic[8];exact_read(in,magic,8);
    if(std::string(magic,8)!="SGNVC001")throw std::runtime_error("wrong parameter corpus schema");
    auto count=read_word(in);if(count==0 || count>100)throw std::runtime_error("invalid parameter record count");
    for(unsigned j=0;j<count;++j) {
        auto n=read_word(in);if(n>1024)throw std::runtime_error("parameter name too long");
        std::string name(n,'\0');exact_read(in,name.data(),n);Record expected{};exact_read(in,expected.data(),parameter_record_size);
        auto raw=assets.resource(name);auto p=decode_vehicle_parameters(std::string_view(reinterpret_cast<const char*>(raw.data()),raw.size()));
        auto actual=expected;VehicleState state;transfer(actual,p,state,false);
        if(actual!=expected) {
            for(std::size_t at=0;at<record_size;at+=4)if(word(actual,at)!=word(expected,at))
                std::cerr<<name<<" offset "<<std::hex<<at<<" expected "<<word(expected,at)<<" actual "<<word(actual,at)<<std::dec<<'\n';
            throw std::runtime_error("vehicle parameters differ from original loader");
        }
    }
    if(in.peek()!=std::char_traits<char>::eof())throw std::runtime_error("trailing parameter corpus bytes");
    std::cout<<"{\"passed\":true,\"original_vehicle_configs\":"<<count<<",\"comparison\":\"exact mapped parameter words\"}\n";return 0;
}
}
int main(int argc,char** argv) {
    try {
        if(argc==4 && std::string(argv[1])=="--export-configs")return export_parameters(argv[2],argv[3]);
        if(argc==4 && std::string(argv[1])=="--parameters")return parameters(argv[2],argv[3]);
        if(argc!=2) throw std::runtime_error("usage: vehicle_oracle corpus");
        std::ifstream in(argv[1],std::ios::binary);char magic[8];exact_read(in,magic,8);
        const auto schema=std::string(magic,8);
        if(schema!="SGNVH003" && schema!="SGNVH004") throw std::runtime_error("wrong vehicle oracle schema");
        auto count=read_word(in);std::uint64_t exact=0,undefined=0;unsigned errors=0;
        if(count==0 || count>100000)throw std::runtime_error("vehicle corpus case limit");
        for(unsigned n=0;n<count;++n) {
            auto entry=read_word(in),index=read_word(in);Record initial{},expected{};
            const auto bytes=schema=="SGNVH004"?read_word(in):vehicle_record_size;
            if(bytes!=vehicle_record_size && bytes!=record_size)throw std::runtime_error("invalid vehicle record size");
            if(index>=4 || (paired_entry(entry) && (index>=2 || bytes!=record_size)))
                throw std::runtime_error("invalid vehicle case index or paired record size");
            exact_read(in,initial.data(),bytes);exact_read(in,expected.data(),bytes);
            auto actual=initial;apply(entry,index,actual);
            for(std::size_t at=0;at<bytes;at+=4) {
                // 0044f9b4 copies an unwritten stack word into the first column w.
                // Only that undefined payload lane is outside semantic comparison.
                if(entry==0x44f8e0 && at==0x1bd8){++undefined;continue;}
                if(entry==0x40f330 && (at==0x1bd8 || at==0x1be8 || at==0x1bf8)){++undefined;continue;}
                auto x=word(actual,at),y=word(expected,at);
                if(x==y){++exact;continue;}
                const float a=std::bit_cast<float>(x),b=std::bit_cast<float>(y);
                const auto ax=ordered(x),bx=ordered(y),ulp=ax>bx?ax-bx:bx-ax;
                if(errors++<24) std::cerr<<"case "<<n<<" entry "<<std::hex<<entry<<" offset "<<at<<" expected "<<y<<" actual "<<x<<std::dec<<" ("<<b<<", "<<a<<"), ULP "<<ulp<<'\n';
            }
        }
        if(in.peek()!=std::char_traits<char>::eof())throw std::runtime_error("trailing corpus bytes");
        std::cout<<"{\"passed\":"<<(errors?"false":"true")<<",\"cases\":"<<count<<",\"exact_words\":"<<exact<<",\"undefined_payload_words\":"<<undefined<<",\"allowed_ulp\":0,\"errors\":"<<errors<<"}\n";
        return errors?1:0;
    } catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
