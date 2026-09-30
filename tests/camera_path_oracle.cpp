#include "sgn/camera_path.hpp"
#include <bit>
#include <fstream>
#include <iostream>
#include <cstring>
using namespace sgn::retail;
namespace {
void read(std::ifstream& f,void* p,std::size_t n) {f.read(static_cast<char*>(p),static_cast<std::streamsize>(n));if(!f)throw sgn::FormatError("truncated camera path corpus");}
std::uint32_t word(std::ifstream& f) {std::uint32_t x;read(f,&x,4);return x;}
float real(std::ifstream& f) {return std::bit_cast<float>(word(f));}
}
int main(int argc,char** argv) try {
    if(argc!=2)throw sgn::FormatError("expected camera path corpus");std::ifstream f(argv[1],std::ios::binary);
    char magic[8];read(f,magic,8);if(std::memcmp(magic,"SGNPATH1",8))throw sgn::FormatError("wrong path corpus");
    const auto count=word(f);std::size_t words=0,mismatches=0;
    for(std::uint32_t n=0;n<count;++n) {
        const auto kind=word(f),points=word(f),type=word(f),loop=word(f),input=word(f);
        float parameter=real(f),speed=real(f);if(points==0||points>100000)throw sgn::FormatError("invalid point count");
        sgn::Bytes storage(std::size_t(points)*16);read(f,storage.data(),storage.size());
        SplineView spline{{1,0,points,{type,loop,0},1,0},storage,storage.size()};
        sgn::Vec3 result{11,22,33};
        if(kind==0)result=camera_sample_path(spline,parameter).position;
        else if(kind==1)camera_advance_path(parameter,speed,{bool(input&1),bool(input&2)});
        else throw sgn::FormatError("unknown path case");
        const std::array<float,5> actual{result.x,result.y,result.z,parameter,speed};
        for(unsigned i=0;i<actual.size();++i) {
            const auto expected=word(f),value=std::bit_cast<std::uint32_t>(actual[i]);++words;
            if(expected!=value){if(mismatches<16)std::cerr<<"case="<<n<<" component="<<i<<" actual="<<std::hex<<value<<" expected="<<expected<<std::dec<<'\n';++mismatches;}
        }
    }
    if(f.peek()!=std::char_traits<char>::eof())throw sgn::FormatError("trailing path corpus");
    std::cout<<"cases="<<count<<" words="<<words<<" mismatches="<<mismatches<<" tolerance_ulp=0\n";return mismatches?1:0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}
