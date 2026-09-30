// Consumes an immutable corpus from unpatched original x86 execution by Main.
// Does not generate expected results. This reader itself executes only C++ CPU.
#include "sgn/retail_frontend_descriptors.hpp"
#include "sgn/retail_frontend_state.hpp"
#include "sgn/retail_world_material_loading.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

using namespace sgn::retail;
namespace {
void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
std::uint32_t word(std::istream& in){std::array<std::uint8_t,4> v{};in.read(reinterpret_cast<char*>(v.data()),4);require(bool(in),"truncated word");return persistence_word(v,0);}
std::vector<std::uint8_t> bytes(std::istream& in,std::size_t n){std::vector<std::uint8_t> v(n);if(n)in.read(reinterpret_cast<char*>(v.data()),static_cast<std::streamsize>(n));require(bool(in),"truncated image");return v;}
template<std::size_t N> void transfer(std::array<std::uint32_t,N>& owner,std::vector<std::uint8_t>& image,std::size_t& at,bool input){
    const auto n=sizeof(owner);require(at+n<=image.size(),"owner outside image");
    if(input)std::memcpy(owner.data(),image.data()+at,n);else std::memcpy(image.data()+at,owner.data(),n);at+=n;
}
}
int main(int argc,char** argv)try{
    require(argc==2,"usage: retail_frontend_descriptors_oracle corpus.dat");
    std::ifstream in(argv[1],std::ios::binary);require(bool(in),"cannot open corpus");
    const auto magic=bytes(in,8);require(std::string(magic.begin(),magic.end())=="SGNFD001","corpus schema");
    const auto count=word(in);require(count>0 && count<=5000,"corpus count outside bounded domain");
    std::array<unsigned,5> counts{};std::uint64_t compared=0;
    for(std::uint32_t i=0;i<count;++i){
        const auto op=word(in),input=word(in),alignment=word(in),result=word(in),size=word(in),extra=word(in);
        require(op>=1&&op<=4&&size<=20084,"corpus operation/size");
        auto actual=bytes(in,size);const auto expected=bytes(in,size);std::uint32_t got=0;
        if(op==1){require(size==80&&alignment<4&&extra==0,"row shape");got=initialize_frontend_descriptor_441380(FrontendDescriptor{actual.data()+8+alignment,48},input);}
        else if(op==2){require(size==18584&&alignment==0&&input==0&&extra==386&&result==0,"bank shape");initialize_frontend_descriptors_441340(FrontendDescriptorBank{actual.data()+4,frontend_descriptor_bytes});}
        else if(op==3){
            require(size==20084&&alignment==0&&input==0&&extra==301&&result==0,"prefix shape");
            auto f=std::make_unique<FrontendSourceStorage>();auto bank=frontend_descriptor_bank(*f);
            std::copy_n(actual.begin()+4,bank.size(),bank.begin());std::size_t at=18584;
            transfer(f->lookup_a,actual,at,true);transfer(f->lookup_b,actual,at,true);transfer(f->lookup_c,actual,at,true);require(at==size,"prefix input image end");
            const auto stop=initialize_frontend_material_prefix_441190(*f,source_world_material_prelude(2));
            require(stop.stop==FrontendInitializationStop::before_4411c0&&stop.prelude_entries==extra,"prefix boundary/count");
            std::copy(bank.begin(),bank.end(),actual.begin()+4);at=18584;
            transfer(f->lookup_a,actual,at,false);transfer(f->lookup_b,actual,at,false);transfer(f->lookup_c,actual,at,false);
        }else{require(size==0&&alignment==0&&extra==0,"absolute shape");got=frontend_descriptor_absolute_43d9b0(input);}
        if(got!=result||actual!=expected)throw std::runtime_error("original/C++ mismatch at case "+std::to_string(i)+", op "+std::to_string(op)+", input "+std::to_string(input));
        ++counts[op];compared+=size;
    }
    require(in.peek()==std::char_traits<char>::eof(),"trailing corpus bytes");
    std::cout<<"{\"passed\":true,\"cases\":"<<count<<",\"row_cases\":"<<counts[1]<<",\"bank_cases\":"<<counts[2]
        <<",\"prefix_cases\":"<<counts[3]<<",\"absolute_cases\":"<<counts[4]<<",\"compared_bytes\":"<<compared<<"}\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
