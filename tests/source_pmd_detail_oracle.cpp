#include "sgn/vehicle_model_detail.hpp"
#include <bit>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
using namespace sgn;using namespace sgn::retail;
namespace {
using VehicleRecord=std::array<std::uint32_t,0x7724/4>;
using ModelRecord=std::array<std::uint32_t,2+4*25*21>;
void read(std::ifstream& file,void* data,std::size_t bytes){if(!file.read(static_cast<char*>(data),static_cast<std::streamsize>(bytes)))throw FormatError("short model detail corpus");}
std::uint32_t number(std::ifstream& file){std::uint32_t value;read(file,&value,4);return value;}
ModelRecord pack(SourcePmdModelView model){ModelRecord r{};r[0]=model.current_detail_level().value();r[1]=model.previous_detail_level().value();unsigned at=2;
    for(unsigned lod=0;lod<4;++lod){const auto resident=model.source_lod_index(lod);if(model.part_count()!=25)throw FormatError("model detail requires25authoredparts");
        for(unsigned i=0;i<25;++i){const auto part=model.part(resident,i);r[at++]=part.flags().value()|(std::uint32_t(part.flags_02().value())<<16);auto words=std::bit_cast<std::array<std::uint32_t,20>>(part.transform().value());std::copy(words.begin(),words.end(),r.begin()+at);at+=20;}}
    return r;
}
void restore(SourcePmdModelView model,const ModelRecord& r){std::map<std::uint32_t,std::uint32_t> flags;std::map<std::uint32_t,std::array<std::uint32_t,20>> transforms;unsigned at=2;
    if(r[0]>=4)throw FormatError("model detail selected LOD limit");model.current_detail_level().set(r[0]);model.previous_detail_level().set(r[1]);
    for(unsigned lod=0;lod<4;++lod){const auto resident=model.source_lod_index(lod);for(unsigned i=0;i<25;++i){auto p=model.part(resident,i);const auto flag=r[at++];std::array<std::uint32_t,20> words;std::copy_n(r.begin()+at,20,words.begin());at+=20;
        const auto [f,added]=flags.emplace(p.location().offset,flag);if(!added&&f->second!=flag)throw FormatError("conflicting model detail flag alias");
        const auto [t,inserted]=transforms.emplace(p.transform().location().offset,words);if(!inserted&&t->second!=words)throw FormatError("conflicting model detail transform alias");
        p.flags().set(static_cast<std::uint16_t>(flag));p.flags_02().set(static_cast<std::uint16_t>(flag>>16));p.transform().set(std::bit_cast<SourcePmdTransform>(words));
    }}
    if(pack(model)!=r)throw FormatError("model detail initial owner roundtrip");
}
}
int main(int argc,char** argv)try{
    if(argc!=3)throw FormatError("expected model detail corpus and retail root");std::ifstream in(argv[1],std::ios::binary);char magic[8];read(in,magic,8);
    if(std::string_view(magic,8)!="SGNMDL01")throw FormatError("model detail signature");const auto count=number(in);if(!count||count>4096)throw FormatError("model detail case limit");
    AssetStore assets(argv[2]);std::uint64_t compared=0,mismatches=0;
    for(unsigned n=0;n<count;++n){const auto car=number(in),rate=number(in);if(car<1||car>20)throw FormatError("model detail car identity");std::array<std::uint32_t,9> c;read(in,c.data(),sizeof(c));
        VehicleRecord r,expected;ModelRecord m,model_expected;read(in,r.data(),sizeof(r));read(in,m.data(),sizeof(m));read(in,expected.data(),sizeof(expected));read(in,model_expected.data(),sizeof(model_expected));
        SourceAllocationBank bank;bank.initialize_42eab0();ModelResources models(assets,argv[2]);
        const auto identity=models.load_source(bank,"car"+std::to_string(car));const auto handle=*models.resolve(identity);const auto model=models.source_at(handle);restore(model,m);
        CameraState camera;std::memcpy(&camera.position,c.data(),16);CameraVector forward;std::memcpy(&forward,c.data()+4,16);camera.lens=std::bit_cast<float>(c[8]);
        VehicleState vehicle;std::memcpy(&vehicle.position,r.data()+0x1bac/4,16);std::memcpy(&vehicle.previous_position,r.data()+0x1bbc/4,16);vehicle.brake_signal=r[0x28a8/4];
        for(unsigned i=0;i<4;++i)vehicle.wheels[i].braking_signal=std::bit_cast<float>(r[(0x2cf4+i*4)/4]);
        VehiclePresentationState presentation;presentation.field_3134=r[0x3134/4];presentation.field_33c8=r[0x33c8/4];presentation.field_3508=r[0x3508/4];presentation.field_3924=r[0x3924/4];presentation.field_3928=r[0x3928/4];
        contact::effects::VehicleParticleState particles;particles.field_7628=r[0x7628/4];VehiclePresentationEffects effects{r[0x226c/4],r[0x328c/4],r[0x33cc/4],{}, {}};
        std::memcpy(&effects.first_scale,r.data()+0x32b0/4,12);std::memcpy(&effects.second_scale,r.data()+0x33f0/4,12);
        prepare_vehicle_model_detail(vehicle,presentation,particles,effects,models,handle,camera,forward,rate);
        r[0x28a8/4]=vehicle.brake_signal;for(unsigned i=0;i<4;++i)r[(0x2cf4+i*4)/4]=std::bit_cast<std::uint32_t>(vehicle.wheels[i].braking_signal);
        r[0x3134/4]=presentation.field_3134;r[0x33c8/4]=presentation.field_33c8;r[0x3508/4]=presentation.field_3508;r[0x3924/4]=presentation.field_3924;r[0x3928/4]=presentation.field_3928;r[0x7628/4]=particles.field_7628;
        const auto actual_model=pack(model);
        auto compare=[&](auto& actual,auto& wanted,const char* kind){for(unsigned i=0;i<actual.size();++i){++compared;if(actual[i]!=wanted[i]){if(mismatches<12)std::cerr<<"case "<<n<<' '<<kind<<" word "<<i<<" actual "<<actual[i]<<" expected "<<wanted[i]<<'\n';++mismatches;}}};compare(r,expected,"vehicle");compare(actual_model,model_expected,"model");
    }
    if(in.peek()!=std::char_traits<char>::eof())throw FormatError("trailing model detail corpus");
    std::cout<<"{\"passed\":"<<(mismatches?"false":"true")<<",\"cases\":"<<count<<",\"words\":"<<compared<<",\"mismatches\":"<<mismatches<<",\"allowed_ulp\":0}\n";return mismatches?1:0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 2;}
