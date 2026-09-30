#include "sgn/retail_camera_mounted.hpp"
#include "sgn/model_resources.hpp"
#include "retail_camera_state.hpp"
#include <fstream>
#include <iostream>

namespace {
using namespace sgn;using namespace sgn::retail;using namespace sgn::retail::camera_test;
void require(bool ok,const char* why){if(!ok)throw FormatError(why);}
void read(std::ifstream& f,void* p,std::size_t size){f.read(static_cast<char*>(p),static_cast<std::streamsize>(size));require(bool(f),"truncated mounted corpus");}
std::uint32_t u32(std::ifstream& f){std::uint32_t n;read(f,&n,4);return n;}
std::uint64_t checked{},calls{},continued{};
void compare(std::span<const std::uint8_t> a,std::span<const std::uint8_t> e,const char* owner){
    require(a.size()==e.size(),"mounted comparison size");
    for(std::size_t i=0;i<a.size();i+=4){++checked;if(std::memcmp(a.data()+i,e.data()+i,4)!=0){
        std::uint32_t av,ev;std::memcpy(&av,a.data()+i,4);std::memcpy(&ev,e.data()+i,4);
        std::cerr<<"call="<<calls<<" owner="<<owner<<" offset="<<std::hex<<i<<" actual="<<av<<" expected="<<ev<<std::dec<<'\n';
        throw FormatError("mounted source mismatch");}}
}
}
int main(int argc,char** argv)try{
    require(argc==3,"expected corpus and retail root");std::ifstream f(argv[1],std::ios::binary);
    std::array<char,8> magic;read(f,magic.data(),8);require(std::string_view(magic.data(),8)=="SGNCM601","mounted corpus magic");
    const auto model_count=u32(f);require(model_count==20,"mounted requires all20 car PMDs");
    AssetStore assets(argv[2]);SourceAllocationBank bank;bank.initialize_42eab0();
    for(unsigned m=0;m<model_count;++m){const auto length=u32(f);require(length>0&&length<50,"mounted model name");
        std::string name(length,'\0');read(f,name.data(),length);require(name.find_first_of("/\\:")==name.npos,"mounted unsafe name");
        ModelResources models(assets,argv[2]);const auto handle=*models.resolve(models.load_source(bank,name));
        const auto model=models.source_at(handle);const auto pose=model.part(model.source_lod_index(0),0).transform();
        ModelTransform authored;read(f,&authored,sizeof(authored));const auto authored_pose=pose.value();require(std::memcmp(&authored,&authored_pose,80)==0,"mounted source LOD0 binding differs");
        const auto count=u32(f);require(count>0&&count<=4096,"mounted invocation count");
        CameraState camera;CameraVehicle vehicle;Bytes state(camera_size),vehicle_state(vehicle_size);
        for(unsigned n=0;n<count;++n){const auto reset=u32(f),integrate=u32(f);require(reset<=1&&integrate<=1&&(n||reset),"mounted operation");
            Bytes c(camera_size),v(vehicle_size),ec(camera_size),ev(vehicle_size);ModelTransform input;
            read(f,c.data(),c.size());read(f,v.data(),v.size());read(f,&input,sizeof(input));
            if(reset)transfer(c,v,camera,vehicle,true);
            else {
                compare(state,c,"continued camera");
                compare(std::span(vehicle_state).subspan(0x22cc,16),std::span(v).subspan(0x22cc,16),"continued vehicle orbit");
                const auto retained=camera;transfer(c,v,camera,vehicle,true);camera=retained;++continued;
            }
            SourcePmdTransform fixture;std::memcpy(&fixture,&input,80);pose.set(fixture);
            const auto ref=model.allocation();const auto before=bank.bytes(ref);const Bytes pmd_before(before.begin(),before.end());
#if SGN_MOUNTED_SOURCE_ROUTE == 1
            camera_mounted_tick(camera,vehicle,model);
#else
            camera_mounted_tick(camera,vehicle,models,handle);
#endif
            if(integrate){camera_integrate_velocity(camera);camera_integrate_angles(camera);}
            transfer(c,v,camera,vehicle,false);state=c;vehicle_state=v;
            read(f,ec.data(),ec.size());read(f,ev.data(),ev.size());
            compare(c,ec,"camera");compare(v,ev,"vehicle");
            const auto after=bank.bytes(ref);require(std::equal(pmd_before.begin(),pmd_before.end(),after.begin(),after.end()),"mounted camera mutated canonical PMD");++calls;
        }
        bank.release_scope_42ea00();bank.begin_42ea80();
    }
    require(f.peek()==std::char_traits<char>::eof(),"trailing mounted corpus");
    std::cout<<"calls="<<calls<<" continued="<<continued<<" words="<<checked<<" mismatches=0 tolerance_ulp=0 excluded_words=0 models=20\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
