#include "source_pmd_mounted_fixture.hpp"
using namespace sgn;using namespace sgn::mounted_test;
int main(int argc,char** argv)try{
    check(argc==3,"eager probe requires kind and retail root");const std::string_view kind=argv[1];Fixture f(argv[2]);auto& s=*f.state;
    if(kind=="legacy_later"){
        ModelResources legacy(f.assets,argv[2]);const auto h=*legacy.resolve(legacy.load("car1"));s.vehicles.models[0].model=h;
        s.controls.controllers[1].active=1;s.vehicles.models[1].model=99;const auto old=s.controls.controllers[0].camera;
        auto world=f.world();world.models=&legacy;bool rejected=false;
        try{
#ifdef SGN_MOUNTED_FROZEN
            camera_frame_before_world(world);
#else
            call_frame(world);
#endif
        }catch(const std::out_of_range&){rejected=true;}
        const bool complete=camera_bytes(old)!=camera_bytes(s.controls.controllers[0].camera);
        std::cout<<"eager_probe=legacy_later rejected="<<rejected<<" previous_completed="<<complete<<" current="<<s.controls.current<<'\n';
        check(rejected&&complete&&s.controls.current==1,"legacy later identity preflight discarded completed controller");
    }else{
        check(kind=="canonical_prefix","unknown eager probe");auto world=f.world();f.bank.release_42e980(f.model().allocation());bool rejected=false;
        try{
#ifdef SGN_MOUNTED_FROZEN
            camera_frame_before_world(world);
#else
            call_frame(world);
#endif
        }catch(const std::exception&){rejected=true;}
        auto expected=camera();auto ev=vehicle();expected_prefix(expected,ev,{},false);
        const bool prefix=camera_bytes(s.controls.controllers[0].camera)==camera_bytes(expected)&&vehicle_bytes(s.vehicles.camera_snapshot(0))==vehicle_bytes(ev);
        std::cout<<"eager_probe=canonical_prefix rejected="<<rejected<<" complete_prefix="<<prefix<<'\n';
        check(rejected&&prefix,"canonical eager barrier discards mounted prefix");
    }
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
