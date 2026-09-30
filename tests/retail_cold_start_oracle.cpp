#include "sgn/retail_cold_start.hpp"
#include "contact_solver_state.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <memory>
#include <string_view>
#include <vector>

namespace {
using namespace sgn::retail;
using Record=std::array<std::uint8_t,0x7800>;
struct Owned {
    std::array<VehicleParameters,6> parameters{};
    std::array<VehicleState,6> physics{};
    std::array<VehicleRaceState,6> race{};
    std::array<contact::solver::State,6> contacts{};
    unsigned current=5;
};
void transfer(Record& b,Owned& s,unsigned i,bool read,unsigned group) {
    if(group==0)oracle::transfer(b,s.parameters[i],s.physics[i],read);
    if(group==1) {
        race_oracle::transfer(b,s.race[i],read);
        if(read) {
            std::memcpy(&s.race[i].place,b.data()+0x24ac,4);
            std::uint32_t word{};std::memcpy(&word,b.data()+0x7268,4);
            s.race[i].recording=word?std::optional<ReplayHandle>{ReplayHandle{word%6}}:std::nullopt;
        } else {
            std::memcpy(b.data()+0x24ac,&s.race[i].place,4);
            const std::uint32_t handle=s.race[i].recording?s.race[i].recording->slot+1:0;
            std::memcpy(b.data()+0x7268,&handle,4);
        }
    }
    if(group==2) {
        contact::oracle::transfer_contact(b,s.contacts[i],read);
        if(!read) {
            const std::uint32_t present=s.contacts[i].surface.object_mode?1:0;
            std::memcpy(b.data()+0x726c,&present,4);
        }
    }
}
Record layout() {
    // Fixed by the existing field bridges; the corpus supplies no masks.
    Record mask{};
    const auto s=std::make_unique<Owned>();
    for(unsigned group=0;group<3;++group) {
        Record a,b;a.fill(0xa5);b.fill(0x5a);
        transfer(a,*s,0,false,group);transfer(b,*s,0,false,group);
        for(std::size_t j=0;j<a.size();++j) {
            const bool touched=a[j]!=0xa5||b[j]!=0x5a;
            if(touched) {
                if(a[j]!=b[j]||mask[j]||j>=0x7724)throw std::runtime_error("invalid or overlapping cold owner mapping");
                mask[j]=1;
            }
        }
    }
    return mask;
}
void read_exact(std::istream& in,void* p,std::size_t size) {
    if(!in.read(static_cast<char*>(p),static_cast<std::streamsize>(size)))throw std::runtime_error("truncated cold corpus");
}
}
int main(int argc,char** argv) {
    try {
        const auto mask=layout();
        if(argc==3&&std::string_view(argv[1])=="--layout") {
            std::ofstream out(argv[2],std::ios::binary);
            if(!out.write(reinterpret_cast<const char*>(mask.data()),mask.size()))throw std::runtime_error("layout write failed");
            return 0;
        }
        if(argc!=2)throw std::runtime_error("expected cold corpus path");
        std::ifstream in(argv[1],std::ios::binary);
        std::array<char,8> magic{};read_exact(in,magic.data(),magic.size());
        if(std::string_view(magic.data(),8)!="SGNCOLD1")throw std::runtime_error("wrong cold corpus version");
        std::uint32_t count{};read_exact(in,&count,4);
        if(count==0||count>1024)throw std::runtime_error("invalid cold corpus count");
        std::uint64_t mismatches=0,compared=0;
        const auto state=std::make_unique<Owned>();
        for(unsigned n=0;n<count;++n) {
            float load{};read_exact(in,&load,4);
            std::vector<Record> before(6),expected(6);
            for(auto& b:before)read_exact(in,b.data(),b.size());
            for(auto& b:expected)read_exact(in,b.data(),b.size());
            for(unsigned i=0;i<6;++i)for(unsigned g=0;g<3;++g)transfer(before[i],*state,i,true,g);
            state->current=5;
            initialize_cold_numeric_storage({state->parameters,state->physics,state->race,state->contacts,state->current},load);
            if(state->current!=0)throw std::runtime_error("cold current selector differs");
            for(unsigned i=0;i<6;++i) {
                auto actual=before[i];
                for(unsigned g=0;g<3;++g)transfer(actual,*state,i,false,g);
                for(std::size_t at=0;at<mask.size();++at)if(mask[at]) {
                    ++compared;
                    if(actual[at]!=expected[i][at]) {
                        if(mismatches<16)std::cerr<<"case="<<n<<" slot="<<i<<" offset="<<std::hex<<at<<std::dec<<" native="<<unsigned(actual[at])<<" original="<<unsigned(expected[i][at])<<'\n';
                        ++mismatches;
                    }
                }
            }
        }
        if(in.peek()!=std::char_traits<char>::eof())throw std::runtime_error("trailing cold corpus data");
        std::cout<<"{\"passed\":"<<(mismatches?"false":"true")<<",\"cases\":"<<count
                 <<",\"compared_bytes\":"<<compared<<",\"mismatches\":"<<mismatches<<"}\n";
        return mismatches?1:0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 2;}
}
