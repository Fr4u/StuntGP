#include "world_template_fixture.hpp"
#include "sgn/vehicle_draw_pose.hpp"
#include <bit>
#include <cstring>
#include <fstream>
#include <iostream>
using namespace sgn;using namespace sgn::retail;
namespace {
using Record=std::array<std::uint32_t,0x7724/4>;
void read(std::ifstream& file,void* data,std::size_t size){if(!file.read(static_cast<char*>(data),static_cast<std::streamsize>(size)))throw FormatError("short vehicle draw pose corpus");}
std::uint32_t word(std::ifstream& file){std::uint32_t value;read(file,&value,4);return value;}
}
int main(int argc,char** argv)try{
    if(argc!=2)throw FormatError("expected vehicle draw pose corpus");std::ifstream file(argv[1],std::ios::binary);char magic[8];read(file,magic,8);
    if(std::string_view(magic,8)!="SGNDRW01")throw FormatError("vehicle draw pose signature");const auto count=word(file);if(!count||count>4096)throw FormatError("vehicle draw pose count");
    std::uint64_t compared=0,mismatches=0;
    for(unsigned n=0;n<count;++n){Record r,expected;read(file,r.data(),sizeof(r));const auto old_metadata=word(file);read(file,expected.data(),sizeof(expected));std::array<std::uint32_t,20> expected_pose;read(file,expected_pose.data(),sizeof(expected_pose));const auto expected_metadata=word(file);
        ModelTransform live_template;std::memcpy(&live_template,r.data()+0x2b00/4,80);VehicleState v;std::memcpy(&v.position,r.data()+0x1bac/4,16);std::memcpy(&v.up,r.data()+0x1ccc/4,12);v.basis_y_w=std::bit_cast<float>(r[0x1cd8/4]);std::memcpy(&v.basis_z,r.data()+0x1cdc/4,16);
        const auto pose=std::bit_cast<std::array<std::uint32_t,20>>(vehicle_draw_pose(v,std::bit_cast<float>(r[0x73b0/4]),live_template));
        VehicleModelEffects effects;effects.route_envelope=r[0x7684/4];const auto metadata=(old_metadata&0xffff0000u)|vehicle_draw_ambience(effects);
        VehicleRaceState race;race.collision_hold=r[0x7640/4];VehicleModelStorage model;model.field_2708=r[0x2708/4];update_vehicle_draw_flags(race,model);r[0x2708/4]=model.field_2708;
        auto compare=[&](const auto& actual,const auto& wanted,const char* kind){for(unsigned i=0;i<actual.size();++i){++compared;if(actual[i]!=wanted[i]){if(mismatches<12)std::cerr<<"case "<<n<<' '<<kind<<" word "<<i<<" actual "<<actual[i]<<" expected "<<wanted[i]<<'\n';++mismatches;}}};
        compare(r,expected,"vehicle");compare(pose,expected_pose,"pose");++compared;mismatches+=metadata!=expected_metadata;
    }
    if(file.peek()!=std::char_traits<char>::eof())throw FormatError("trailing vehicle draw pose corpus");
    std::cout<<"{\"passed\":"<<(mismatches?"false":"true")<<",\"cases\":"<<count<<",\"words\":"<<compared<<",\"mismatches\":"<<mismatches<<",\"allowed_ulp\":0}\n";return mismatches?1:0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 2;}
