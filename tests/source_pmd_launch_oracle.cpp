#include "source_pmd_launch_fixture.hpp"
using namespace sgn::launch_test;
int main(int argc,char** argv)try{
    check(argc==3,"expected launch corpus and retail root");std::ifstream file(argv[1],std::ios::binary);std::array<char,8> magic{};read(file,magic);
    check(std::string_view(magic.data(),8)=="SGNMEF01","bad launch signature");unsigned count;read(file,count);check(count>0&&count<=10000,"bad launch count");
    unsigned cases{},skipped{};std::uint64_t words{},masked{},intact{};std::array<unsigned,3> branches{};
    for(unsigned n=0;n<count;++n){unsigned op;read(file,op);check(op<=5,"unknown launch operation");Record actual,expected;read(file,actual);read(file,expected);
        if(op!=3){++skipped;continue;}finite_record(actual);const auto before=actual;const auto mask=policy(before,op);auto s=std::make_unique<State>();transfer(actual,*s,true);
        Projection p(argv[2],before);const auto m=p.owner.view();const auto bytes=snapshot(p.owner.bank,m.allocation());
        source(*s,word(before,0x7e18),p.owner);intact+=sgn::brake_test::footprint(bytes,snapshot(p.owner.bank,m.allocation()),p.allowed);p.pack(*s);transfer(actual,*s,false);
        const auto turbo=std::bit_cast<float>(word(before,0x2254));++branches[word(before,0x328c)!=1?0:(turbo==0&&word(before,0x2260)==0)?1:2];
        for(unsigned at=0;at<actual.size();at+=4){if(mask.contains(at)){++masked;check(word(actual,at)==word(before,at),"launch W retention failed");continue;}
            if(word(actual,at)!=word(expected,at))throw std::runtime_error("launch mismatch case "+std::to_string(n)+" offset "+std::to_string(at));++words;}++cases;
    }
    check(file.peek()==std::char_traits<char>::eof(),"trailing launch corpus");check(cases>0,"no standalone launch cases");
    std::cout<<"{\"cases\":"<<cases<<",\"words\":"<<words<<",\"native_policy_words\":"<<masked<<",\"skipped_records\":"<<skipped<<",\"unchanged_pmd_bytes\":"<<intact
        <<",\"unavailable\":"<<branches[0]<<",\"disabled\":"<<branches[1]<<",\"active\":"<<branches[2]<<",\"mismatches\":0,\"allowed_ulp\":0}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}
