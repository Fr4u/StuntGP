#include "source_pmd_pose_fixture.hpp"
#include <iostream>
#include <thread>
namespace {
using namespace sgn;using namespace sgn::retail;using namespace sgn::retail::prepare_full_test;
template<class F>void rejects(F action){bool caught=false;try{action();}catch(const SourceAllocationError&){caught=true;}require(caught,"missing checked pose lifetime rejection");}
Bytes snapshot(SourceAllocationBank& bank,SourceAllocationRef ref){const auto bytes=bank.bytes(ref);return {bytes.begin(),bytes.end()};}
struct Fixture {
    SourceAllocationBank bank;AssetStore assets;ModelResources models;std::size_t handle;
    explicit Fixture(const std::filesystem::path& root):assets(root),models(assets,root){bank.initialize_42eab0();handle=*models.resolve(models.load_source(bank,"car1"));}
    SourcePmdModelView model(){return models.source_at(handle);}
    void seed(){const auto m=model();const auto run=m.part(m.source_lod_index(0),0).transform_run(25);
        for(unsigned i=0;i<25;++i){SourcePmdTransform t{{float(i),float(i+1),float(i+2),float(i+3)},{2,3,4,float(i+10)},
            {0,0,1,std::bit_cast<float>(0x7fc12000u+i)},{0,1,0,std::bit_cast<float>(0x7fc24000u+i)},{17,19,23,29}};run.at(i).set(t);}}
};
struct Inputs:VehicleOwners {
    VehicleMatrix basis=vehicle_identity();
    Inputs(){physics.body_heave=1;physics.body_roll=physics.body_pitch=physics.steering=0;
        physics.position={20,30,40,50};physics.basis_x={1,0,0,3};physics.up={0,1,0};physics.basis_z={0,0,1,5};
        inputs.field_28a0=1;presentation.field_3134=1;anchors.position={1,2,3,4};anchors.extent=2;
        anchors.effect_available={2,3};anchors.effect_position={VehicleVector{1,2,3,4},VehicleVector{4,5,6,7}};
        for(unsigned i=0;i<4;++i){wheels[i].rotation=vehicle_identity();wheels[i].rotation[7]=float(i+20);wheels[i].rotation[11]=float(i+30);}
        for(unsigned i=0;i<6;++i)segments.items[i].direction={float(i+1),float(i+2),float(i+3),float(i+100)};
        effect={8,9,10,11};basis[3]=31;basis[7]=37;}
    void run(Fixture& f){prepare_vehicle_model(physics,inputs,wheels,presentation,anchors,segments,f.models,f.handle,basis,effect);}
    void run(SourcePmdModelView model){prepare_vehicle_model(physics,inputs,wheels,presentation,anchors,segments,model,basis,effect);}
};
void allow(std::set<unsigned>& allowed,unsigned at,unsigned n){for(unsigned i=0;i<n;++i)allowed.insert(at+i);}
void footprint(const std::filesystem::path& root){
    Fixture f(root);f.seed();Inputs in;auto model=f.model();model.current_detail_level().set(3);const auto lod=model.source_lod_index(3);
    const auto other_id=f.models.load_source(f.bank,"car1");const auto other=f.models.source_at(*f.models.resolve(other_id));
    const auto isolated=snapshot(f.bank,other.allocation()),before=snapshot(f.bank,model.allocation());std::set<unsigned> allowed;
    for(unsigned p:{0u,5u,10u}){const auto at=model.part(lod,p).transform().location().offset;allow(allowed,at+4,4);allow(allowed,at+32,12);allow(allowed,at+48,12);}
    for(unsigned p:{1u,2u,3u,4u}){const auto at=model.part(lod,p).transform().location().offset;const auto bytes=p<3?16u:12u;allow(allowed,at+32,bytes);allow(allowed,at+48,bytes);}
    const auto run=model.part(lod,11).transform_run(6);for(unsigned i=0;i<6;++i){const auto at=run.at(i).location().offset;allow(allowed,at,12);allow(allowed,at+32,16);}
    for(unsigned p:{19u,20u}){const auto at=model.part(lod,p).transform().location().offset;allow(allowed,at,12);allow(allowed,at+32,32);}
    in.run(f);const auto after=snapshot(f.bank,model.allocation());require(before.size()==after.size(),"pose changed allocation extent");
    for(unsigned i=0;i<before.size();++i)if(before[i]!=after[i])require(allowed.contains(i),"pose wrote outside exact scalar footprint");
    require(snapshot(f.bank,other.allocation())==isolated,"pose mutated another model instance");
    require(model.current_detail_level().value()==3u&&std::bit_cast<std::array<unsigned,4>>(in.effect)[3]==11u&&in.basis[3]==31&&in.basis[7]==37,"pose changed owner LOD or retained lane");
}
void aliases(const std::filesystem::path& root){
    Fixture f(root);f.seed();auto model=f.model();Inputs in;in.anchors.effect_available={0,1};
    const auto p12=model.part(0,12);model.bytes().field<unsigned>(p12.location().offset+8).set(model.part(0,0).transform().location().offset);
    in.run(f);const auto body=model.part(0,0).transform().value(),segment=model.part(0,11).transform_run(6).at(1).value();
    require(body.position.y==1&&body.forward.x==0&&body.forward.y==0&&body.forward.z==1,"segment followed part12 instead of contiguous run");
    //413B7B..413B8C gives cross(forward,up).x = 0*0-1*1 = -1.
    require(segment.forward.x==-2&&segment.forward.y==3&&segment.forward.z==4&&segment.forward.w==101,"contiguous segment record not written");
    in.presentation.field_3134=0;in.anchors.effect_available={2,3};
    const auto shared=model.part(0,18).transform();
    for(unsigned p:{19u,20u})model.bytes().field<unsigned>(model.part(0,p).location().offset+8).set(shared.location().offset);
    in.run(f);const auto t=shared.value();require(t.position.x==-4&&t.position.y==6&&t.position.z==6,"effect alias did not retain second write/nonzero guard");
    require(std::bit_cast<unsigned>(t.up.w)==std::bit_cast<unsigned>(model.part(0,0).transform().value().up.w),"effect did not copy opaque body W");
    in.anchors.effect_available={0,1};const auto before=std::bit_cast<std::array<unsigned,20>>(shared.value());in.run(f);
    require(before==std::bit_cast<std::array<unsigned,20>>(shared.value()),"second effect escaped first availability guard");
}
void lifetime(const std::filesystem::path& root){
    Fixture f(root),other(root);f.seed();Inputs in;auto model=f.model();const auto retained=model.part(0,11).transform_run(6).at(2);
    const auto allocation=model.allocation();const auto persistent=f.bank.allocate_42e8d0(4,1);f.bank.bytes(persistent)[0]=71;
    f.bank.release_42eaa0();const auto before_basis=in.basis;const auto before_effect=std::bit_cast<std::array<float,4>>(in.effect);
    rejects([&]{in.run(model);});rejects([&]{(void)retained.value();});
    require(in.basis==before_basis&&std::bit_cast<std::array<float,4>>(in.effect)==before_effect,"expired call changed outputs before rejection");
    require(f.models.source_name(f.handle)=="car1"&&f.bank.bytes(persistent)[0]==71,"release lost metadata or category1 control");
    in.run(other);f.bank.begin_42ea80();const auto id=f.models.load_source(f.bank,"car1");const auto replacement=f.models.source_at(*f.models.resolve(id));
    require(replacement.allocation()!=allocation,"scope reused generation");rejects([&]{in.run(model);});in.run(replacement);
    bool caught=false;std::thread thread([&]{try{in.run(other);}catch(const SourceAllocationError&){caught=true;}});thread.join();require(caught,"foreign pose thread accepted");
    SourcePmdModelView expired;{Fixture local(root);expired=local.model();}rejects([&]{in.run(expired);});
}
void dispatch(const std::filesystem::path& root){
    Fixture f(root);f.seed();Inputs canonical,legacy;AssetStore assets(root);ModelResources models(assets,root);const auto id=models.load("car1");const auto handle=*models.resolve(id);
    auto& model=models.at(handle);const auto source=f.model().part(0,0).transform_run(25);auto target=model.source_transform_run(0,0,25);
    for(unsigned i=0;i<25;++i)target[i]=std::bit_cast<ModelTransform>(source.at(i).value());
    unsigned selected=0;model.bind_current_detail_level(selected);selected=2;f.model().current_detail_level().set(2);
    canonical.run(f);prepare_vehicle_model(legacy.physics,legacy.inputs,legacy.wheels,legacy.presentation,legacy.anchors,legacy.segments,models,handle,legacy.basis,legacy.effect);
    for(unsigned i=0;i<25;++i)require(std::bit_cast<std::array<unsigned,20>>(target[i])==std::bit_cast<std::array<unsigned,20>>(source.at(i).value()),"registry dispatch transform mismatch");
    require(std::bit_cast<std::array<unsigned,16>>(canonical.basis)==std::bit_cast<std::array<unsigned,16>>(legacy.basis)&&
        std::bit_cast<std::array<unsigned,4>>(canonical.effect)==std::bit_cast<std::array<unsigned,4>>(legacy.effect)&&selected==2u,"registry owner/output mismatch");
}
}
int main(int argc,char** argv)try{
    require(argc==3,"expected pose contract and retail root");const std::string_view name=argv[1];
    if(name=="footprint")footprint(argv[2]);else if(name=="aliases")aliases(argv[2]);else if(name=="lifetime")lifetime(argv[2]);else if(name=="dispatch")dispatch(argv[2]);else throw FormatError("unknown pose contract");
    std::cout<<"canonical_pose_contract="<<name<<" errors=0 activation=false\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
