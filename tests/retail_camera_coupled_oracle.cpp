#include "retail_camera_state.hpp"
#include "sgn/retail_camera_collision.hpp"
#include "sgn/retail_camera_vehicle.hpp"
#include "sgn/vehicle.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
using namespace sgn::retail;
using namespace sgn::retail::camera_test;
void read(std::ifstream& in,void* data,std::size_t size) {
    in.read(static_cast<char*>(data),static_cast<std::streamsize>(size));
    if(!in)throw std::runtime_error("truncated coupled camera corpus");
}
std::uint32_t read_word(std::ifstream& in) {std::uint32_t x;read(in,&x,4);return x;}
void observe(CameraVehicle& v) {
    // Decode independently into the physical/race API, then use its real adapter.
    VehicleState state;VehicleParameters parameters;parameters.height=v.height;
    state.position={v.position.x,v.position.y,v.position.z,v.position.w};
    state.velocity={v.velocity.x,v.velocity.y,v.velocity.z};state.velocity_w=v.velocity.w;
    state.basis_x={v.right.x,v.right.y,v.right.z,v.right.w};
    state.up={v.up.x,v.up.y,v.up.z};state.basis_y_w=v.up.w;
    state.basis_z={v.forward.x,v.forward.y,v.forward.z,v.forward.w};
    state.orientation=v.orientation;state.speed=v.speed;
    state.contact_class=std::bit_cast<std::int32_t>(v.contact_class);
    state.contact_counter=std::bit_cast<std::int32_t>(v.contact_counter);
    state.airborne_ticks=v.contact_counter^0xffffffffu;
    state.auxiliary_displacement={v.shift.x,v.shift.y,v.shift.z,v.shift.w};
    state.previous_auxiliary_displacement={v.previous_shift.x,v.previous_shift.y,v.previous_shift.z,v.previous_shift.w};
    camera_observe_vehicle(v,parameters,state,{v.active,v.progress,v.contact_height});
}
}
int main(int argc,char** argv) {
    try {
        if(argc!=3)throw std::runtime_error("usage: coupled camera oracle retail-root corpus");
        std::ifstream in(argv[2],std::ios::binary);char header[8];read(in,header,8);
        if(std::memcmp(header,"SGNCPL01",8))throw std::runtime_error("wrong coupled camera corpus");
        std::uint64_t total=0,checked=0,mismatches=0;unsigned tracks=0;
        while(const auto length=read_word(in)) {
            if(length>256)throw std::runtime_error("invalid coupled camera mesh name length");
            std::string name(length,'\0');read(in,name.data(),length);
            if(name.find('/')!=name.npos||name.find('\\')!=name.npos)throw std::runtime_error("invalid mesh name");
            const auto track=contact::decode_track_collision(sgn::read_file(std::filesystem::path(argv[1])/"meshdata"/name));
            const auto count=read_word(in);if(!count||count>10000)throw std::runtime_error("invalid case count");
            ++tracks;
            for(unsigned n=0;n<count;++n) {
                const auto integrate=read_word(in);if(integrate>1)throw std::runtime_error("invalid integration selector");
                Bytes c(camera_size),v(vehicle_size),expected_c(camera_size),expected_v(vehicle_size);
                read(in,c.data(),c.size());read(in,v.data(),v.size());
                read(in,expected_c.data(),expected_c.size());read(in,expected_v.data(),expected_v.size());
                CameraMatrix shared_model_identity{}; //isolated fixture owner; old corpus does not compare61C468
                CameraState camera;CameraVehicle vehicle;transfer(c,v,camera,vehicle,true);observe(vehicle);
                camera_chase_tick(camera,vehicle,{real(v,0x2704),real(v,0x2708),real(v,0x270c)},
                    {real(v,0x2750),real(v,0x2754),real(v,0x2758),0},camera_collision_adapter(track),shared_model_identity);
                if(integrate) {camera_integrate_velocity(camera);camera_integrate_angles(camera);}
                transfer(c,v,camera,vehicle,false);++total;
                auto compare=[&](const Bytes& actual,const Bytes& expected,const char* label) {
                    for(std::size_t i=0;i<actual.size();i+=4) {
                        ++checked;if(word(actual,i)==word(expected,i))continue;
                        if(mismatches<20)std::cerr<<name<<" case="<<n<<" "<<label<<"+"<<std::hex<<i
                            <<" actual="<<word(actual,i)<<" expected="<<word(expected,i)<<std::dec<<'\n';
                        ++mismatches;
                    }
                };
                compare(c,expected_c,"camera");compare(v,expected_v,"vehicle");
            }
        }
        if(in.peek()!=std::char_traits<char>::eof()||!total)throw std::runtime_error("empty or trailing coupled camera corpus");
        std::cout<<"tracks="<<tracks<<" cases="<<total<<" words="<<checked<<" mismatches="<<mismatches<<" tolerance_ulp=0\n";
        return mismatches?1:0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
