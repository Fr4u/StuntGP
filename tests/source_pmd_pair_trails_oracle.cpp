#include "source_pmd_pair_trails_fixture.hpp"
using namespace sgn::pair_trail_test;
int main(int argc,char** argv)try{
    check(argc==3,"expected pair trail corpus and retail root");std::ifstream stream(argv[1],std::ios::binary);std::array<char,8> magic{};load(stream,magic);
    check(std::string_view(magic.data(),8)=="SGNTRAL1","bad pair trail signature");const auto count=read(stream);check(count>0&&count<=10000,"bad pair trail count");
    unsigned cases{},skipped{},a_cases{},b_cases{};std::uint64_t words{},policies{},intact{};std::array<unsigned,3> branches{};
    for(unsigned n=0;n<count;++n){const auto entry=read(stream);check(entry==0x414c60||entry==0x414f90||entry==0x4152d0||entry==1,"unknown pair trail entry");Record actual,expected;load(stream,actual);load(stream,expected);
        if(entry!=0x414f90&&entry!=0x4152d0){++skipped;continue;}const bool second=entry==0x4152d0;validate_record(actual,second);
        const auto before=actual;const auto exclusions=mask(entry,before);State state;state.load_record(actual);Fixture fixture(argv[2]);project_basis(fixture,state.model);
        const auto pmd_before=snapshot(fixture.bank,fixture.view().allocation());source(state,second,fixture.models,fixture.handle);intact+=unchanged_pmd(fixture,pmd_before);state.pack(actual);
        ++branches[word(before,tick_at)<60?0:word(before,0x28a0)==0?1:2];if(second)++b_cases;else ++a_cases;
        for(unsigned at=0;at<size;at+=4){if(exclusions.contains(at)){++policies;check(word(actual,at)==word(before,at),"canonical pair trail W retention failed");continue;}
            if(word(actual,at)!=word(expected,at))throw std::runtime_error("pair trail mismatch case "+std::to_string(n)+" offset "+std::to_string(at));++words;}++cases;
    }
    check(stream.peek()==std::char_traits<char>::eof(),"trailing pair trail corpus");check(cases>0,"no standalone pair trail cases");
    std::cout<<"{\"cases\":"<<cases<<",\"pair_a\":"<<a_cases<<",\"pair_b\":"<<b_cases<<",\"words\":"<<words<<",\"native_policy_words\":"<<policies
        <<",\"skipped_records\":"<<skipped<<",\"unchanged_pmd_bytes\":"<<intact<<",\"early_tick\":"<<branches[0]<<",\"disabled\":"<<branches[1]
        <<",\"active_pairs\":"<<branches[2]<<",\"mismatches\":0,\"allowed_ulp\":0}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}
