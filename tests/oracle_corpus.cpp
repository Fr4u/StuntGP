#include "sgn/assets.hpp"
#include <fstream>
#include <iostream>
using namespace sgn;
namespace {
void u32(std::ofstream& out,std::uint32_t n){for(unsigned i=0;i<4;++i)out.put(static_cast<char>((n>>(i*8))&255));}
}
int main(int argc,char** argv){try{
    if(argc!=3)throw std::runtime_error("usage: oracle_corpus retail-root output.dat");
    std::ofstream out(argv[2],std::ios::binary);out.write("SGNORC01",8);unsigned count=0;
    std::vector<std::filesystem::path> paths;for(auto& e:std::filesystem::directory_iterator(std::filesystem::path(argv[1])/"wads"))paths.push_back(e.path());std::sort(paths.begin(),paths.end());
    for(const auto& p:paths){Archive archive(read_file(p));for(const auto& e:archive.entries()){
        auto raw=archive.data(e);if(raw.size()<4 || raw[0]!='T'||raw[1]!='M'||raw[2]!='!'||raw[3]!=26)continue;
        auto im=decode_tm(raw);auto count_pixels=im.width*im.height;
        std::string name=p.filename().string()+"/"+e.name;u32(out,static_cast<std::uint32_t>(name.size()));out.write(name.data(),static_cast<std::streamsize>(name.size()));u32(out,static_cast<std::uint32_t>(raw.size()));out.write(reinterpret_cast<const char*>(raw.data()),static_cast<std::streamsize>(raw.size()));u32(out,count_pixels);
        for(std::size_t i=0;i<count_pixels;++i){unsigned v=im.rgba[i*4+3]?0x8000:0;for(unsigned c=0;c<3;++c)v|=((unsigned(im.rgba[i*4+c])*31+127)/255)<<(10-c*5);out.put(static_cast<char>(v&255));out.put(static_cast<char>(v>>8));}++count;
    }}
    u32(out,0);if(!out)throw std::runtime_error("corpus write failed");std::cout<<"exported_tm_images="<<count<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
