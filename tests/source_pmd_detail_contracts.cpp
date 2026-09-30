#include "sgn/vehicle_model_detail.hpp"
#include <bit>
#include <iostream>
#include <set>
#include <thread>
namespace {
using namespace sgn;using namespace sgn::retail;
void check(bool value,const char* text){if(!value)throw std::runtime_error(text);}
template<class F>void rejects(F action){bool caught=false;try{action();}catch(const SourceAllocationError&){caught=true;}check(caught,"missing checked allocation rejection");}
Bytes snapshot(const SourceAllocationBank& bank,SourceAllocationRef ref){const auto b=bank.bytes(ref);return {b.begin(),b.end()};}
struct Fixture {
    SourceAllocationBank bank;AssetStore assets;ModelResources models;std::size_t handle;
    explicit Fixture(const std::filesystem::path& root):assets(root),models(assets,root){bank.initialize_42eab0();handle=*models.resolve(models.load_source(bank,"car1"));}
    SourcePmdModelView model(){return models.source_at(handle);}
};
struct Inputs {
    VehicleState vehicle;VehiclePresentationState presentation;contact::effects::VehicleParticleState particles;
    VehiclePresentationEffects effects{1,1,1,{2,3,4},{5,6,7}};CameraState camera;CameraVector forward{0,0,1,0};
    Inputs(){camera.lens=1;vehicle.position={0,0,100,1};vehicle.previous_position={0,0,2000,1};
        vehicle.brake_signal=123;for(auto& wheel:vehicle.wheels)wheel.braking_signal=42;
        presentation.field_3134=13;presentation.field_3924=19;presentation.field_3928=23;
        presentation.field_33c8=29;presentation.field_3508=31;particles.field_7628=37;}
    void run(Fixture& f){prepare_vehicle_model_detail(vehicle,presentation,particles,effects,f.models,f.handle,camera,forward,60);}
};
void allow(std::set<std::uint32_t>& offsets,std::uint32_t start,std::uint32_t bytes){for(std::uint32_t i=0;i<bytes;++i)offsets.insert(start+i);}
void footprint(const std::filesystem::path& root){
    Fixture f(root);const auto model=f.model();Inputs in;
    const auto other_id=f.models.load_source(f.bank,"car1");const auto other_handle=*f.models.resolve(other_id);
    const auto other=f.models.source_at(other_handle);const auto isolated=snapshot(f.bank,other.allocation());
    for(bool distant:{false,true}){
        in.vehicle.position.z=distant?4000.f:100.f;const auto selected=distant?3u:0u,lod=model.source_lod_index(selected);
        // Seed every flag and all transform bytes so forbidden clearing is visible.
        for(unsigned slot=0;slot<4;++slot)for(unsigned part=0;part<model.part_count();++part){const auto p=model.part(model.source_lod_index(slot),part);
            p.flags().set(0x3210);p.flags_02().set(0xabcd);auto t=p.transform().value();t.scale={9,10,11,std::bit_cast<float>(0x7fc12345u)};
            t.tail={0x13579u,0x24680u,0x98765u,0x54321u};p.transform().set(t);}
        const auto before=snapshot(f.bank,model.allocation());std::set<std::uint32_t> writes;
        allow(writes,model.current_detail_level().location().offset,4);allow(writes,model.previous_detail_level().location().offset,4);
        for(unsigned part=0;part<model.part_count();++part){const bool flag=distant?
            ((part>=5&&part<=9)||(part>=11&&part<=16)||part==19||part==20):((part>=11&&part<=16)||part==19||part==20);
            if(flag)allow(writes,model.part(lod,part).flags().location().offset,2);}
        if(!distant)for(unsigned part:{19u,20u})allow(writes,model.part(lod,part).transform().location().offset+0x10,12);
        in.run(f);const auto after=snapshot(f.bank,model.allocation());check(before.size()==after.size(),"detail changed allocation extent");
        for(std::uint32_t i=0;i<before.size();++i)if(before[i]!=after[i])check(writes.contains(i),"detail wrote outside PE scalar footprint");
        check(model.current_detail_level().value()==selected&&model.previous_detail_level().value()==2u,"canonical descriptor LOD values wrong");
        check(f.models.current_detail_level(f.handle)==selected,"registry copied current LOD");
        check(isolated==snapshot(f.bank,other.allocation()),"detail mutated another equal-name model");
        if(distant)check(in.vehicle.brake_signal==0&&in.presentation.field_3928==0&&in.particles.field_7628==0,"far side effects wrong");
        else check(in.presentation.field_3928==19u&&in.particles.field_7628==37u,"near side effects wrong");
    }
}
void aliases(const std::filesystem::path& root){
    Fixture f(root);const auto model=f.model();Inputs in;const auto d=model.descriptor();
    // Legal current descriptor pointers: slot0 aliases the relocated last table.
    const auto last=model.maximum_detail_level();d.field<std::uint32_t>(0x48).set(d.field<std::uint32_t>(0x48+last*4).value());
    const auto first=model.part(0,19),second=model.part(0,20);
    // Legal source transform alias: both effects share the same current pointer.
    model.bytes().field<std::uint32_t>(second.location().offset+8).set(first.transform().location().offset);
    auto initial=first.transform().value();initial.scale={9,10,11,12};first.transform().set(initial);
    in.run(f);auto actual=first.transform().value();check(actual.scale.x==5&&actual.scale.y==6&&actual.scale.z==7&&actual.scale.w==12,"effect alias did not observe last scalar writes");
    check(model.part(last,19).flags().value()==1&&model.part(last,20).flags().value()==1,"current descriptor alias ignored");
    in.effects.field_328c=2;in.effects.field_33cc=1;first.flags().set(17);second.flags().set(19);first.transform().set(initial);
    in.run(f);check(first.flags().value()==17&&second.flags().value()==19&&first.transform().value().scale.x==9,"second activation escaped exact-one first guard");
    in.effects.field_226c=0;in.run(f);check(first.flags().value()==17&&second.flags().value()==0,"disabled second clear incorrectly nested");
}
void lifetime(const std::filesystem::path& root){
    Fixture f(root),other(root);Inputs in;const auto model=f.model();const auto current=model.current_detail_level(),previous=model.previous_detail_level();
    const auto flags=model.part(0,19).flags();const auto scale=model.bytes().field<float>(model.part(0,19).transform().location().offset+0x10);
    const auto identity=f.models.identity(f.handle);const auto old_allocation=model.allocation();
    const auto persistent=f.bank.allocate_42e8d0(16,1);f.bank.bytes(persistent)[0]=0x6d;
    f.bank.release_42eaa0();check(f.models.source_name(f.handle)=="car1"&&f.models.identity(f.handle)==identity,"release erased metadata identity");
    check(f.bank.bytes(persistent)[0]==0x6d&&f.bank.live_count()==1,"category1 was not retained");
    rejects([&]{(void)current.value();});rejects([&]{previous.set(0);});rejects([&]{flags.set(1);});rejects([&]{scale.set(1);});rejects([&]{in.run(f);});
    in.run(other);check(other.models.current_detail_level(other.handle)==0u,"independent bank affected by release");
    f.bank.begin_42ea80();const auto new_id=f.models.load_source(f.bank,"car1");const auto new_handle=*f.models.resolve(new_id);
    check(f.models.source_at(new_handle).allocation()!=old_allocation,"new scope reused old allocation generation");
    f.models.source_at(new_handle).current_detail_level().set(2);rejects([&]{current.set(3);});rejects([&]{flags.set(0);});
    check(f.models.current_detail_level(new_handle)==2u,"expired generation wrote to new model");
    bool rejected=false;std::thread foreign([&]{try{(void)other.models.current_detail_level(other.handle);}catch(const SourceAllocationError&){rejected=true;}});foreign.join();check(rejected,"foreign field thread accepted");
    auto metadata_view=other.model();{ModelResources local(other.assets,root);auto id=local.load_source(other.bank,"car2");metadata_view=local.source_at(*local.resolve(id));}
    rejects([&]{(void)metadata_view.current_detail_level().value();});
}
void dispatch(const std::filesystem::path& root){
    AssetStore assets(root);ModelResources legacy(assets,root);const auto id=legacy.load("car1");const auto handle=*legacy.resolve(id);
    std::uint32_t selected=99;legacy.at(handle).bind_current_detail_level(selected);Inputs in;
    prepare_vehicle_model_detail(in.vehicle,in.presentation,in.particles,in.effects,legacy,handle,in.camera,in.forward,60);
    check(selected==0u&&legacy.current_detail_level(handle)==0u&&legacy.at(handle).previous_detail_level()==2u,"legacy bound owner disconnected");
    selected=3;check(legacy.current_detail_level(handle)==3u,"legacy read returned a stale copy");
    Fixture f(root);auto current=f.model().current_detail_level();current.set(2);check(f.models.current_detail_level(f.handle)==2u,"canonical read returned a stale copy");
    in.run(f);check(current.value()==0u,"canonical dispatch wrote a separate selected field");
}
}
int main(int argc,char** argv)try{
    check(argc==3,"expected footprint|aliases|lifetime|dispatch and retail root");const std::string_view name=argv[1];
    if(name=="footprint")footprint(argv[2]);else if(name=="aliases")aliases(argv[2]);else if(name=="lifetime")lifetime(argv[2]);else if(name=="dispatch")dispatch(argv[2]);else throw std::runtime_error("unknown detail contract");
    std::cout<<"canonical_detail_contract="<<name<<" errors=0 activation=false\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
