#include "sgn/source_numeric_control.hpp"
#include <array>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <vector>

using namespace sgn::retail;
namespace {
void require(bool value,const char* why){if(!value)throw std::runtime_error(why);}
template<class T>void read(std::ifstream& input,T& value){
    if(!input.read(reinterpret_cast<char*>(&value),sizeof(value)))throw std::runtime_error("short numeric corpus");
}
// Explicit logical instruction receiver for core tests. This never executes
// original or native x87 instructions and is not a native hardware oracle.
struct Words final:SourceX87Control {
    std::uint16_t current{};
    std::vector<std::uint16_t> writes;
    unsigned calls{},fail_at{};
    void event(){if(++calls==fail_at)throw std::runtime_error("receiving stop");}
    std::uint16_t fstcw() override{event();return current;}
    void fldcw(std::uint16_t word) override{event();writes.push_back(word);current=word;}
};
}
int main(int argc,char** argv)try{
    require(argc==2,"expected numeric corpus path");
    std::ifstream input(argv[1],std::ios::binary);std::array<char,8> magic{};read(input,magic);
    require(std::string_view(magic.data(),8)=="SGNNUM01","numeric corpus schema");
    std::uint32_t count{};read(input,count);require(count>0&&count<=1000000,"numeric corpus count");
    for(std::uint32_t i=0;i<count;++i){
        std::array<std::uint32_t,7> r{};read(input,r);
        if(r[0]==1)require(decode_source_x87_control_46308c(r[1])==r[4],"decode differs");
        else if(r[0]==2)require(encode_source_x87_control_46311e(r[1])==r[4],"encode differs");
        else if(r[0]==3){
            require(r[1]<=0xffffu,"invalid native control input");Words device;device.current=static_cast<std::uint16_t>(r[1]);
            const auto result=update_source_x87_control_463041(device,r[2],r[3]);
            require(result==r[4]&&device.current==r[5]&&device.calls==r[6]&&device.writes.size()==1,"read/merge/write/return differs");
        }else if(r[0]==4){
            require(r[1]<=0xffffu,"invalid native control input");Words device;device.current=static_cast<std::uint16_t>(r[1]);
            prepare_source_numeric_429510(device);
            require(device.calls==4&&device.writes.size()==2&&device.writes[0]==r[4]&&device.writes[1]==r[5],"429510 two-call sequence differs");
        }else if(r[0]==5){
            require(r[2]<=0xffffu,"invalid native control input");
            require(source_compatible_mxcsr(r[1],static_cast<std::uint16_t>(r[2]))==r[4],"new SSE policy differs");
        }else throw std::runtime_error("unknown numeric corpus operation");
    }
    require(input.peek()==std::ifstream::traits_type::eof(),"trailing numeric corpus bytes");
    for(unsigned stage=1;stage<=4;++stage){
        Words device;device.current=0x037fu;device.fail_at=stage;bool stopped=false;
        try{prepare_source_numeric_429510(device);}catch(const std::runtime_error&){stopped=true;}
        require(stopped&&device.calls==stage,"failure continued to later numeric child");
        require(device.writes.size()==(stage>2?1u:0u),"numeric stop rolled back or fabricated a write");
    }
    std::cout<<"{\"passed\":true,\"data_cases\":"<<count<<",\"stop_cases\":4,\"scope\":\"integer translation and instruction-boundary sequence only\",\"native_or_original_executed\":false}\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
