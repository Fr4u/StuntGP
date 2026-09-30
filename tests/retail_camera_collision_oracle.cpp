#include "sgn/retail_camera_collision.hpp"
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
void read(std::ifstream& in,void* data,std::size_t size) {
    in.read(static_cast<char*>(data),static_cast<std::streamsize>(size));
    if(!in)throw std::runtime_error("short camera collision corpus");
}
std::uint32_t word(std::ifstream& in) { std::uint32_t result;read(in,&result,4);return result; }
}
int main(int argc,char** argv) {
    using namespace sgn::retail;
    try {
        if(argc!=3)throw std::runtime_error("usage: camera collision oracle retail-root corpus");
        std::ifstream in(argv[2],std::ios::binary);char header[8];read(in,header,8);
        if(std::memcmp(header,"SGNCC001",8))throw std::runtime_error("wrong camera collision corpus");
        unsigned total=0,failed=0,hits=0,tracks=0;
        while(const auto length=word(in)) {
            if(length>256)throw std::runtime_error("camera collision name too long");
            std::string name(length,'\0');read(in,name.data(),length);
            if(name.find('/')!=name.npos||name.find('\\')!=name.npos)throw std::runtime_error("invalid camera mesh name");
            const auto track=contact::decode_track_collision(sgn::read_file(std::filesystem::path(argv[1])/"meshdata"/name));
            const auto count=word(in);if(count>10000)throw std::runtime_error("too many camera collision cases");
            ++tracks;
            for(unsigned n=0;n<count;++n) {
                const auto kind=word(in);CameraSweep sweep;
                read(in,&sweep.position,16);read(in,&sweep.velocity,16);read(in,&sweep.radius,4);
                sweep.radius_squared=static_cast<float>(double(sweep.radius)*sweep.radius);
                const auto expected_hit=word(in);CameraVector expected;read(in,&expected,16);
                CameraVector actual;bool hit=false;
                if(kind==0) { const auto result=camera_sweep(track,sweep);actual=result.position;hit=result.hit; }
                else if(kind==1) { CameraState s;CameraVehicle v;s.mode=5;s.position=sweep.position;s.velocity=sweep.velocity;v.position=s.position;
                    camera_move(s,v,camera_collision_adapter(track));actual=s.position; }
                else throw std::runtime_error("invalid camera collision operation");
                ++total;hits+=hit;
                if(hit!=bool(expected_hit)||std::memcmp(&actual,&expected,16)) {
                    if(failed<15)std::cerr<<name<<" case="<<n<<" kind="<<kind<<" hit="<<hit<<"/"<<expected_hit
                        <<" actual="<<actual.x<<","<<actual.y<<","<<actual.z<<" expected="<<expected.x<<","<<expected.y<<","<<expected.z<<'\n';
                    ++failed;
                }
            }
        }
        if(in.peek()!=std::char_traits<char>::eof())throw std::runtime_error("trailing camera collision data");
        if(!total)throw std::runtime_error("empty camera collision corpus");
        std::cout<<"tracks="<<tracks<<" cases="<<total<<" hits="<<hits<<" failed="<<failed<<" tolerance_ulp=0\n";
        return failed?1:0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
