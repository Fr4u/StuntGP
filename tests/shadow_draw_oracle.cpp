#include "world_template_fixture.hpp"
#include "sgn/vehicle_shadow_draw.hpp"
#include <cstring>
#include <fstream>
#include <iostream>
#include "shadow_draw_codec.hpp"
namespace {
using Car=std::array<std::uint32_t,0x7724/4>;
std::uint32_t descriptor_at(const std::filesystem::path& root,const std::string& name,std::uint32_t base){
    const auto raw=read_file(root/"meshdata"/(name+".PMD"));std::uint32_t relative;std::memcpy(&relative,raw.data()+0x4c,4);return base+328+relative;
}
void attributes(std::ifstream& in,ModelResource& model,bool input){
    const auto count=word(in);auto values=model.source_attribute_table();require(count==values.size(),"shadow attribute count");
    for(auto& value:values){std::array<std::uint8_t,8> raw;read(in,raw);if(input)value=raw;else compare(value,raw,"shadow attribute");}
}
void metadata(std::ifstream& in,ModelResource& model,ModelResource& car,bool input){
    VehicleVector car_vector,shadow_vector;read(in,car_vector);read(in,shadow_vector);const auto flags=word(in);require(flags<=65535,"shadow model flag word");
    if(input){car.source_model_vector_10()=car_vector;model.source_model_vector_10()=shadow_vector;model.source_model_flags_02()=static_cast<std::uint16_t>(flags);}
    else{compare(car.source_model_vector_10(),car_vector,"car vector");compare(model.source_model_vector_10(),shadow_vector,"shadow vector");compare(std::uint32_t(model.source_model_flags_02()),flags,"shadow flags");}
    attributes(in,model,input);
}
}
int main(int argc,char** argv)try{
    require(argc==3,"expected shadow-draw corpus and retail root");std::ifstream in(argv[1],std::ios::binary);std::array<char,8> magic;read(in,magic);
    require(std::string_view(magic.data(),magic.size())=="SGNSHD01","shadow draw signature");const auto count=word(in);require(count>0&&count<=20,"shadow model count");AssetStore assets(argv[2]);
    for(unsigned m=0;m<count;++m){const auto id=word(in);require(id>=1&&id<=20,"shadow car id");const auto name="car"+std::to_string(id);const auto shadow_name=name+"sh";
        ModelResources models(assets,argv[2]);const auto car_id=models.load(name),shadow_id=models.load(shadow_name);auto& car_model=models.at(*models.resolve(car_id));auto& shadow_model=models.at(*models.resolve(shadow_id));
        const auto descriptor=descriptor_at(argv[2],shadow_name,0x10000000);const auto car_descriptor=descriptor_at(argv[2],name,0x11000000);
        ModelDrawQueue regular,background;ViewMatrixArena matrices;const auto frames=word(in);require(frames>0&&frames<=64,"shadow frame count");
        for(unsigned n=0;n<frames;++n){const auto slot=word(in);require(slot<4,"shadow source LOD");const auto lod=shadow_model.source_lod_index(slot);Frame initial;read(in,initial);
            if(n==0)transfer(initial,regular,background,matrices,descriptor,shadow_id,true);
            else{Frame actual;transfer(actual,regular,background,matrices,descriptor,shadow_id,false);require(std::memcmp(&actual,&initial,sizeof(actual))==0,"continued shadow queue mismatch");++continued;}
            Car record;read(in,record);require(record[0x26b0/4]==car_descriptor&&record[0x26b4/4]==descriptor,"shadow source model identity");
            metadata(in,shadow_model,car_model,true);read_parts(in,shadow_model,lod,true);
            VehiclePresentationState presentation;presentation.field_3928=record[0x3928/4];VehicleShadowState shadow;
            static_assert(sizeof(VehicleShadowState)==56);std::memcpy(&shadow,record.data()+0x392c/4,sizeof(shadow));
            const bool appended=append_vehicle_shadow_draw(presentation,shadow,car_model,shadow_model,slot,shadow_id,background,matrices,sgn::retail::fixture::cold_world_template);
            require(appended==(presentation.field_3928!=0),"shadow append diagnostic");
            Frame expected,actual;read(in,expected);transfer(actual,regular,background,matrices,descriptor,shadow_id,false);compare(actual,expected,"shadow frame");
            Car expected_car;read(in,expected_car);std::memcpy(record.data()+0x392c/4,&shadow,sizeof(shadow));record[0x3928/4]=presentation.field_3928;compare(record,expected_car,"shadow vehicle");
            metadata(in,shadow_model,car_model,false);read_parts(in,shadow_model,lod,false);++calls;
        }
    }
    require(in.peek()==std::char_traits<char>::eof(),"trailing shadow draw corpus");std::cout<<"{\"passed\":true,\"models\":"<<count<<",\"calls\":"<<calls<<",\"continued\":"<<continued<<",\"words\":"<<words<<",\"mismatches\":0,\"allowed_ulp\":0}\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 2;}
