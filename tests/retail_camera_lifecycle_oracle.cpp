#include "retail_camera_lifecycle_state.hpp"
#include "sgn/retail_camera_lifecycle.hpp"
#include "sgn/assets.hpp"
#include "sgn/model_resources.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
using namespace sgn::retail;
using namespace sgn::retail::camera_test;
void read(std::ifstream& in,void* p,std::size_t n) {in.read(static_cast<char*>(p),static_cast<std::streamsize>(n));if(!in)throw std::runtime_error("truncated lifecycle corpus");}
void write(std::ofstream& out,const void* p,std::size_t n) {out.write(static_cast<const char*>(p),static_cast<std::streamsize>(n));if(!out)throw std::runtime_error("failed lifecycle corpus output");}
void write_word(std::ofstream& out,std::uint32_t x) {write(out,&x,4);}
void apply(std::uint32_t entry,Bytes& c,Bytes& v,const CameraBank& bank,const ModelTransform& template_pose) {
    CameraMatrix shared_model_identity{}; //isolated fixture owner; old corpus does not compare61C468
    CameraState s;CameraVehicle vehicle;CameraLifecycle life;
    transfer(c,v,s,vehicle,true);lifecycle(c,life,true);
    switch(entry) {
    case 0x4023d0:camera_clear_velocity(s);break;
    case 0x4023f0:camera_clear_acceleration(s);break;
    case 0x4028f0:camera_request_record(life,word(v,0x27a0)?std::optional(word(v,0x27a0)-1):std::nullopt,word(v,0x27a4));break;
    case 0x402950:camera_apply_record(s,life,vehicle,bank,template_pose,shared_model_identity);break;
    case 0x403800:put(v,0x279c,camera_select_authored(life,vehicle,bank)+1);break;
    case 0x4038c0:camera_authored_lens(s,life,vehicle,bank);break;
    case 0x4039b0:camera_prepare_authored(s,life,vehicle,bank);break;
    case 0x402469:camera_initialize_values(s,life,shared_model_identity);break;
    default:throw std::runtime_error("unknown lifecycle case");
    }
    transfer(c,v,s,vehicle,false);lifecycle(c,life,false);
}
void contracts() {
    CameraMatrix shared_model_identity{}; //contract fixture owner
    CameraBank bank;bank.count=7;const auto before=bank;
    bool rejected=false;try {camera_load_authored_text(bank,"-CAMERA[]\nTotalCameraCnt=33\n");}catch(const sgn::FormatError&){rejected=true;}
    if(!rejected || bank.count!=before.count)throw std::runtime_error("invalid camera count partially mutated bank");
    CameraState state;CameraLifecycle life;CameraVehicle vehicle;life.pending_record=32;state.mode=6;rejected=false;
    try {camera_apply_record(state,life,vehicle,bank,ModelTransform{},shared_model_identity);}catch(const std::invalid_argument&){rejected=true;}
    if(!rejected || state.mode!=6)throw std::runtime_error("invalid record partially mutated camera");
}
}
int main(int argc,char** argv) {
    try {
        contracts();
        if(argc==4 && std::string_view(argv[1])=="--export-banks") {
            sgn::AssetStore assets(argv[2]);auto names=assets.resources("config/cams/");
            std::vector<std::string> files;for(const auto& name:names)if(name.ends_with(".cam"))files.push_back(name);
            if(files.empty())throw std::runtime_error("no original camera text resources");
            std::ofstream out(argv[3],std::ios::binary);write(out,"SGNBANK2",8);write_word(out,static_cast<std::uint32_t>(files.size()));
            for(const auto& name:files) {
                const auto raw=assets.resource(name);CameraBank bank;std::memset(bank.records.data(),0xa5,sizeof(bank.records));
                std::string error;
                try {camera_load_authored_text(bank,{reinterpret_cast<const char*>(raw.data()),raw.size()});}
                catch(const sgn::FormatError& e) {error=e.what();}
                write_word(out,static_cast<std::uint32_t>(name.size()));write(out,name.data(),name.size());
                write_word(out,static_cast<std::uint32_t>(raw.size()));write(out,raw.data(),raw.size());
                write_word(out,static_cast<std::uint32_t>(error.size()));write(out,error.data(),error.size());
                write_word(out,bank.count);write(out,bank.records.data(),sizeof(bank.records));
            }
            std::cout<<"authored camera resources="<<files.size()<<'\n';return 0;
        }
        if(argc!=2)throw std::runtime_error("usage: camera lifecycle oracle corpus.dat");
        std::ifstream in(argv[1],std::ios::binary);char signature[8];read(in,signature,8);
        if(std::memcmp(signature,"SGNLIF02",8))throw std::runtime_error("wrong lifecycle corpus");
        std::uint32_t count;read(in,&count,4);if(!count||count>10000)throw std::runtime_error("invalid lifecycle count");
        std::uint64_t words=0,mismatches=0;
        for(std::uint32_t n=0;n<count;++n) {
            std::uint32_t entry;read(in,&entry,4);CameraBank bank;read(in,&bank.count,4);read(in,bank.records.data(),sizeof(bank.records));
            ModelTransform template_pose;static_assert(sizeof(template_pose)==80);read(in,&template_pose,sizeof(template_pose));
            Bytes c(camera_size),v(vehicle_size),expected_c(camera_size),expected_v(vehicle_size);
            read(in,c.data(),c.size());read(in,v.data(),v.size());read(in,expected_c.data(),expected_c.size());read(in,expected_v.data(),expected_v.size());
            apply(entry,c,v,bank,template_pose);
            auto compare=[&](const Bytes& a,const Bytes& b,const char* label) {
                for(std::size_t at=0;at<a.size();at+=4) {
                    ++words;if(word(a,at)==word(b,at))continue;
                    if(mismatches<20)std::cerr<<"case="<<n<<" entry="<<std::hex<<entry<<" "<<label<<"+"<<at<<" actual="<<word(a,at)<<" expected="<<word(b,at)<<std::dec<<'\n';
                    ++mismatches;
                }
            };
            compare(c,expected_c,"camera");compare(v,expected_v,"vehicle");
        }
        if(in.peek()!=std::char_traits<char>::eof())throw std::runtime_error("trailing lifecycle corpus");
        std::cout<<"cases="<<count<<" words="<<words<<" mismatches="<<mismatches<<" tolerance_ulp=0\n";
        return mismatches?1:0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
