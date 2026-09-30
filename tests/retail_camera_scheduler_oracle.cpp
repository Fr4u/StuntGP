#include "retail_camera_lifecycle_state.hpp"
#include "sgn/retail_camera_vehicle.hpp"
#include "sgn/vehicle.hpp"
#include "sgn/contact_player.hpp"
#include "sgn/model_resources.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace {
using namespace sgn::retail;
using namespace sgn::retail::camera_test;
void read(std::ifstream& in,void* p,std::size_t n) {in.read(static_cast<char*>(p),static_cast<std::streamsize>(n));if(!in)throw std::runtime_error("truncated scheduler corpus");}
struct Reset {
    std::array<VehicleState,6> physical;
    CameraEffectTargets targets;
    CameraModeReset actual;
    std::uint32_t hash{0x811c9dc5u},count{};
    CameraModeReset adapter() {return {this,actual.active,[](void* context,std::uint32_t slot) noexcept {
        auto& self=*static_cast<Reset*>(context);self.hash=(self.hash^slot)*16777619u;++self.count;
        self.actual.reset(self.actual.context,slot);
    }};}
};
}
int main(int argc,char** argv) {
    try {
        CameraMatrix shared_model_identity{}; //one scheduler fixture owner, retained across calls; not in old corpus
        CameraState valid,unsupported;CameraLifecycle valid_life,unsupported_life;
        valid.mode=5;valid.position={1,2,3,4};unsupported.mode=7;
        CameraRig rejected_rig;std::uint32_t current=3,selected=2;rejected_rig.current_slot=&current;
        rejected_rig.slots[0]={&valid,&valid_life,1,0};rejected_rig.slots[3]={&unsupported,&unsupported_life,1,1};
        std::array<CameraVehicle,6> reject_vehicles;std::array<CameraRaceContext,6> reject_races;CameraBank reject_bank;
        bool rejected=false;try{camera_tick(rejected_rig,reject_vehicles,reject_races,{0,-1,0,0},reject_bank,ModelTransform{},selected,{},{},shared_model_identity);}
        catch(const std::invalid_argument&){rejected=true;}
        if(!rejected || current!=3 || valid.position.x!=1 || selected!=2)
            throw std::runtime_error("unsupported camera partially advanced an earlier slot");
        contact::player::Controls controls;std::array<CameraLifecycle,4> controls_life;
        controls.current=3;controls.controllers[2].active=1;controls.controllers[2].vehicle=5;
        auto borrowed=camera_controls_rig(controls,controls_life,4);
        if(borrowed.current_slot!=&controls.current || borrowed.slots[2].state!=&controls.controllers[2].camera ||
           borrowed.slots[2].lifecycle!=&controls_life[2] || borrowed.slots[2].vehicle_slot!=5)
            throw std::runtime_error("camera Controls adapter duplicated or misbound state");
        if(argc!=2)throw std::runtime_error("usage: camera scheduler oracle corpus");
        std::ifstream in(argv[1],std::ios::binary);char header[8];read(in,header,8);
        if(std::memcmp(header,"SGNRIG02",8))throw std::runtime_error("wrong camera scheduler corpus");
        std::uint32_t count;read(in,&count,4);if(!count || count>10000)throw std::runtime_error("invalid scheduler count");
        std::uint64_t checked=0,mismatches=0;
        for(std::uint32_t n=0;n<count;++n) {
            CameraBank bank;read(in,&bank.count,4);read(in,bank.records.data(),sizeof(bank.records));
            ModelTransform template_pose;static_assert(sizeof(template_pose)==80);read(in,&template_pose,sizeof(template_pose));
            std::array<std::uint32_t,5> globals,expected_globals;read(in,globals.data(),sizeof(globals));
            std::array<Bytes,4> c,expected_c;std::array<Bytes,6> v,expected_v;
            for(auto* group:{&c,&expected_c})for(auto& bytes:*group){bytes.resize(camera_size);read(in,bytes.data(),bytes.size());}
            for(auto* group:{&v,&expected_v})for(auto& bytes:*group){bytes.resize(vehicle_size);read(in,bytes.data(),bytes.size());}
            read(in,expected_globals.data(),sizeof(expected_globals));
            std::array<CameraState,4> states;std::array<CameraLifecycle,4> lives;std::array<CameraVehicle,6> vehicles;
            std::array<CameraRaceContext,6> race;Reset reset;
            for(std::size_t i=0;i<6;++i) {
                Bytes dummy=c[0];CameraState ignored;transfer(dummy,v[i],ignored,vehicles[i],true);
                race[i]={real(v[i],0x2704),real(v[i],0x2708),real(v[i],0x270c)};
                reset.targets.vehicles[i]=&reset.physical[i];reset.targets.active[i]=vehicles[i].active;
                for(std::size_t k=0;k<8;++k)reset.physical[i].camera_effects.values[k]=word(v[i],0x2800+k*4);
            }
            CameraRig rig;rig.current_slot=&globals[0];rig.current_vehicle_slot=globals[1];
            for(std::size_t i=0;i<4;++i) {
                const auto target=word(c[i],0xac);if(target>=6)throw std::runtime_error("invalid fixture target");
                CameraVehicle ignored;Bytes dummy=v[target];transfer(c[i],dummy,states[i],ignored,true);lifecycle(c[i],lives[i],true);
                rig.slots[i]={&states[i],&lives[i],word(c[i],0),target};
            }
            reset.actual=camera_vehicle_reset_adapter(reset.targets);
            camera_tick(rig,vehicles,race,{0,-1,0,0},bank,template_pose,globals[2],reset.adapter(),{},shared_model_identity);
            globals[3]=reset.hash;globals[4]=reset.count;
            for(std::size_t i=0;i<4;++i) {
                CameraVehicle ignored;Bytes dummy=v[0];transfer(c[i],dummy,states[i],ignored,false);lifecycle(c[i],lives[i],false);
            }
            for(std::size_t i=0;i<6;++i) {
                Bytes dummy=c[0];CameraState ignored;transfer(dummy,v[i],ignored,vehicles[i],false);
                for(std::size_t k=0;k<8;++k)put(v[i],0x2800+k*4,reset.physical[i].camera_effects.values[k]);
            }
            auto compare=[&](const auto& a,const auto& b,const char* label,std::size_t limit) {
                for(std::size_t at=0;at<limit;at+=4) {
                    ++checked;if(word(a,at)==word(b,at))continue;
                    if(mismatches<24)std::cerr<<"case="<<n<<" "<<label<<"+"<<std::hex<<at<<" actual="<<word(a,at)<<" expected="<<word(b,at)<<std::dec<<'\n';
                    ++mismatches;
                }
            };
            // 200..2ff is the individual oracle's matrix-cache protocol, not
            // per-camera source storage: do not count those as comparisons.
            for(std::size_t i=0;i<4;++i)compare(c[i],expected_c[i],("camera"+std::to_string(i)).c_str(),0x200);
            for(std::size_t i=0;i<6;++i)compare(v[i],expected_v[i],("vehicle"+std::to_string(i)).c_str(),vehicle_size);
            for(std::size_t i=0;i<globals.size();++i){++checked;if(globals[i]!=expected_globals[i]){++mismatches;if(mismatches<24)std::cerr<<"case="<<n<<" global="<<i<<" actual="<<globals[i]<<" expected="<<expected_globals[i]<<'\n';}}
        }
        if(in.peek()!=std::char_traits<char>::eof())throw std::runtime_error("trailing scheduler data");
        std::cout<<"cases="<<count<<" words="<<checked<<" mismatches="<<mismatches<<" tolerance_ulp=0\n";
        return mismatches?1:0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
