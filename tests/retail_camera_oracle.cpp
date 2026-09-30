#include "retail_camera_state.hpp"
#include "sgn/retail_camera_vehicle.hpp"
#include "sgn/vehicle.hpp"
#include <bit>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace sgn::retail;
using namespace sgn::retail::camera_test;
struct ResetFixture {
    Bytes& data;
    std::array<VehicleState,6> vehicles;
    CameraEffectTargets targets;
    CameraModeReset actual;
    explicit ResetFixture(Bytes& v):data(v) {
        for(std::size_t i=0;i<6;++i) {
            targets.vehicles[i]=&vehicles[i];targets.active[i]=word(v,0x27a0+i*4);
            for(std::size_t j=0;j<8;++j)vehicles[i].camera_effects.values[j]=word(v,0x2800+i*32+j*4);
        }
        actual=camera_vehicle_reset_adapter(targets);
    }
    CameraModeReset adapter() {
        return {this,actual.active,[](void* opaque,std::uint32_t slot) noexcept {
            auto& self=*static_cast<ResetFixture*>(opaque);auto& bytes=self.data;
            put(bytes,0x27c0,(word(bytes,0x27c0)^slot)*16777619u);
            put(bytes,0x27c4,word(bytes,0x27c4)+1);
            self.actual.reset(self.actual.context,slot);
        }};
    }
    void store() {
        for(std::size_t i=0;i<6;++i)for(std::size_t j=0;j<8;++j)
            put(data,0x2800+i*32+j*4,vehicles[i].camera_effects.values[j]);
    }
};
void apply(std::uint32_t entry,Bytes& c,Bytes& v) {
    CameraMatrix shared_model_identity{}; //isolated fixture owner; old corpus does not compare61C468
    CameraState s;CameraVehicle vehicle;transfer(c,v,s,vehicle,true);
    const float argument=real(v,0x2700);
    auto selected=word(v,0x2790);
    ResetFixture effects(v);const auto reset=effects.adapter();
    switch(entry) {
    case 0x4031e0:camera_orbit_angle(vehicle,argument);break;
    case 0x4032a0:camera_orbit_offset(vehicle);break;
    case 0x403310:camera_set_lens(s,argument,shared_model_identity);break;
    case 0x403340:camera_translate(s,vehicle);break;
    case 0x403420:camera_twist(s);break;
    case 0x403460:camera_move(s,vehicle,{});break;
    case 0x403650:camera_settle(s,word(v,0x2700));break;
    case 0x403770:camera_track_target(s,vehicle);break;
    case 0x403bc0:camera_speed_lens(s,vehicle,shared_model_identity);break;
    case 0x403e80:camera_finish_orbit(s,vehicle);break;
    case 0x402ab0:camera_request_lens(s,argument);break;
    case 0x402a80:camera_consume_lens(s,shared_model_identity);break;
    case 0x403080:camera_integrate_velocity(s);break;
    case 0x402f60:camera_integrate_angles(s);break;
    case 0x4028d0:camera_select_mode(s,word(v,0x2700),reset);break;
    case 0x40d200:camera_apply_preset(s,vehicle,2,selected,reset,shared_model_identity);break;
    case 0x40d280:camera_apply_preset(s,vehicle,1,selected,reset,shared_model_identity);break;
    case 0x40d300:camera_apply_preset(s,vehicle,3,selected,reset,shared_model_identity);break;
    case 0x40d380:camera_apply_preset(s,vehicle,4,selected,reset,shared_model_identity);break;
    case 0x404310:camera_restore_preset(s,vehicle,selected,word(v,0x2794),reset,shared_model_identity);break;
    case 0x4039b0:camera_prepare_follow_mode(s,vehicle,selected,reset,shared_model_identity);break;
    case 0x404360:camera_adaptive_distance(vehicle,{real(v,0x2704),real(v,0x2708),real(v,0x270c)});break;
    case 0x404450:camera_chase_tick(s,vehicle,{real(v,0x2704),real(v,0x2708),real(v,0x270c)},
        {real(v,0x2750),real(v,0x2754),real(v,0x2758),0},{},shared_model_identity);break;
    case 0x44f4e0:camera_projection(s);break;
    case 0x44ec00:vehicle.orbit_offset=camera_rotate_degrees(vehicle.angles,vehicle.orbit_offset);break;
    case 0x44ed40:camera_normalize(vehicle.orbit_offset);break;
    case 0x44fb00:s.projection=camera_rotation(std::bit_cast<std::int32_t>(word(v,0x2700)),std::bit_cast<std::int32_t>(word(v,0x2704)),std::bit_cast<std::int32_t>(word(v,0x2708)));break;
    case 0x450190:camera_view_basis(s);break;
    default:throw std::runtime_error("unknown camera oracle entry");
    }
    put(v,0x2790,selected);
    effects.store();
    transfer(c,v,s,vehicle,false);
}
void read(std::ifstream& in,void* data,std::size_t size) {
    in.read(static_cast<char*>(data),static_cast<std::streamsize>(size));
    if(!in)throw std::runtime_error("truncated camera corpus");
}
void contracts() {
    CameraMatrix shared_model_identity{}; //contract fixture owner
    CameraState s;CameraVehicle v;s.mode=5;
    bool rejected=false;
    try { camera_move(s,v,{}); } catch(const std::logic_error&) { rejected=true; }
    if(!rejected)throw std::runtime_error("missing camera collision was silently accepted");
    s.mode=6;camera_move(s,v,{});
    s.mode=12;s.twist=2;s.collision_bypass_ticks=11;
    CameraModeReset missing;missing.active[4]=1;rejected=false;
    try { camera_select_mode(s,5,missing); } catch(const std::logic_error&) { rejected=true; }
    if(!rejected || s.mode!=12 || s.twist!=2 || s.collision_bypass_ticks!=11)
        throw std::runtime_error("missing mode reset was accepted or partially changed camera");
    v.distance=1234;v.camera_mode=99;std::uint32_t selected=3;rejected=false;
    try { camera_apply_preset(s,v,1,selected,missing,shared_model_identity); } catch(const std::logic_error&) { rejected=true; }
    if(!rejected || v.distance!=1234 || v.camera_mode!=99 || selected!=3 || s.mode!=12)
        throw std::runtime_error("missing preset reset was accepted or partially changed state");
    missing.active.fill(2);camera_select_mode(s,4,missing);
    if(s.mode!=4)throw std::runtime_error("non-active slots incorrectly required reset callback");
    CameraEffectTargets targets;targets.active[2]=1;rejected=false;
    try { (void)camera_vehicle_reset_adapter(targets); } catch(const std::invalid_argument&) { rejected=true; }
    if(!rejected)throw std::runtime_error("missing active vehicle pointer accepted");
    VehicleState physical;VehicleParameters parameters;parameters.height=173;
    physical.contact_counter=-1;physical.airborne_ticks=2;physical.contact_class=2;
    physical.auxiliary_displacement={1,2,3,4};v.distance=1350;v.angles={6,200,0,9};
    physical.previous_auxiliary_displacement={5,6,7,8};
    camera_observe_vehicle(v,parameters,physical,{1,100,250});
    if(v.contact_counter!=0xffffffffu || v.contact_class!=2 || v.height!=173 ||
       v.shift.x!=1 || v.shift.w!=4 || v.previous_shift.x!=5 || v.previous_shift.w!=8 ||
       v.distance!=1350 || v.angles.x!=6 || v.angles.y!=200 || v.angles.w!=9)
        throw std::runtime_error("physical camera observation confused counters, history, or preset ownership");
}
}
int main(int argc,char** argv) {
    try {
        contracts();
        if(argc!=2)throw std::runtime_error("usage: retail_camera_oracle corpus.dat");
        std::ifstream in(argv[1],std::ios::binary);
        char signature[8];read(in,signature,8);
        if(std::memcmp(signature,"SGNCAM02",8)!=0)throw std::runtime_error("wrong camera corpus");
        std::uint32_t count;read(in,&count,4);
        if(count==0 || count>100000)throw std::runtime_error("camera corpus count outside limit");
        std::uint64_t checked=0,mismatches=0;
        for(std::uint32_t n=0;n<count;++n) {
            std::uint32_t entry;read(in,&entry,4);
            Bytes c(camera_size),v(vehicle_size),expected_c(camera_size),expected_v(vehicle_size);
            read(in,c.data(),c.size());read(in,v.data(),v.size());
            read(in,expected_c.data(),expected_c.size());read(in,expected_v.data(),expected_v.size());
            apply(entry,c,v);
            auto compare=[&](const Bytes& actual,const Bytes& expected,const char* label) {
                for(std::size_t i=0;i<actual.size();i+=4) {
                    ++checked;
                    if(word(actual,i)!=word(expected,i)) {
                        if(mismatches<20)std::cerr<<"case "<<n<<" entry "<<std::hex<<entry<<" "<<label<<"+"<<i<<" actual="<<word(actual,i)<<" expected="<<word(expected,i)<<std::dec<<" ("<<real(actual,i)<<", "<<real(expected,i)<<")\n";
                        ++mismatches;
                    }
                }
            };
            compare(c,expected_c,"camera");compare(v,expected_v,"vehicle");
        }
        if(in.peek()!=std::char_traits<char>::eof())throw std::runtime_error("trailing camera corpus bytes");
        std::cout<<"cases="<<count<<" words="<<checked<<" mismatches="<<mismatches<<" tolerance_ulp=0\n";
        return mismatches?1:0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n';return 1; }
}
