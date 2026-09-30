#include "source_pmd_presentation_fixture.hpp"
using namespace sgn::presentation_test;
int main(int argc,char** argv)try{
    check(argc==3,"expected presentation corpus and retail root");std::ifstream stream(argv[1],std::ios::binary);
    auto read=[&](void* p,std::size_t n){stream.read(static_cast<char*>(p),static_cast<std::streamsize>(n));check(bool(stream),"truncated standalone presentation corpus");};
    auto number=[&]{unsigned v;read(&v,4);return v;};char magic[8];read(magic,8);check(std::string_view(magic,8)=="SGNPRES1","bad standalone presentation signature");
    const auto count=number();check(count>0&&count<=10000,"bad standalone presentation count");unsigned cases{},skipped{};std::uint64_t words{},intact{};
    for(unsigned n=0;n<count;++n){const auto entry=number();check(entry==0||entry==1||entry==0x413da0||entry==0x4166f0||entry==0x414370||entry==0x414460||entry==0x4137b0,"unknown presentation entry");
        for(unsigned slot=0;slot<(entry==0?8u:1u);++slot){Record b,expected;read(b.data(),b.size());read(expected.data(),expected.size());if(entry!=0x413da0){++skipped;continue;}
            Projection projection(argv[2],b);auto m=projection.owner.view();VehiclePresentationState visual{};state(b,visual,true);const auto e=effects(b);
            const auto allowed=footprint(m,projection.lod,e);
            const auto before=snapshot(projection.owner.bank,m.allocation());
            prepare_vehicle_presentation(visual,e,projection.owner.models,projection.owner.handle);
            intact+=unchanged(before,snapshot(projection.owner.bank,m.allocation()),allowed);projection.pack(b);state(b,visual,false);
            for(unsigned at=0;at<record_size;at+=4){if(word(b,at)!=word(expected,at))throw std::runtime_error("presentation mismatch case "+std::to_string(n)+" offset "+std::to_string(at));++words;}++cases;
        }
    }
    check(stream.peek()==std::char_traits<char>::eof(),"trailing standalone presentation corpus");check(cases>0,"no standalone presentation cases");
    std::cout<<"{\"cases\":"<<cases<<",\"words\":"<<words<<",\"skipped_records\":"<<skipped<<",\"unchanged_pmd_bytes\":"<<intact<<",\"mismatches\":0,\"excluded_words\":0,\"allowed_ulp\":0}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}
