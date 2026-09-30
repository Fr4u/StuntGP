#include "sgn/vehicle_configuration_identity.hpp"
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {
void read(std::istream& input,void* data,std::size_t size){
    if(!input.read(static_cast<char*>(data),static_cast<std::streamsize>(size)))
        throw std::runtime_error("truncated identity corpus");
}
std::uint32_t word(std::istream& input){std::uint32_t value;read(input,&value,sizeof(value));return value;}
}
int main(int argc,char** argv){
    try{
        if(argc!=2)throw std::runtime_error("identity oracle needs one corpus");
        std::ifstream input(argv[1],std::ios::binary);
        std::array<char,8> magic{};read(input,magic.data(),magic.size());
        if(std::string_view(magic.data(),magic.size())!="SGNIDEN1")throw std::runtime_error("identity corpus magic");
        const auto count=word(input);if(count==0||count>1024)throw std::runtime_error("identity corpus count");
        std::uint64_t compared=0;
        for(std::uint32_t i=0;i<count;++i){
            std::array<sgn::retail::VehicleConfigurationIdentity,8> identities{},expected{};
            for(auto& identity:identities)read(input,identity.header.data(),identity.header.size());
            for(auto& identity:expected)read(input,identity.header.data(),identity.header.size());
            sgn::retail::initialize_cold_configuration_identity(identities);
            for(std::size_t slot=0;slot<identities.size();++slot){
                if(identities[slot].header!=expected[slot].header)throw std::runtime_error("source identity mismatch");
                compared+=identities[slot].header.size();
            }
        }
        if(input.peek()!=std::char_traits<char>::eof())throw std::runtime_error("identity corpus trailing bytes");
        std::cout<<"{\"passed\":true,\"cases\":"<<count<<",\"bytes\":"<<compared<<",\"mismatches\":0}\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
