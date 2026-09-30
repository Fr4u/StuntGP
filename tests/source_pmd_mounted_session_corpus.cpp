#include "source_pmd_mounted_fixture.hpp"
#include <fstream>
using namespace sgn;using namespace sgn::mounted_test;
namespace {
std::uint64_t words{},frames{},retained{},pmd_compared{};
void read(std::ifstream& f,void* out,std::size_t size){f.read(static_cast<char*>(out),static_cast<std::streamsize>(size));check(bool(f),"truncated stored mounted Session capture");}
unsigned u32(std::ifstream& f){unsigned value;read(f,&value,4);return value;}
std::string name(std::ifstream& f){const auto length=u32(f);check(length>0&&length<64,"capture name length");std::string n(length,'\0');read(f,n.data(),length);check(n.find_first_of("/\\:")==n.npos,"capture path admission");return n;}
void compare(std::span<const std::uint8_t> a,std::span<const std::uint8_t> b,const char* owner){check(a.size()==b.size()&&a.size()%4==0,"capture word boundary");
    for(std::size_t at=0;at<a.size();at+=4){++words;if(std::memcmp(a.data()+at,b.data()+at,4)!=0){std::cerr<<"frame="<<frames<<" owner="<<owner<<" offset="<<at<<'\n';throw FormatError("stored mounted Session mismatch");}}}
void life_decode(Bytes& raw,CameraLifecycle& life,bool reading){
    // Capture encodes actual guest record tokens, not optional index+1. Preserve
    // all pointer bytes on output; pending is zero in this stored source domain.
    if(reading){auto normalized=raw;for(unsigned at:{0x1ccu,0x1d8u}){auto token=word(raw,at);if(token){check(token>=0x4722d8&&(token-0x4722d8)%64==0&&(token-0x4722d8)/64<32,"captured record token");put(normalized,at,1+(token-0x4722d8)/64);}}lifecycle(normalized,life,true);}
    else{lifecycle(raw,life,false);for(unsigned at:{0x1ccu,0x1d8u}){auto token=word(raw,at);if(token)put(raw,at,0x4722d8+(token-1)*64);}}
}
}
int main(int argc,char** argv)try{
    check(argc==3,"retail root and immutable stored mounted Session capture required");std::ifstream f(argv[2],std::ios::binary);std::array<char,8> magic;read(f,magic.data(),8);
    check(std::string_view(magic.data(),8)=="SGNMCSP1"&&u32(f)==3,"stored Session capture header");Fixture owner(argv[1]);
    for(unsigned scenario=0;scenario<3;++scenario){const auto level=name(f),model_name=name(f);const auto count=u32(f);check(count==720,"stored mounted count");
        const auto h=*owner.models.resolve(owner.models.load_source(owner.bank,model_name));const auto model=owner.models.source_at(h);const auto ref=model.allocation();const auto pose=model.part(model.source_lod_index(0),0).transform();
        Bytes previous;
        for(unsigned frame=0;frame<count;++frame){const auto tick=u32(f),cursor=u32(f),selected_lod=u32(f);(void)tick;(void)selected_lod;check(cursor<4,"captured current slot");
            std::array<Bytes,4> cameras;Bytes vehicle_raw(vehicle_size);SourcePmdTransform transform;
            for(auto& c:cameras){c.resize(camera_size);read(f,c.data(),c.size());}read(f,vehicle_raw.data(),vehicle_raw.size());const auto input_vehicle=vehicle_raw;read(f,&transform,80);
            if(frame){for(unsigned at:{8u,0x28u,0x38u})compare(std::span(previous).subspan(at,16),std::span(cameras[0]).subspan(at,16),"retained camera");++retained;}
            check(word(vehicle_raw,0x26b0)==0x10400000,"stored normalized model identity");
            auto& s=*owner.state;CameraVehicle observed;for(unsigned i=0;i<4;++i){auto& c=s.controls.controllers[i];transfer(cameras[i],vehicle_raw,c.camera,observed,true);life_decode(cameras[i],s.camera_lifecycle[i],true);
                c.active=word(cameras[i],0);check(word(cameras[i],0xac)==0x10010000,"stored normalized vehicle identity");c.vehicle=0;
                if(i==0)check(c.active==1&&c.camera.mode==6&&!s.camera_lifecycle[i].pending_record&&c.camera.pending_lens==0,"stored mode6 no-pending domain");else check(c.active!=1,"stored one-active-controller domain");}
            observe(s.vehicles,0,observed);s.vehicles.models[0].model=h;s.controls.current=cursor;pose.set(transform);
            const auto before=pmd_bytes(owner.bank,ref);auto world=owner.world();
#ifdef SGN_MOUNTED_ACTUAL_ENTRY
            camera_frame_before_world(world);
#else
            call_frame(world);
#endif
            std::array<Bytes,4> expected;for(auto& c:expected){c.resize(camera_size);read(f,c.data(),c.size());}CameraVector orbit;read(f,&orbit,16);const auto expected_cursor=u32(f);
            auto actual_vehicle=s.vehicles.camera_snapshot(0);
            for(unsigned i=0;i<4;++i){transfer(cameras[i],vehicle_raw,s.controls.controllers[i].camera,actual_vehicle,false);life_decode(cameras[i],s.camera_lifecycle[i],false);put(cameras[i],0,s.controls.controllers[i].active);compare(cameras[i],expected[i],"camera");}
            // Re-read the original vehicle owner through a second file stream is
            // unnecessary: preserve the input and patch only source-owned orbitXYZW.
            auto expected_vehicle=input_vehicle;std::memcpy(expected_vehicle.data()+0x22cc,&orbit,16);compare(vehicle_raw,expected_vehicle,"complete vehicle");
            check(s.controls.current==expected_cursor,"stored current reset");++words;
            const auto after=pmd_bytes(owner.bank,ref);check(before==after,"mounted stored frame wrote PMD");pmd_compared+=before.size();
            const auto observed_pose=pose.value();compare(std::span(reinterpret_cast<const std::uint8_t*>(&observed_pose),80),std::span(reinterpret_cast<const std::uint8_t*>(&transform),80),"model transform");
            previous=expected[0];++frames;
        }
        std::cout<<"stored_capture level="<<level<<" model="<<model_name<<" frames="<<count<<" source_oracle=historical_only\n";
    }
    check(f.peek()==std::char_traits<char>::eof()&&frames==2160&&retained==2157,"stored capture complete count");
    std::cout<<"frames="<<frames<<" retained="<<retained<<" words="<<words<<" unchanged_pmd_bytes="<<pmd_compared<<" mismatches=0 tolerance_ulp=0 excluded_words=0 actualSession_execution=0 original_execution=0\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
