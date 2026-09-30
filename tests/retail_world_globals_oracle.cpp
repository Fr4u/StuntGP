#include "sgn/retail_world_globals.hpp"
#include <cstring>
#include <fstream>
#include <iostream>

namespace {
using namespace sgn::retail;
constexpr unsigned size=0x120;
using Record=std::array<std::uint8_t,size>;
void read(std::ifstream& in,void* out,std::size_t count){if(!in.read(static_cast<char*>(out),static_cast<std::streamsize>(count)))throw std::runtime_error("short world globals corpus");}
unsigned word(std::ifstream& in){unsigned result;read(in,&result,4);return result;}
}
int main(int argc,char** argv){try{
    if(argc!=2)throw std::runtime_error("expected world globals corpus");std::ifstream in(argv[1],std::ios::binary);
    char magic[8];read(in,magic,8);if(std::string(magic,8)!="SGNWG001")throw std::runtime_error("world globals schema");
    const auto count=word(in);if(count==0||count>100000)throw std::runtime_error("world globals case count");
    static_assert(sizeof(WorldFrameGlobals)==224&&sizeof(WorldGravity)==44);
    unsigned errors=0;std::uint64_t compared=0;
    for(unsigned n=0;n<count;++n){const auto entry=word(in);Record actual,expected;read(in,actual.data(),size);read(in,expected.data(),size);
        WorldFrameGlobals frame;WorldGravity gravity;std::memcpy(&frame,actual.data(),224);std::memcpy(&gravity,actual.data()+224,44);
        if(entry==0x401de0)initialize_world_frame_globals(frame);
        else if(entry==0x441f90)initialize_world_gravity(gravity);
        else if(entry==3){for(unsigned repeat=0;repeat<5;++repeat){initialize_world_frame_globals(frame);initialize_world_gravity(gravity);}}
        else throw std::runtime_error("unknown world globals entry");
        std::memcpy(actual.data(),&frame,224);std::memcpy(actual.data()+224,&gravity,44);
        for(unsigned at=0;at<size;at+=4){++compared;unsigned a,b;std::memcpy(&a,actual.data()+at,4);std::memcpy(&b,expected.data()+at,4);
            if(a!=b&&errors++<16)std::cerr<<"case "<<n<<" offset "<<std::hex<<at<<" actual "<<a<<" expected "<<b<<std::dec<<'\n';}
    }
    if(in.peek()!=std::char_traits<char>::eof())throw std::runtime_error("trailing world globals corpus");
    std::cout<<"{\"passed\":"<<(errors?"false":"true")<<",\"cases\":"<<count<<",\"compared_words\":"<<compared<<",\"mismatches\":"<<errors<<",\"allowed_ulp\":0}\n";return errors?1:0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}}
