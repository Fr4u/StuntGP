#include "sgn/retail_camera_transform.hpp"
#include <bit>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace sgn::retail;
namespace {
std::uint32_t read(std::ifstream& stream) {
    std::uint32_t value{};stream.read(reinterpret_cast<char*>(&value),4);
    if(!stream)throw std::runtime_error("truncated transform corpus");return value;
}
float real(std::ifstream& s) {return std::bit_cast<float>(read(s));}
CameraVector vector(std::ifstream& s) {return {real(s),real(s),real(s),real(s)};}
CameraMatrix matrix(std::ifstream& s) {CameraMatrix m;for(float& v:m)v=real(s);return m;}
CameraModelTransform transform(std::ifstream& s) {return {vector(s),vector(s),vector(s),matrix(s)};}
}
int main(int argc,char** argv) try {
    if(argc!=2)throw std::runtime_error("expected corpus path");
    std::ifstream stream(argv[1],std::ios::binary);char magic[8]{};stream.read(magic,8);
    if(std::string_view(magic,8)!="SGNVIEW1")throw std::runtime_error("wrong transform corpus");
    const auto cases=read(stream);if(cases==0 || cases>4096)throw std::runtime_error("invalid case count");
    std::size_t words{},mismatches{};
    for(std::uint32_t n=0;n<cases;++n) {
        CameraState camera;camera.position=vector(stream);camera.target=vector(stream);camera.up=vector(stream);
        const auto unused_camera_scale=vector(stream);(void)unused_camera_scale;camera.twist=real(stream);
        const auto model=transform(stream),part=transform(stream);const auto count=read(stream),has_part=read(stream);
        camera_view_basis(camera);
        const std::array<CameraMatrix,4> actual{camera.camera_to_world_rotation,camera.world_to_camera_rotation,
            camera_model_view(camera,model,count,has_part?&part:nullptr),camera_world_view(camera)};
        for(unsigned stage=0;stage<actual.size();++stage)for(unsigned i=0;i<16;++i) {
            const auto expected=read(stream),value=std::bit_cast<std::uint32_t>(actual[stage][i]);++words;
            if(value!=expected) {
                if(mismatches<12)std::cerr<<"case="<<n<<" stage="<<stage<<" index="<<i<<" actual="<<std::hex<<value<<" expected="<<expected<<std::dec<<'\n';
                ++mismatches;
            }
        }
    }
    if(stream.peek()!=std::char_traits<char>::eof())throw std::runtime_error("trailing transform corpus data");
    std::cout<<"cases="<<cases<<" words="<<words<<" mismatches="<<mismatches<<" tolerance_ulp=0\n";
    return mismatches?1:0;
}catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 2;}
