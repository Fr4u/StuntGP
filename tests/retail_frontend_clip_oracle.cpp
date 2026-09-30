#include "sgn/retail_frontend_clip.hpp"
#include "sgn/retail_frontend_state.hpp"
#include "sgn/retail_frontend_initialization.hpp"
#include "sgn/retail_world_material_loading.hpp"
#include <array>
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>
using namespace sgn::retail;
namespace {
void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
std::uint32_t word(std::istream& in){std::array<std::uint8_t,4> v{};in.read(reinterpret_cast<char*>(v.data()),4);require(bool(in),"truncated corpus word");return persistence_word(v,0);}
std::vector<std::uint8_t> bytes(std::istream& in,std::size_t n){std::vector<std::uint8_t> v(n);if(n)in.read(reinterpret_cast<char*>(v.data()),static_cast<std::streamsize>(n));require(bool(in),"truncated corpus image");return v;}
template<class T> void transfer(T& value,std::vector<std::uint8_t>& image,std::size_t& offset,bool input){
    require(offset+sizeof(value)<=image.size(),"owner exceeds corpus image");
    if(input)std::memcpy(&value,image.data()+offset,sizeof(value));else std::memcpy(image.data()+offset,&value,sizeof(value));offset+=sizeof(value);
}
template<class T,std::size_t N> void transfer(std::span<T,N>,std::vector<std::uint8_t>&,std::size_t&,bool)=delete;
template<class T> concept ObjectTransferable=requires(T& value,std::vector<std::uint8_t>& image,std::size_t& at){transfer(value,image,at,true);};
static_assert(!ObjectTransferable<std::span<std::uint8_t,92>> && ObjectTransferable<std::array<std::uint8_t,92>>);
void transfer_payload(std::span<std::uint8_t> payload,std::vector<std::uint8_t>& image,std::size_t& offset,bool input){
    require(offset<=image.size()&&payload.size()<=image.size()-offset,"payload exceeds corpus image");
    if(input)std::copy_n(image.data()+offset,payload.size(),payload.data());
    else std::copy(payload.begin(),payload.end(),image.begin()+static_cast<std::ptrdiff_t>(offset));
    offset+=payload.size();
}
void transfer_owners(FrontendSourceStorage& f,std::vector<std::uint8_t>& image,bool input){
    std::size_t at=0;
    transfer(f.frontend_descriptors,image,at,input);transfer(f.lookup_a,image,at,input);transfer(f.lookup_b,image,at,input);transfer(f.lookup_c,image,at,input);
    transfer(f.frontend_clip,image,at,input);transfer(f.menu_counter_5da370,image,at,input);transfer(f.menu_words_5df194,image,at,input);
    transfer_payload(f.page_navigation(),image,at,input);transfer(f.menu_music_word(),image,at,input);require(at==image.size(),"owner image length");
}
}
int main(int argc,char** argv)try{
    require(argc==2,"usage: retail_frontend_clip_oracle corpus.dat");std::ifstream in(argv[1],std::ios::binary);require(bool(in),"open corpus");
    const auto magic=bytes(in,8);require(std::string(magic.begin(),magic.end())=="SGNFCL01","wrong corpus schema");
    const auto count=word(in);require(count>0&&count<=100000,"corpus count bound");std::array<unsigned,5> counts{};std::uint64_t compared=0;
    auto f=std::make_unique<FrontendSourceStorage>();
    for(unsigned index=0;index<count;++index){
        const auto op=word(in),cw=word(in);std::array<std::uint32_t,4> args{};for(auto& v:args)v=word(in);
        const std::uint64_t lo=word(in),hi=word(in),expected_value=lo|(hi<<32);const auto size=word(in);
        require(op>=1&&op<=4&&cw<=0xffff&&(cw&0x3f)==0x3f&&(cw&0xf080)==0&&(cw&0x300)!=0x100,"operation/source control-word domain");
        require(size==(op==1?0u:op==2?16u:20832u),"corpus operation image length");
        auto actual=bytes(in,size);const auto expected=bytes(in,size);std::uint64_t value=0;
        if(op==1){require(args[1]==0&&args[2]==0&&args[3]==0,"ftol unused input lanes");value=frontend_ftol_f32_462860(args[0]);}
        else if(op==2){std::memcpy(f->frontend_clip.data(),actual.data(),16);set_frontend_clip_438190(*f,args);std::memcpy(actual.data(),f->frontend_clip.data(),16);}
        else{
            require(args==std::array<std::uint32_t,4>{},"composition input lanes");transfer_owners(*f,actual,true);
            if(op==3)initialize_frontend_materials_441190(*f,source_world_material_prelude(2));else initialize_frontend_drawing_437e60(*f);
            transfer_owners(*f,actual,false);
        }
        if(value!=expected_value||actual!=expected)throw std::runtime_error("corpus mismatch at case "+std::to_string(index)+", op "+std::to_string(op)+", input "+std::to_string(args[0]));
        compared+=size+(op==1?8:0);++counts[op];
    }
    require(in.peek()==std::char_traits<char>::eof(),"trailing corpus data");
    std::cout<<"{\"passed\":true,\"cases\":"<<count<<",\"ftol_cases\":"<<counts[1]<<",\"clip_cases\":"<<counts[2]
        <<",\"material_cases\":"<<counts[3]<<",\"drawing_cases\":"<<counts[4]<<",\"compared_bytes\":"<<compared<<"}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
