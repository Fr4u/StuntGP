#include "sgn/vehicle_draw_pose.hpp"
#include <bit>
#include <iostream>
#include <set>
#include <thread>
namespace {
using namespace sgn;using namespace sgn::retail;
void check(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
template<class Error,class F>void rejects(F action){bool caught=false;try{action();}catch(const Error&){caught=true;}check(caught,"missing typed vehicle draw rejection");}
Bytes snapshot(SourceAllocationBank& bank,SourceAllocationRef allocation){const auto bytes=bank.bytes(allocation);return {bytes.begin(),bytes.end()};}
struct Fixture {
    SourceAllocationBank bank;AssetStore assets;ModelResources models;std::size_t handle;
    explicit Fixture(const std::filesystem::path& root,std::string_view name="car1"):assets(root),models(assets,root){bank.initialize_42eab0();
        handle=*models.resolve(models.load_source(bank,std::string(name)));}
    SourcePmdModelView model(){return models.source_at(handle);}
};
struct Inputs {
    VehicleState vehicle;VehicleModelEffects effects;VehicleRaceState race;VehicleModelStorage storage;
    ModelDrawQueue regular,background;ViewMatrixArena matrices;ModelTransform pose;
    Inputs(){vehicle.position={1,2,3,std::bit_cast<float>(0x7fc11223u)};vehicle.up={0,1,0};vehicle.basis_y_w=std::bit_cast<float>(0xf1234567u);
        vehicle.basis_z={0,0,1,std::bit_cast<float>(0x80000000u)};effects.route_envelope=std::bit_cast<unsigned>(0.5f);race.collision_hold=60;
        pose.scale={2,3,4};pose.scale_w=0x7fc12345;pose.tail={77,0,0,0xe7654321};reset_model_draw_queues(regular,background);reset_view_matrix_cursor(matrices);}
    void run(ModelResources& models,std::size_t handle){append_vehicle_model_draw(vehicle,65,effects,race,storage,models,handle,regular,matrices,pose);}
    void run(Fixture& f){run(f.models,f.handle);}
};
void allow(std::set<unsigned>& values,unsigned at,unsigned count){for(unsigned i=0;i<count;++i)values.insert(at+i);}
void footprint(const std::filesystem::path& root){
    Fixture f(root);Inputs in;const auto model=f.model();model.current_detail_level().set(2);
    model.descriptor().field<unsigned>(4).set(0xa5c5beefu);const auto field=model.ambience();
    check(field.location().offset==model.descriptor().location().offset+4&&field.location().allocation==model.allocation(),"ambience has a second owner");
    field.set(0x1234);check(model.descriptor().field<unsigned>(4).value()==0xa5c51234,"ambience alias wrote high WORD");
    model.descriptor().field<std::uint8_t>(5).set(0x56);check(field.value()==0x5634,"ambience cached descriptor value");
    const auto before=snapshot(f.bank,model.allocation());const auto template_before=std::bit_cast<std::array<unsigned,20>>(in.pose);
    std::set<unsigned> allowed;allow(allowed,model.descriptor().location().offset+2,4);
    const auto lod=model.source_lod_index(2);
    for(unsigned i=0;i<model.part_count();++i){const auto part=model.part(lod,i);allow(allowed,part.flags_02().location().offset,2);allow(allowed,part.transform().location().offset+64,8);}
    in.run(f);const auto after=snapshot(f.bank,model.allocation());
    for(unsigned i=0;i<before.size();++i)if(before[i]!=after[i])check(allowed.contains(i),"draw changed bytes outside source footprint");
    check(model.descriptor().field<unsigned>(4).value()==0xa5c50100,"ambience changed descriptor high WORD or used wrong multiplier");
    check(model.current_detail_level().value()==2&&in.storage.field_2708==16&&in.regular.cursor==1u&&in.background.cursor==0u,"draw used wrong owner or queue");
    check(in.regular.records[0].model_identity==f.models.identity(f.handle)&&in.regular.records[1].model_identity==0,"wrong draw identity/terminator");
    const auto& queued=in.regular.records[0].pose;
    check(queued.position.x==1&&queued.position.y==-3&&queued.position.z==3&&queued.forward.z==4,"wrong contact-offset/target pose");
    check(queued.position_w==0x7fc11223&&queued.up_w==0xf1234567&&queued.forward_w==0x80000000&&queued.scale_w==0x7fc12345&&queued.tail[0]==0&&queued.tail[3]==0xe7654321,"copied W/tail or queue tail zero lost");
    check(template_before==std::bit_cast<std::array<unsigned,20>>(in.pose),"draw changed live template");
}
void order(const std::filesystem::path& root){
    Fixture f(root);Inputs in;const auto view=f.model();view.descriptor().field<unsigned>(4).set(0xabcdef01);in.storage.field_2708=91;
    in.regular.cursor=167;const auto cursor=in.matrices.cursor;const auto count=in.matrices.count;
    rejects<FormatError>([&]{in.run(f);});
    check(view.descriptor().field<unsigned>(4).value()==0xabcd0100&&in.storage.field_2708==16,"ambience/blink did not precede queue admission");
    check(in.regular.cursor==167u&&in.matrices.cursor==cursor&&in.matrices.count==count,"queue admission failure mutated matrices");
    in.regular.cursor=0;view.current_detail_level().set(4);in.effects.route_envelope=std::bit_cast<unsigned>(-0.5f);in.race.collision_hold=2;
    rejects<FormatError>([&]{in.run(f);});check(view.ambience().value()==0xff00&&in.storage.field_2708==0&&in.regular.cursor==0u,"current invalid LOD did not reject after prior writers");
    view.current_detail_level().set(3);in.run(f);check(in.regular.cursor==1u&&view.current_detail_level().value()==3,"draw cached previous LOD instead of current owner");
    //Admission and extent guards are host-only. No claim about original invalid memory.
}
void lifetime(const std::filesystem::path& root){
    Fixture f(root),other(root);Inputs in;const auto view=f.model(),other_view=other.model();const auto field=view.ambience();const auto old=view.allocation();
    const auto persistent=f.bank.allocate_42e8d0(8,1);f.bank.bytes(persistent)[0]=97;f.bank.release_42eaa0();
    rejects<SourceAllocationError>([&]{(void)field.value();});rejects<SourceAllocationError>([&]{field.set(0);});
    in.storage.field_2708=99;rejects<SourceAllocationError>([&]{in.run(f);});
    check(in.storage.field_2708==99&&in.regular.cursor==0u&&in.matrices.count==0,"expired ambience changed later owners");
    check(f.models.source_name(f.handle)=="car1"&&f.bank.bytes(persistent)[0]==97,"release lost metadata or persistent allocation");
    in.run(other);check(other_view.ambience().value()==256,"release affected another bank");
    f.bank.begin_42ea80();const auto replacement=f.models.source_at(*f.models.resolve(f.models.load_source(f.bank,"car1")));
    check(replacement.allocation()!=old,"new model reused old allocation generation");rejects<SourceAllocationError>([&]{field.set(123);});
    bool rejected=false;std::thread foreign([&]{try{update_vehicle_draw_ambience(in.effects,other_view);}catch(const SourceAllocationError&){rejected=true;}});foreign.join();check(rejected,"foreign-thread ambience write accepted");
    SourcePmdModelView dead;{Fixture temporary(root);dead=temporary.model();}rejects<SourceAllocationError>([&]{(void)dead.ambience();});
}
void dispatch(const std::filesystem::path& root){
    unsigned calls=0;for(unsigned car=1;car<=20;++car){const auto name="car"+std::to_string(car);Fixture f(root,name);AssetStore assets(root);ModelResources legacy(assets,root);
        const auto handle=*legacy.resolve(legacy.load(name));auto& old=legacy.at(handle);unsigned selected=0;old.bind_current_detail_level(selected);const auto view=f.model();
        for(unsigned lod=0;lod<4;++lod)for(unsigned hold:std::array<unsigned,4>{0,3,59,60}){
            selected=lod;view.current_detail_level().set(lod);Inputs a,b;a.race.collision_hold=hold;b.race.collision_hold=hold;a.run(f);b.run(legacy,handle);++calls;
            check(std::bit_cast<std::array<unsigned,168*21>>(a.regular.records)==std::bit_cast<std::array<unsigned,168*21>>(b.regular.records),"canonical/legacy registry queue mismatch");
            check(a.regular.cursor==b.regular.cursor&&a.matrices.cursor==b.matrices.cursor&&a.matrices.count==b.matrices.count&&
                std::bit_cast<std::array<unsigned,384*16>>(a.matrices.matrices)==std::bit_cast<std::array<unsigned,384*16>>(b.matrices.matrices),"canonical/legacy registry matrices mismatch");
            check(view.ambience().value()==old.source_model_ambience()&&view.descriptor().field<std::uint16_t>(2).value()==old.source_model_flags_02()&&selected==lod&&view.current_detail_level().value()==lod,"descriptor dispatch mismatch");
            const auto resident=view.source_lod_index(lod);for(unsigned i=0;i<view.part_count();++i)check(view.part(resident,i).flags_02().value()==old.source_part_flags_02(resident,i),"queue flag branch mismatch");
        }
    }check(calls==320,"missing model/LOD/flag dispatch combinations");
}
}
int main(int argc,char** argv)try{
    check(argc==3,"expected vehicle draw contract and retail root");const std::string_view name=argv[1];
    if(name=="footprint")footprint(argv[2]);else if(name=="order")order(argv[2]);else if(name=="lifetime")lifetime(argv[2]);else if(name=="dispatch")dispatch(argv[2]);else throw FormatError("unknown vehicle draw contract");
    std::cout<<"canonical_vehicle_draw_contract="<<name<<" errors=0 activation=false\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
