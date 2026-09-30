#include "sgn/retail_frontend_initialization.hpp"
#include "sgn/retail_frontend_state.hpp"
#include <algorithm>
#include <array>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
using namespace sgn::retail;
namespace {
void require(bool good,const char* why){if(!good)throw std::runtime_error(why);}
std::uint32_t word(std::istream& in){std::array<std::uint8_t,4> b{};in.read(reinterpret_cast<char*>(b.data()),4);require(bool(in),"truncated word");return persistence_word(b,0);}
void payload(std::istream& in,std::span<std::uint8_t> b){in.read(reinterpret_cast<char*>(b.data()),static_cast<std::streamsize>(b.size()));require(bool(in),"truncated208-byte payload");}
}
int main(int argc,char** argv)try{
    require(argc==2,"usage: navigation_initialization_oracle corpus.dat");std::ifstream in(argv[1],std::ios::binary);require(bool(in),"open corpus");
    std::array<char,8> magic{};in.read(magic.data(),8);require(std::string(magic.data(),8)=="SGNFNI01","schema mismatch");
    const auto count=word(in);require(count>0&&count<=10000,"case count bound");std::array<unsigned,3> counts{};
    auto f=std::make_unique<FrontendSourceStorage>();
    for(unsigned i=0;i<count;++i){
        const auto op=word(in);require(op==1||op==2,"operation domain");
        payload(in,f->source_bytes(0x5ff3e8,208));std::array<std::uint8_t,208> expected{};payload(in,expected);
        if(op==1)initialize_frontend_navigation_43dda0(*f);else clear_frontend_navigation_tail_43dee0(*f);
        const auto actual=f->source_bytes(0x5ff3e8,208);
        require(std::equal(actual.begin(),actual.end(),expected.begin()),"full208-byte state mismatch");++counts[op];
    }
    require(in.peek()==std::char_traits<char>::eof(),"trailing corpus bytes");
    std::cout<<"{\"passed\":true,\"cases\":"<<count<<",\"full_cases\":"<<counts[1]<<",\"tail_cases\":"<<counts[2]<<",\"compared_bytes\":"<<std::uint64_t(count)*208<<"}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
