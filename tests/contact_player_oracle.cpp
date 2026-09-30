#include "sgn/contact_player.hpp"
#include <bit>
#include <cstring>
#include <fstream>
#include <iostream>
#include <type_traits>
namespace {
using namespace sgn::retail::contact::player;
using Record=std::array<std::uint8_t,0x2200>;
std::uint32_t u(const Record& b,unsigned at){std::uint32_t value;std::memcpy(&value,b.data()+at,4);return value;}
void u(Record& b,unsigned at,std::uint32_t value){std::memcpy(b.data()+at,&value,4);}
void apply(Record& b,unsigned entry){
    Controls controls{};controls.current=u(b,0x18);
    static_assert(sizeof(Notice)==0x54&&std::is_trivially_copyable_v<Notice>);
    auto transfer=[&](bool reading){
        for(unsigned i=0;i<4;++i){const auto at=0x100+i*0x74c;auto& c=controls.controllers[i];
            if(reading){c.active=std::bit_cast<std::int32_t>(u(b,at));c.vehicle=u(b,at+0xac);std::memcpy(c.notices.data(),b.data()+at+0x4ac,0x2a0);}
            else{u(b,at,std::bit_cast<std::uint32_t>(c.active));u(b,at+0xac,c.vehicle);std::memcpy(b.data()+at+0x4ac,c.notices.data(),0x2a0);}
        }
    };
    transfer(true);
    switch(entry){
    case 0x402590:case 0x442c50:u(b,0x1c,find_controller(controls,u(b,4)).value_or(0xffffffff));break;
    case 0x402580:restore_primary(controls);break;
    case 0x442c10:if(u(b,0x80)>=4)throw std::runtime_error("bad clear index");clear_notices(controls.controllers[u(b,0x80)].notices);break;
    case 0x442cf0:{
        const auto* text=reinterpret_cast<const char*>(b.data()+0x20);unsigned length=0;while(length<64&&text[length])++length;
        post_notice(controls,std::bit_cast<std::int32_t>(u(b,0)),u(b,0x14),std::bit_cast<std::int32_t>(u(b,8)),std::string_view(text,length),
            u(b,4),std::bit_cast<std::int32_t>(u(b,0xc)),std::bit_cast<std::int32_t>(u(b,0x10)));break;
    }
    default:throw std::runtime_error("bad player entry");
    }
    transfer(false);u(b,0x18,controls.current);
}
void read(std::ifstream& in,void* p,std::size_t n){if(!in.read(static_cast<char*>(p),static_cast<std::streamsize>(n)))throw std::runtime_error("short player corpus");}
std::uint32_t read_u(std::ifstream& in){std::uint32_t v;read(in,&v,4);return v;}
}
int main(int argc,char** argv){
    try{
        if(argc!=2)throw std::runtime_error("expected player corpus");std::ifstream in(argv[1],std::ios::binary);char magic[8];read(in,magic,8);
        if(std::string(magic,8)!="SGNCP001")throw std::runtime_error("player corpus header");const auto count=read_u(in);if(count>10000)throw std::runtime_error("player corpus count");
        unsigned errors=0;std::uint64_t exact=0;
        for(unsigned n=0;n<count;++n){const auto entry=read_u(in);Record actual{},expected{};read(in,actual.data(),actual.size());read(in,expected.data(),expected.size());apply(actual,entry);
            for(unsigned at=0;at<actual.size();at+=4){if(u(actual,at)==u(expected,at)){++exact;continue;}
                if(errors++<24)std::cerr<<"case "<<n<<" entry "<<std::hex<<entry<<" offset "<<at<<" expected "<<u(expected,at)<<" actual "<<u(actual,at)<<std::dec<<'\n';}
        }
        if(in.peek()!=std::char_traits<char>::eof())throw std::runtime_error("trailing player corpus");
        std::cout<<"{\"cases\":"<<count<<",\"exact_words\":"<<exact<<",\"errors\":"<<errors<<",\"allowed_ulp\":0}\n";return errors?1:0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
