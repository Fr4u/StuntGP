#include "sgn/contact_config.hpp"
#include "sgn/assets.hpp"
#include <bit>
#include <fstream>
#include <iostream>

namespace {
void read(std::ifstream& in,void* output,std::size_t size){if(!in.read(static_cast<char*>(output),static_cast<std::streamsize>(size)))throw std::runtime_error("short contact config corpus");}
unsigned word(std::ifstream& in){unsigned value;read(in,&value,4);return value;}
}
int main(int argc,char** argv){
    try{
        if(argc!=2)throw std::runtime_error("expected contact config corpus");
        std::ifstream in(argv[1],std::ios::binary);char magic[8];read(in,magic,8);if(std::string(magic,8)!="SGNCC001")throw std::runtime_error("contact config corpus schema");
        const auto count=word(in);if(count==0||count>10000)throw std::runtime_error("contact config corpus count");
        unsigned errors=0,exact=0,rejected=0;
        for(unsigned n=0;n<count;++n){const auto invalid=word(in),size=word(in);if(size>1024*1024+1)throw std::runtime_error("contact config fixture size");
            std::string text(size,'\0');read(in,text.data(),size);std::array<std::uint32_t,128> expected{};if(!invalid)read(in,expected.data(),512);
            try{const auto result=sgn::retail::contact::decode_contact_restitution(text);if(invalid){++errors;std::cerr<<"accepted invalid case "<<n<<'\n';continue;}
                for(unsigned j=0;j<128;++j){if(std::bit_cast<std::uint32_t>(result[j])==expected[j])++exact;
                    else{if(errors++<16)std::cerr<<"case "<<n<<" sample "<<j<<" differs\n";}}
            }catch(const sgn::FormatError&){if(invalid)++rejected;else{++errors;std::cerr<<"rejected valid case "<<n<<'\n';}}
        }
        if(in.peek()!=std::char_traits<char>::eof())throw std::runtime_error("trailing contact config corpus");
        std::cout<<"{\"cases\":"<<count<<",\"exact_words\":"<<exact<<",\"rejected_invalid\":"<<rejected<<",\"errors\":"<<errors<<",\"allowed_ulp\":0}\n";return errors?1:0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
