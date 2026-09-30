#include "source_pmd_speed_trail_fixture.hpp"
using namespace sgn::speed_trail_test;
int main(int argc,char** argv)try{
    check(argc==3,"expected speed trail corpus and retail root");std::ifstream stream(argv[1],std::ios::binary);std::array<char,8> magic{};load(stream,magic);
    check(std::string_view(magic.data(),8)=="SGNTRAL1","bad speed trail signature");const auto count=read(stream);check(count>0&&count<=10000,"bad speed trail count");
    unsigned cases{},skipped{};std::uint64_t words{},policies{},intact{};std::array<unsigned,3> branches{};
    for(unsigned n=0;n<count;++n){const auto entry=read(stream);check(entry==0x414c60||entry==0x414f90||entry==0x4152d0||entry==1,"unknown speed trail entry");Record actual,expected;load(stream,actual);load(stream,expected);
        if(entry!=0x414c60){++skipped;continue;}validate_record(actual);const auto before=actual;const auto exclusions=mask(entry,before);State state;state.load_record(actual);Fixture fixture(argv[2]);project_model(fixture,state.model);
        const auto pmd_before=snapshot(fixture.bank,fixture.view().allocation());update_vehicle_speed_trail(state.binding(),fixture.models,fixture.handle);intact+=unchanged_pmd(fixture,pmd_before);state.pack(actual);
        ++branches[word(before,tick_at)<20?0:double(real(before,0x1cf0))-std::bit_cast<float>(0x42855555u)<0?1:2];
        for(unsigned at=0;at<size;at+=4){if(exclusions.contains(at)){++policies;check(word(actual,at)==word(before,at),"canonical speed trail W retention failed");continue;}
            if(word(actual,at)!=word(expected,at))throw std::runtime_error("speed trail mismatch case "+std::to_string(n)+" offset "+std::to_string(at));++words;}++cases;
    }
    check(stream.peek()==std::char_traits<char>::eof(),"trailing speed trail corpus");check(cases>0,"no standalone speed trail cases");
    std::cout<<"{\"cases\":"<<cases<<",\"words\":"<<words<<",\"native_policy_words\":"<<policies<<",\"skipped_records\":"<<skipped<<",\"unchanged_pmd_bytes\":"<<intact
        <<",\"early_tick\":"<<branches[0]<<",\"reset_slow\":"<<branches[1]<<",\"append\":"<<branches[2]<<",\"mismatches\":0,\"allowed_ulp\":0}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}
