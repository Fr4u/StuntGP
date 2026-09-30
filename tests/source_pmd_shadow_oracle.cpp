#include "world_template_fixture.hpp"
#include "sgn/vehicle_shadow_draw.hpp"
#include <cstring>
#include <fstream>
#include <iostream>
#include "source_pmd_shadow_codec.hpp"
namespace {
using Car=std::array<std::uint32_t,0x7724/4>;
std::uint32_t descriptor_at(const std::filesystem::path& root,const std::string& name,std::uint32_t base){
    const auto raw=read_file(root/"meshdata"/(name+".PMD"));std::uint32_t relative;std::memcpy(&relative,raw.data()+0x4c,4);return base+328+relative;
}
void attributes(std::ifstream& in,SourcePmdModelView model,bool input){
    const auto count=word(in);const auto file=model.bytes();const auto start=328+file.field<std::uint32_t>(0x24).value();
    const auto length=file.field<std::uint32_t>(0xb8).value();require(length%8==0&&count==length/8,"shadow attribute count");
    const SourcePmdArrayView<std::array<std::uint8_t,8>> values(file.subview(start,length));
    for(unsigned i=0;i<count;++i){std::array<std::uint8_t,8> raw;read(in,raw);if(input)values.at(i).set(raw);else compare(values.at(i).value(),raw,"shadow attribute");}
}
void metadata(std::ifstream& in,SourcePmdModelView model,SourcePmdModelView car,bool input){
    VehicleVector car_vector,shadow_vector;read(in,car_vector);read(in,shadow_vector);const auto flags=word(in);require(flags<=65535,"shadow model flag word");
    const auto car_field=car.descriptor().field<VehicleVector>(0x10),shadow_field=model.descriptor().field<VehicleVector>(0x10);
    const auto flag_field=model.descriptor().field<std::uint16_t>(2);
    if(input){car_field.set(car_vector);shadow_field.set(shadow_vector);flag_field.set(static_cast<std::uint16_t>(flags));}
    else{compare(car_field.value(),car_vector,"car vector");compare(shadow_field.value(),shadow_vector,"shadow vector");compare(std::uint32_t(flag_field.value()),flags,"shadow flags");}
    attributes(in,model,input);
}
}
int main(int argc,char** argv)try{
    require(argc==4,"expected cold|live, shadow-draw corpus and retail root");const bool live=std::string_view(argv[1])=="live";require(live||std::string_view(argv[1])=="cold","shadow template mode");std::ifstream in(argv[2],std::ios::binary);std::array<char,8> magic;read(in,magic);
    require(std::string_view(magic.data(),magic.size())=="SGNSHD01","shadow draw signature");const auto count=word(in);require(count>0&&count<=20,"shadow model count");AssetStore assets(argv[3]);
    for(unsigned m=0;m<count;++m){const auto id=word(in);require(id>=1&&id<=20,"shadow car id");const auto name="car"+std::to_string(id);const auto shadow_name=name+"sh";
        SourceAllocationBank bank;bank.initialize_42eab0();ModelResources models(assets,argv[3]);const auto car_id=models.load_source(bank,name),shadow_id=models.load_source(bank,shadow_name);
        const auto car_handle=*models.resolve(car_id),shadow_handle=*models.resolve(shadow_id);const auto car_model=models.source_at(car_handle),shadow_model=models.source_at(shadow_handle);
        const auto descriptor=descriptor_at(argv[3],shadow_name,0x10000000);const auto car_descriptor=descriptor_at(argv[3],name,0x11000000);
        ModelDrawQueue regular,background;ViewMatrixArena matrices;const auto frames=word(in);require(frames>0&&frames<=64,"shadow frame count");
        for(unsigned n=0;n<frames;++n){const auto slot=word(in);require(slot<4,"shadow source LOD");shadow_model.current_detail_level().set(slot);const auto lod=shadow_model.source_lod_index(slot);Frame initial;read(in,initial);
            if(n==0)transfer(initial,regular,background,matrices,descriptor,shadow_id,true);
            else{Frame actual;transfer(actual,regular,background,matrices,descriptor,shadow_id,false);require(std::memcmp(&actual,&initial,sizeof(actual))==0,"continued shadow queue mismatch");++continued;}
            Car record;read(in,record);require(record[0x26b0/4]==car_descriptor&&record[0x26b4/4]==descriptor,"shadow source model identity");
            metadata(in,shadow_model,car_model,true);read_parts(in,shadow_model,lod,true);
            VehiclePresentationState presentation;presentation.field_3928=record[0x3928/4];VehicleShadowState shadow;
            static_assert(sizeof(VehicleShadowState)==56);std::memcpy(&shadow,record.data()+0x392c/4,sizeof(shadow));
            auto template_pose=sgn::retail::fixture::cold_world_template;if(live)std::memcpy(&template_pose,record.data()+0x2b00/4,80);
            const bool appended=append_vehicle_shadow_draw(presentation,shadow,models,car_handle,shadow_handle,background,matrices,template_pose);
            require(appended==(presentation.field_3928!=0),"shadow append diagnostic");
            Frame expected,actual;read(in,expected);transfer(actual,regular,background,matrices,descriptor,shadow_id,false);compare(actual,expected,"shadow frame");
            Car expected_car;read(in,expected_car);std::memcpy(record.data()+0x392c/4,&shadow,sizeof(shadow));record[0x3928/4]=presentation.field_3928;compare(record,expected_car,"shadow vehicle");
            metadata(in,shadow_model,car_model,false);read_parts(in,shadow_model,lod,false);++calls;
        }
    }
    require(in.peek()==std::char_traits<char>::eof(),"trailing shadow draw corpus");std::cout<<"{\"passed\":true,\"models\":"<<count<<",\"calls\":"<<calls<<",\"continued\":"<<continued<<",\"words\":"<<words<<",\"mismatches\":0,\"allowed_ulp\":0}\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 2;}
