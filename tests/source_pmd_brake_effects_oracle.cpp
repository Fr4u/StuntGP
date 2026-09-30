#include "source_pmd_brake_effects_fixture.hpp"
using namespace sgn::brake_effects_test;
int main(int argc,char** argv)try{
    check(argc==3,"expected brake effects corpus and retail root");std::ifstream stream(argv[1],std::ios::binary);
    auto read=[&](void* p,std::size_t n){stream.read(static_cast<char*>(p),static_cast<std::streamsize>(n));check(bool(stream),"truncated brake effects corpus");};
    auto number=[&]{unsigned v;read(&v,4);return v;};char magic[8];read(magic,8);check(std::string_view(magic,8)=="SGNMEF01","bad brake effects signature");
    const auto count=number();check(count>0&&count<=10000,"bad brake effects count");unsigned cases{},skipped{};std::uint64_t words{},intact{};
    for(unsigned n=0;n<count;++n){const auto operation=number();check(operation<=5,"unknown brake effects operation");Record before,expected;read(before.data(),before.size());read(expected.data(),expected.size());
        if(operation!=2){++skipped;continue;}Projection projection(argv[2],before);auto model=projection.owner.view();VehicleState state{};strengths(before,state,true);
        const auto vehicle_before=snapshot(state);const auto pmd_before=snapshot(projection.owner.bank,model.allocation());const auto allowed=footprint(model,state);
        update_vehicle_brake_effects(state,projection.owner.models,projection.owner.handle);check_vehicle(state,vehicle_before);
        intact+=sgn::brake_test::footprint(pmd_before,snapshot(projection.owner.bank,model.allocation()),allowed);projection.pack(before);strengths(before,state,false);
        for(unsigned at=0;at<before.size();at+=4){if(word(before,at)!=word(expected,at))throw std::runtime_error("brake effects mismatch case "+std::to_string(n)+" offset "+std::to_string(at));++words;}++cases;
    }
    check(stream.peek()==std::char_traits<char>::eof(),"trailing brake effects corpus");check(cases>0,"no standalone brake effects cases");
    std::cout<<"{\"cases\":"<<cases<<",\"words\":"<<words<<",\"skipped_records\":"<<skipped<<",\"unchanged_pmd_bytes\":"<<intact<<",\"mismatches\":0,\"excluded_words\":0,\"allowed_ulp\":0}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}
