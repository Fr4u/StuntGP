#include "source_pmd_speed_trail_fixture.hpp"
using namespace sgn;using namespace sgn::speed_trail_test;
namespace {
void inactive_green(const std::filesystem::path& root,bool low){
    Fixture f(root,"car1sh");const auto stale=f.view();AssetStore assets(root);ModelResources legacy(assets,root);const auto handle=*legacy.resolve(legacy.load("car1sh"));
    for(unsigned mode=0;mode<4;++mode){State state;state.tick=low?20:19;state.physics.speed=low?0.0f:100.0f;const auto before=state.record();auto expected=before;
        if(low){put(expected,0x391c,0);put(expected,0x3920,0);}
        if(mode==0)update_vehicle_speed_trail(state.binding(),f.models,f.handle);
        else if(mode==1)update_vehicle_speed_trail(state.binding(),f.models,std::size_t(-1));
        else if(mode==2){f.bank.release_42eaa0();update_vehicle_speed_trail(state.binding(),stale);}
        else update_vehicle_speed_trail(state.binding(),legacy,handle);
        check(state.record()==expected,"inactive speed gate crossed model access or wrong write footprint");
    }
    std::cout<<"GREEN "<<(low?"reset_slow":"early_tick")<<": missing-part model, invalid handle, expired view and legacy registry; source footprint exact\n";
}
void retail40(const std::filesystem::path& root){
    unsigned cars{},shadow_inactive{},shadow_active{};std::uint64_t intact{};
    for(unsigned id=1;id<=20;++id){const auto name="car"+std::to_string(id);Fixture f(root,name);AssetStore assets(root);ModelResources legacy(assets,root);const auto handle=*legacy.resolve(legacy.load(name));
        for(unsigned selector:{0u,1u,3u,0xffffffffu}){State a;a.model=legacy.at(handle).trail_model();a.physics.camera_effects.values[2]=selector;State b=a;auto m=f.view();m.current_detail_level().set(selector);m.descriptor().field<unsigned>(0x24).set(0);
            const auto before=snapshot(f.bank,m.allocation());update_vehicle_speed_trail(a.binding(),f.models,f.handle);update_vehicle_speed_trail(b.legacy_binding());check(a.record()==b.record(),"retail speed output differs");intact+=unchanged_pmd(f,before);++cars;
        }
        Fixture shadow(root,name+"sh");for(unsigned tick:{19u,20u}){State state;state.tick=tick;state.physics.speed=0;update_vehicle_speed_trail(state.binding(),shadow.models,shadow.handle);++shadow_inactive;}
        State active;rejects<FormatError>([&]{update_vehicle_speed_trail(active.binding(),shadow.models,shadow.handle);});++shadow_active;
    }
    check(cars==80&&shadow_inactive==40&&shadow_active==20,"retail speed coverage");std::cout<<"{\"car_pairs\":"<<cars<<",\"shadow_inactive\":"<<shadow_inactive<<",\"shadow_active_rejections\":"<<shadow_active<<",\"unchanged_pmd_bytes\":"<<intact<<"}\n";
}
void read_order(const std::filesystem::path& root){
    Fixture f(root);const auto m=f.view();const auto bytes=m.bytes();const auto last=transform(m,23),first=transform(m,0);
    const auto end=0x148+bytes.field<unsigned>(0x44).value()+bytes.field<unsigned>(0x44+0x94).value();pointer(m,23,end);pointer(m,0,0xffffffffu);
    auto reject=[&](std::string_view expected){State s;const auto before=s.record();bool caught=false;try{update_vehicle_speed_trail(s.binding(),f.models,f.handle);}catch(const FormatError& e){caught=std::string_view(e.what())==expected;}check(caught,"speed part access order diagnostic");check(s.record()==before,"late model rejection mutated trail state");};
    reject("PMD source byte extent");pointer(m,23,last);reject("PMD relocated pointer has wrong section/alignment");pointer(m,0,first);
    State s;s.settings.speed.capacity=0;const auto before=s.record();rejects<std::domain_error>([&]{update_vehicle_speed_trail(s.binding(),f.models,f.handle);});check(s.record()==before,"invalid host capacity wrote state");
}
void raw0_alias(const std::filesystem::path& root){
    Fixture f(root);const auto m=f.view();m.descriptor().field<unsigned>(0x48).set(m.descriptor().field<unsigned>(0x4c).value());m.current_detail_level().set(0xffffffffu);m.descriptor().field<unsigned>(0x24).set(0);
    const auto shared=transform(m,0);pointer(m,23,shared);m.bytes().field<float>(shared+8).set(37);
    State actual;for(unsigned step=0;step<3;++step){m.bytes().field<float>(shared+8).set(37.0f+float(step));State expected=actual;expected.model=observe(m);actual.model=expected.model;
        const auto before=snapshot(f.bank,m.allocation());update_vehicle_speed_trail(actual.binding(),f.models,f.handle);update_vehicle_speed_trail(expected.legacy_binding());check(actual.record()==expected.record(),"raw0 alias/current/count or latest model observation differs");unchanged_pmd(f,before);}
}
struct Audit {
    std::array<std::uint32_t,4> bits{};std::array<const float*,4> addresses{};std::array<VehicleTrailOpaqueLane,4> lanes{};unsigned count{};bool overflow{};
    static void receive(void* p,const float& value,VehicleTrailOpaqueLane lane)noexcept{auto& a=*static_cast<Audit*>(p);if(a.count>=4){a.overflow=true;return;}a.bits[a.count]=std::bit_cast<unsigned>(value);a.addresses[a.count]=&value;a.lanes[a.count]=lane;++a.count;}
};
void audit_ring(const std::filesystem::path& root){
    Fixture f(root);State s;s.physics.camera_effects.values[2]=0xffffffffu;s.physics.camera_effects.values[3]=15;s.scratch[3]=std::bit_cast<float>(0x7fc01234u);s.scratch[7]=-3;
    s.storage.speed[16].w=std::bit_cast<float>(0x7fc04321u);s.storage.speed[0].w=-7;
    Audit audit;VehicleTrailWriteAudit hook{&audit,&Audit::receive};s.audit=&hook;update_vehicle_speed_trail(s.binding(),f.models,f.handle);
    check(audit.count==4&&!audit.overflow,"speed audit count");check(audit.bits==std::array<unsigned,4>{0x7fc01234u,std::bit_cast<unsigned>(-3.0f),0x7fc04321u,std::bit_cast<unsigned>(-7.0f)},"speed source-unwritten W retention");
    check(audit.addresses==std::array<const float*,4>{&s.scratch[3],&s.scratch[7],&s.storage.speed[16].w,&s.storage.speed[0].w},"audit observed copies instead of existing owners");
    check(audit.lanes==std::array<VehicleTrailOpaqueLane,4>{VehicleTrailOpaqueLane::basis_x,VehicleTrailOpaqueLane::basis_y,VehicleTrailOpaqueLane::latest,VehicleTrailOpaqueLane::cyclic},"audit order changed");
    check(s.physics.camera_effects.values[2]==0&&s.physics.camera_effects.values[3]==16,"cursor wrap/count order");
}
void lifetime(const std::filesystem::path& root){
    Fixture f(root),other(root);const auto stale=f.view();f.bank.release_42eaa0();State s;const auto before=s.record();rejects<SourceAllocationError>([&]{update_vehicle_speed_trail(s.binding(),stale);});check(s.record()==before,"expired view wrote trail state");
    rejects<std::out_of_range>([&]{update_vehicle_speed_trail(s.binding(),other.models,std::size_t(-1));});bool caught=false;
    std::thread worker([&]{try{State foreign;update_vehicle_speed_trail(foreign.binding(),other.models,other.handle);}catch(const SourceAllocationError&){caught=true;}});worker.join();check(caught,"foreign thread active speed trail accepted");update_vehicle_speed_trail(s.binding(),other.models,other.handle);
}
}
int main(int argc,char** argv)try{check(argc==3,"expected speed trail contract and retail root");const std::string_view mode=argv[1];
    if(mode=="early_tick_green")inactive_green(argv[2],false);else if(mode=="reset_slow_green")inactive_green(argv[2],true);else if(mode=="retail40")retail40(argv[2]);else if(mode=="read_order")read_order(argv[2]);else if(mode=="raw0_alias")raw0_alias(argv[2]);else if(mode=="audit_ring")audit_ring(argv[2]);else if(mode=="lifetime")lifetime(argv[2]);else throw std::runtime_error("unknown speed trail contract");
    std::cout<<"passed "<<mode<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}
