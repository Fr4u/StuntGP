#include "source_pmd_brake_lights_fixture.hpp"
#include <fstream>
namespace {
using namespace sgn;using namespace sgn::retail;using namespace sgn::brake_test;
using Record=std::array<std::uint8_t,0x16000>;
unsigned word(const Record& r,std::size_t at){unsigned x;std::memcpy(&x,r.data()+at,4);return x;}
void put(Record& r,std::size_t at,unsigned x){std::memcpy(r.data()+at,&x,4);}
std::uint16_t half(const Record& r,std::size_t at){std::uint16_t x;std::memcpy(&x,r.data()+at,2);return x;}
void put_half(Record& r,std::size_t at,std::uint16_t x){std::memcpy(r.data()+at,&x,2);}
template<class T>void read(std::ifstream& f,T& value){if(!f.read(reinterpret_cast<char*>(&value),sizeof(value)))throw std::runtime_error("truncated brake corpus");}
void validate(const Record& r){
    check(word(r,0x7e08)<4&&word(r,0x7e0c)==0,"invalid brake selector/step");
    for(unsigned i=0;i<4;++i)check(word(r,0x7e20+i*4)<4,"invalid brake LOD map");
    for(unsigned i=0;i<96;++i){const auto a=word(r,0x13400+i*16+4);check((a&0xffff)+(a>>16)<=128,"invalid brake attribute span");}
}
// Host projection only. The immutable original fixture has4 synthetic groups;
// actual car1 has3 resident LODs. Its already relocated parts5..8 carry the4
// group-part5 records. Altered descriptor heads/max are test transport, not a
// claim about retail car topology. No runtime adapter owns a mirrored payload.
std::size_t execute(Record& r,const std::filesystem::path& root){
    validate(r);Fixture f(root);const auto m=f.view();const auto b=m.bytes();const auto d=m.descriptor();
    const auto head=d.field<unsigned>(0x48).value();std::array<unsigned,4> target{};
    for(unsigned g=0;g<4;++g)target[g]=head+(5+g)*64;
    const auto attrs=0x148+b.field<unsigned>(0x24).value();
    check(b.field<unsigned>(0xb8).value()>=128*8,"actual PMD has too few attributes for projection");
    for(unsigned i=0;i<128;++i){std::array<std::uint8_t,8> row{};std::memcpy(row.data(),r.data()+0x14000+i*8,8);b.field<std::array<std::uint8_t,8>>(attrs+i*8).set(row);}
    for(unsigned g=0;g<4;++g){const auto index=g*24+5;const auto a=word(r,0x13400+index*16+4);
        b.field<std::uint16_t>(target[g]).set(half(r,0x13000+index*2));
        // Keep upper+26 word as a canary; source415640 only reads low+24 WORD.
        b.field<unsigned>(target[g]+0x24).set(0xa55a0000u|(a>>16));
        b.field<unsigned>(target[g]+0x34).set(attrs+(a&0xffff)*8);
    }
    for(unsigned slot=0;slot<4;++slot)d.field<unsigned>(0x48+slot*4).set(target[word(r,0x7e20+slot*4)]-5*64);
    d.field<unsigned>(0x58).set(3);m.current_detail_level().set(word(r,0x7e08));
    VehicleState physical{};VehicleTrailInputs trails{};VehicleModelEffects effects{};
    physical.brake_signal=std::bit_cast<std::int32_t>(word(r,0x28a8));trails.field_28a0=word(r,0x28a0);effects.cached_brake=word(r,0x28a4);
    const auto part=m.part(m.source_lod_index(m.current_detail_level().value()),5);
    const auto before=snapshot(f.bank,m.allocation());const auto possible=allowed_writes(part,true);
    update_vehicle_brake_lights(physical,trails,effects,f.models,f.handle);
    const auto unchanged=footprint(before,snapshot(f.bank,m.allocation()),possible);
    put(r,0x28a8,std::bit_cast<unsigned>(physical.brake_signal));put(r,0x28a4,effects.cached_brake);
    for(unsigned g=0;g<4;++g)put_half(r,0x13000+(g*24+5)*2,b.field<std::uint16_t>(target[g]).value());
    for(unsigned i=0;i<128;++i){const auto row=b.field<std::array<std::uint8_t,8>>(attrs+i*8).value();std::memcpy(r.data()+0x14000+i*8,row.data(),8);}
    return unchanged;
}
}
int main(int argc,char** argv)try{
    check(argc==3,"expected brake corpus and retail root");std::ifstream file(argv[1],std::ios::binary);
    std::array<char,8> magic{};read(file,magic);check(std::string_view(magic.data(),8)=="SGNMEF01","bad brake magic");
    unsigned count{};read(file,count);check(count>0&&count<=10000,"invalid brake count");
    std::size_t words{},mismatches{},untouched{};unsigned selected{},skipped{};
    for(unsigned n=0;n<count;++n){unsigned op{};read(file,op);check(op<=5,"invalid brake operation");Record r{},expected{};read(file,r);read(file,expected);
        if(op!=0){++skipped;continue;}++selected;untouched+=execute(r,argv[2]);
        for(std::size_t at=0;at<r.size();at+=4){++words;if(word(r,at)!=word(expected,at)){
            if(mismatches<16)std::cerr<<"brake mismatch case="<<n<<" offset="<<std::hex<<at<<std::dec<<'\n';++mismatches;}}
    }
    check(file.peek()==std::char_traits<char>::eof(),"trailing brake corpus");check(selected>0,"no single-op brake cases");
    if(count==1536)check(selected==256&&skipped==1280,"original brake subset shape changed");
    std::cout<<"canonical_projection_cases="<<selected<<" other_operations_skipped="<<skipped<<" words="<<words
        <<" unchanged_pmd_bytes="<<untouched<<" mismatches="<<mismatches<<" tolerance_ulp=0\n";return mismatches?1:0;
}catch(const std::exception& e){std::cerr<<"brake oracle: "<<e.what()<<'\n';return 2;}
