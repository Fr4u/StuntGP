#include "source_pmd_brake_lights_fixture.hpp"
#include "vehicle_oracle_state.hpp"
#include <fstream>
namespace {
using namespace sgn;using namespace sgn::retail;using namespace sgn::brake_test;
using Record=std::array<std::uint8_t,0x16000>;
unsigned word(const Record& r,std::size_t at){unsigned x;std::memcpy(&x,r.data()+at,4);return x;}
void put(Record& r,std::size_t at,unsigned x){std::memcpy(r.data()+at,&x,4);}
std::uint16_t half(const Record& r,std::size_t at){std::uint16_t x;std::memcpy(&x,r.data()+at,2);return x;}
void put_half(Record& r,std::size_t at,std::uint16_t x){std::memcpy(r.data()+at,&x,2);}
template<class T>void read(std::ifstream& f,T& value){if(!f.read(reinterpret_cast<char*>(&value),sizeof(value)))throw std::runtime_error("truncated route corpus");}
std::size_t execute(Record& r,const std::filesystem::path& root){
    check(word(r,0x7e0c)==0,"invalid route step");
    for(unsigned i=0;i<4;++i)check(word(r,0x7e20+i*4)<4,"invalid route fixture LOD map");
    Fixture f(root);const auto m=f.view();const auto b=m.bytes();const auto d=m.descriptor();const auto head=d.field<unsigned>(0x48).value();
    std::array<std::array<unsigned,2>,4> targets{};std::set<unsigned> allowed;
    //Test transport only: four original synthetic groups use actual relocated
    //car1 parts5..8 and10..13. No model payload is copied by runtime adapters.
    for(unsigned group=0;group<4;++group)for(unsigned lane=0;lane<2;++lane){
        const auto part=lane==0?10u:5u;const auto at=head+(part+group)*64;targets[group][lane]=at;
        b.field<std::uint16_t>(at).set(half(r,0x13000+(group*24+part)*2));allowed.insert(at);allowed.insert(at+1);
    }
    for(unsigned slot=0;slot<4;++slot)d.field<unsigned>(0x48+slot*4).set(head+word(r,0x7e20+slot*4)*64);
    d.field<unsigned>(0x58).set(3);m.current_detail_level().set(word(r,0x7e08));
    //Unlike415640, this stage reads synthetic descriptor count, so preserve it.
    d.field<unsigned>(0x24).set(word(r,0x7e10));
    VehicleParameters parameters{};VehicleState physical{};VehicleRaceState race{};VehicleTrailInputs trails{};VehicleModelEffects effects{};
    oracle::transfer(r,parameters,physical,true);race.spline_index=word(r,0x24a4);trails.field_28a0=word(r,0x28a0);
    effects.phase=word(r,0x225c);effects.enabled=word(r,0x226c);effects.cached_brake=word(r,0x28a4);effects.route_envelope=word(r,0x7684);
    std::array<float,128> lights{};std::array<unsigned,128> ambience{};
    std::memcpy(lights.data(),r.data()+0x7a00,sizeof(lights));std::memcpy(ambience.data(),r.data()+0x7c00,sizeof(ambience));
    const auto before=snapshot(f.bank,m.allocation());
    update_vehicle_route_lights(physical,race,trails,effects,f.models,f.handle,{lights,ambience,word(r,0x7e00),word(r,0x7e04)});
    const auto unchanged=footprint(before,snapshot(f.bank,m.allocation()),allowed);
    oracle::transfer(r,parameters,physical,false);put(r,0x24a4,race.spline_index);put(r,0x28a0,trails.field_28a0);
    put(r,0x225c,effects.phase);put(r,0x226c,effects.enabled);put(r,0x28a4,effects.cached_brake);put(r,0x7684,effects.route_envelope);
    for(unsigned group=0;group<4;++group)for(unsigned lane=0;lane<2;++lane){const auto part=lane==0?10u:5u;
        put_half(r,0x13000+(group*24+part)*2,b.field<std::uint16_t>(targets[group][lane]).value());}
    return unchanged;
}
}
int main(int argc,char** argv)try{
    check(argc==3,"expected route corpus and retail root");std::ifstream file(argv[1],std::ios::binary);
    std::array<char,8> magic{};read(file,magic);check(std::string_view(magic.data(),8)=="SGNMEF01","bad route magic");
    unsigned count{};read(file,count);check(count>0&&count<=10000,"invalid route count");
    std::size_t words{},mismatches{},untouched{};unsigned selected{},skipped{};
    for(unsigned n=0;n<count;++n){unsigned op{};read(file,op);check(op<=5,"invalid route operation");Record r{},expected{};read(file,r);read(file,expected);
        if(op!=1){++skipped;continue;}++selected;untouched+=execute(r,argv[2]);
        for(std::size_t at=0;at<r.size();at+=4){++words;if(word(r,at)!=word(expected,at)){
            if(mismatches<16)std::cerr<<"route mismatch case="<<n<<" offset="<<std::hex<<at<<std::dec<<'\n';++mismatches;}}
    }
    check(file.peek()==std::char_traits<char>::eof(),"trailing route corpus");check(selected>0,"no single-op route cases");
    if(count==1536)check(selected==256&&skipped==1280,"original route subset shape changed");
    std::cout<<"canonical_route_projection_cases="<<selected<<" other_operations_skipped="<<skipped<<" words="<<words
        <<" unchanged_pmd_bytes="<<untouched<<" mismatches="<<mismatches<<" tolerance_ulp=0\n";return mismatches?1:0;
}catch(const std::exception& e){std::cerr<<"route oracle: "<<e.what()<<'\n';return 2;}
