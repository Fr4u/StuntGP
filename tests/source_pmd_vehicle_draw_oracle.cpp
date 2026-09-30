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
    if(argc!=4)throw FormatError("expected cold|live, draw pose corpus, retail root");const bool live=std::string_view(argv[1])=="live";if(!live&&std::string_view(argv[1])!="cold")throw FormatError("draw pose mode");std::ifstream file(argv[2],std::ios::binary);char magic[8];read(file,magic,8);
    if(std::string_view(magic,8)!="SGNDRW01")throw FormatError("vehicle draw pose signature");const auto count=word(file);if(!count||count>4096)throw FormatError("vehicle draw pose count");
    SourceAllocationBank bank;bank.initialize_42eab0();AssetStore assets(argv[3]);ModelResources models(assets,argv[3]);
    std::array<std::size_t,20> handles;for(unsigned i=0;i<20;++i)handles[i]=*models.resolve(models.load_source(bank,"car"+std::to_string(i+1)));
    std::uint64_t compared=0,mismatches=0,immutable_bytes=0;
    for(unsigned n=0;n<count;++n){Record r,expected;read(file,r.data(),sizeof(r));const auto old_metadata=word(file);read(file,expected.data(),sizeof(expected));std::array<std::uint32_t,20> expected_pose;read(file,expected_pose.data(),sizeof(expected_pose));const auto expected_metadata=word(file);
        auto template_pose=sgn::retail::fixture::cold_world_template;if(live)std::memcpy(&template_pose,r.data()+0x2b00/4,80);VehicleState v;std::memcpy(&v.position,r.data()+0x1bac/4,16);std::memcpy(&v.up,r.data()+0x1ccc/4,12);v.basis_y_w=std::bit_cast<float>(r[0x1cd8/4]);std::memcpy(&v.basis_z,r.data()+0x1cdc/4,16);
        const auto pose=std::bit_cast<std::array<std::uint32_t,20>>(vehicle_draw_pose(v,std::bit_cast<float>(r[0x73b0/4]),template_pose));
        VehicleModelEffects effects;effects.route_envelope=r[0x7684/4];const auto handle=handles[n%handles.size()];const auto view=models.source_at(handle);
        const auto descriptor=view.descriptor();descriptor.field<std::uint32_t>(4).set(old_metadata);
        const auto raw_before=bank.bytes(view.allocation());const Bytes before(raw_before.begin(),raw_before.end());
        update_vehicle_draw_ambience(effects,models,handle);const auto metadata=descriptor.field<std::uint32_t>(4).value();
        const auto raw_after=bank.bytes(view.allocation());const auto at=view.ambience().location().offset;
        for(std::size_t i=0;i<before.size();++i)if(i!=at&&i!=at+1){++immutable_bytes;if(before[i]!=raw_after[i])throw FormatError("ambience source footprint");}
        VehicleRaceState race;race.collision_hold=r[0x7640/4];VehicleModelStorage model;model.field_2708=r[0x2708/4];update_vehicle_draw_flags(race,model);r[0x2708/4]=model.field_2708;
        auto compare=[&](const auto& actual,const auto& wanted,const char* kind){for(unsigned i=0;i<actual.size();++i){++compared;if(actual[i]!=wanted[i]){if(mismatches<12)std::cerr<<"case "<<n<<' '<<kind<<" word "<<i<<" actual "<<actual[i]<<" expected "<<wanted[i]<<'\n';++mismatches;}}};
        compare(r,expected,"vehicle");compare(pose,expected_pose,"pose");++compared;mismatches+=metadata!=expected_metadata;
    }
    if(file.peek()!=std::char_traits<char>::eof())throw FormatError("trailing vehicle draw pose corpus");
    std::cout<<"{\"passed\":"<<(mismatches?"false":"true")<<",\"cases\":"<<count<<",\"words\":"<<compared<<",\"mismatches\":"<<mismatches<<",\"allowed_ulp\":0,\"canonical_models\":20,\"immutable_bytes\":"<<immutable_bytes<<"}\n";return mismatches?1:0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 2;}
