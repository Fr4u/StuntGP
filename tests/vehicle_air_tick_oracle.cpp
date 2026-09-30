#include "vehicle_air_fixture.hpp"
#include <fstream>
#include <iostream>

int main(int argc,char** argv) {try {
    using namespace sgn::retail;using namespace air_oracle;
    if(argc!=2)throw std::runtime_error("expected air corpus");std::ifstream in(argv[1],std::ios::binary);
    const auto read=[&](void* out,std::size_t bytes){if(!in.read(static_cast<char*>(out),static_cast<std::streamsize>(bytes)))throw std::runtime_error("short air corpus");};
    const auto word=[&](){unsigned value;read(&value,4);return value;};
    char magic[8];read(magic,8);if(std::string(magic,8)!="SGNAT001")throw std::runtime_error("air corpus schema");
    const auto count=word();if(count==0 || count>20000)throw std::runtime_error("air corpus count");
    std::uint64_t errors=0,exact=0,excluded=0,retained=0;
    for(unsigned n=0;n<count;++n) {
        if(word()!=0x412470)throw std::runtime_error("unknown air entry");
        Record before{},expected{};read(before.data(),before.size());read(expected.data(),expected.size());
        const auto mask_count=word();if(mask_count>452)throw std::runtime_error("air mask count");
        std::vector<unsigned> supplied(mask_count);for(auto& at:supplied)at=word();
        auto actual=before;Fixture fixture(actual);
        const auto observed=vehicle_air_tick(fixture.parameters,fixture.physics,fixture.air,fixture.context());fixture.save();
        const auto mask=fixture.excluded(observed);if(mask!=supplied)throw std::runtime_error("air dynamic mask differs at case "+std::to_string(n));
        excluded+=mask.size();
        for(unsigned at:mask){++retained;if(u(actual,at)!=u(before,at))throw std::runtime_error("air native opaque retention at case "+std::to_string(n)+" offset "+std::to_string(at));}
        for(unsigned at=0;at<size;at+=4) {
            if(std::binary_search(mask.begin(),mask.end(),at))continue;
            if(u(actual,at)==u(expected,at)){++exact;continue;}
            if(errors++<24)std::cerr<<"case "<<n<<" offset "<<std::hex<<at<<" expected "<<u(expected,at)<<" actual "<<u(actual,at)<<std::dec<<'\n';
        }
    }
    if(in.peek()!=std::char_traits<char>::eof())throw std::runtime_error("trailing air corpus");
    std::cout<<"{\"cases\":"<<count<<",\"exact_words\":"<<exact<<",\"errors\":"<<errors<<",\"allowed_ulp\":0,\"excluded_words\":"<<excluded<<",\"native_retained_words\":"<<retained<<"}\n";
    return errors?1:0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
