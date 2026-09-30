#include "sgn/vehicle_shadow_draw.hpp"
#include <bit>
#include <iostream>
#include <set>
#include <thread>
namespace {
using namespace sgn;using namespace sgn::retail;
void check(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
template<class Error,class F>void rejects(F action){bool caught=false;try{action();}catch(const Error&){caught=true;}check(caught,"missing typed shadow rejection");}
Bytes snapshot(SourceAllocationBank& bank,SourceAllocationRef allocation){const auto bytes=bank.bytes(allocation);return {bytes.begin(),bytes.end()};}
struct Fixture {
    SourceAllocationBank bank;AssetStore assets;ModelResources models;std::size_t car,shadow;
    explicit Fixture(const std::filesystem::path& root):assets(root),models(assets,root){bank.initialize_42eab0();
        car=*models.resolve(models.load_source(bank,"car1"));shadow=*models.resolve(models.load_source(bank,"car1sh"));}
    SourcePmdModelView model(){return models.source_at(shadow);}
    SourcePmdModelView car_model(){return models.source_at(car);}
};
struct Inputs {
    VehiclePresentationState presentation;VehicleShadowState shadow;ModelDrawQueue regular,background;ViewMatrixArena matrices;ModelTransform pose;
    Inputs(){presentation.field_3928=2;shadow.factor_392c=2;shadow.factor_3930=19;
        shadow.position={1,2,3,0x71234567};shadow.forward={0,0,1,0x72456789};shadow.normal={0,1,0,0x736789ab};
        pose.scale={3,4,5};pose.scale_w=0x7fc12345;pose.tail={17,0,0,0xf1234567};
        reset_model_draw_queues(regular,background);reset_view_matrix_cursor(matrices);}
    bool run(Fixture& f){return append_vehicle_shadow_draw(presentation,shadow,f.models,f.car,f.shadow,background,matrices,pose);}
};
void allow(std::set<unsigned>& values,unsigned at,unsigned count){for(unsigned i=0;i<count;++i)values.insert(at+i);}
void footprint(const std::filesystem::path& root){
    Fixture f(root);Inputs in;const auto model=f.model();model.current_detail_level().set(3);
    f.car_model().descriptor().field<VehicleVector>(0x10).set(std::bit_cast<VehicleVector>(std::array<unsigned,4>{0x7fc99999,0x80000000,0x3f800000,0xabcdef01}));
    const auto car_before=snapshot(f.bank,f.car_model().allocation()),before=snapshot(f.bank,model.allocation());
    const auto template_before=std::bit_cast<std::array<unsigned,20>>(in.pose);
    std::set<unsigned> allowed;allow(allowed,model.first_polygon_attribute_flags().location().offset,1);
    allow(allowed,model.descriptor().location().offset+2,2);allow(allowed,model.descriptor().location().offset+0x10,16);
    for(unsigned i=0;i<model.part_count();++i)allow(allowed,model.part(model.source_lod_index(3),i).transform().location().offset+64,8);
    check(in.run(f),"visible shadow did not append");const auto after=snapshot(f.bank,model.allocation());
    for(unsigned i=0;i<before.size();++i)if(before[i]!=after[i])check(allowed.contains(i),"shadow changed byte outside source footprint");
    check(car_before==snapshot(f.bank,f.car_model().allocation()),"shadow mutated car owner");
    for(unsigned at=0x10;at<0x20;at+=4)check(model.descriptor().field<unsigned>(at).value()==f.car_model().descriptor().field<unsigned>(at).value(),"descriptor DWORD copy changed bits");
    check(template_before==std::bit_cast<std::array<unsigned,20>>(in.pose),"shadow changed live template owner");
    const auto& queued=in.background.records[0].pose;
    check(queued.scale_w==in.pose.scale_w&&queued.tail[3]==in.pose.tail[3]&&queued.position_w==in.shadow.position.opaque&&
        queued.forward_w==in.shadow.forward.opaque&&queued.up_w==in.shadow.normal.opaque,"shadow lost copied/retained W or tail");
    check(in.background.cursor==1u&&in.regular.cursor==0u&&in.background.records[1].model_identity==0&&model.current_detail_level().value()==3u,"wrong queue/LOD owner");
}
void pointers_and_order(const std::filesystem::path& root){
    Fixture f(root);Inputs in;
    //Authored car1sh has one attribute. Use a second real car1 payload for
    //the host pointer-mutation contract; retail shadow cases remain in the corpus.
    f.shadow=*f.models.resolve(f.models.load_source(f.bank,"car1"));
    const auto model=f.model();const auto descriptor=model.descriptor();
    const auto start=328+model.bytes().field<unsigned>(0x24).value();const auto size=model.bytes().field<unsigned>(0xb8).value();
    check(size>=16,"shadow receiving needs two real attribute records");
    model.bytes().field<std::uint8_t>(start+3).set(0x08);model.bytes().field<std::uint8_t>(start+11).set(0x09);
    descriptor.field<unsigned>(0x2c).set(start+8);const auto current=model.first_polygon_attribute_flags();
    check(current.location().offset==start+11,"attribute accessor ignored current descriptor pointer");
    check(in.run(f),"redirected attribute rejected");
    check(model.bytes().field<std::uint8_t>(start+3).value()==0x08&&current.value()==0x71,"shadow used part0 attribute or wrote extra bytes");
    descriptor.field<unsigned>(0x2c).set(start+1);rejects<FormatError>([&]{(void)model.first_polygon_attribute_flags();});
    descriptor.field<unsigned>(0x2c).set(start+size);rejects<FormatError>([&]{(void)model.first_polygon_attribute_flags();});
    descriptor.field<unsigned>(0x2c).set(start);model.first_polygon_attribute_flags().set(0x08);
    descriptor.field<VehicleVector>(0x10).set({21,22,23,24});f.car_model().descriptor().field<VehicleVector>(0x10).set({31,32,33,34});
    const auto old=std::bit_cast<std::array<unsigned,4>>(descriptor.field<VehicleVector>(0x10).value());in.background.cursor=167;
    const auto matrix_cursor=in.matrices.cursor;rejects<FormatError>([&]{(void)in.run(f);});
    check(model.first_polygon_attribute_flags().value()==0x70,"attribute write did not precede queue rejection");
    check(old==std::bit_cast<std::array<unsigned,4>>(descriptor.field<VehicleVector>(0x10).value())&&matrix_cursor==in.matrices.cursor,
        "post-queue descriptor copy ran before queue admission");
    // The admission guard is host-only; the original invalid/full queue is not executed.
}
void lifetime_and_gate(const std::filesystem::path& root){
    Fixture f(root),other(root);Inputs in;const auto view=f.model(),car=f.car_model();const auto attr=view.first_polygon_attribute_flags();
    const auto vector=view.descriptor().field<VehicleVector>(0x10);const auto allocation=view.allocation();
    const auto persistent=f.bank.allocate_42e8d0(8,1);f.bank.bytes(persistent)[0]=97;f.bank.release_42eaa0();
    rejects<SourceAllocationError>([&]{(void)attr.value();});rejects<SourceAllocationError>([&]{vector.set({});});
    rejects<SourceAllocationError>([&]{(void)append_vehicle_shadow_draw(in.presentation,in.shadow,car,view,1,in.background,in.matrices,in.pose);});
    check(in.background.cursor==0u&&in.matrices.count==0u,"expired shadow changed queue/matrix owner");
    check(f.models.source_name(f.shadow)=="car1sh"&&f.bank.bytes(persistent)[0]==97,"release lost metadata/category1 control");
    in.presentation.field_3928=0;
    check(!append_vehicle_shadow_draw(in.presentation,in.shadow,f.models,std::size_t(-1),std::size_t(-1),in.background,in.matrices,in.pose),"hidden shadow accessed invalid handles");
    check(!append_vehicle_shadow_draw(in.presentation,in.shadow,car,view,0,in.background,in.matrices,in.pose),"hidden shadow accessed expired fields");
    in.presentation.field_3928=1;check(in.run(other),"another bank affected by release");
    f.bank.begin_42ea80();const auto id=f.models.load_source(f.bank,"car1sh");const auto replacement=f.models.source_at(*f.models.resolve(id));
    check(replacement.allocation()!=allocation,"new scope reused old generation");rejects<SourceAllocationError>([&]{attr.set(0);});
    bool rejected=false;std::thread foreign([&]{try{(void)in.run(other);}catch(const SourceAllocationError&){rejected=true;}});foreign.join();check(rejected,"foreign shadow thread accepted");
    SourcePmdModelView dead;{Fixture temporary(root);dead=temporary.model();}rejects<SourceAllocationError>([&]{(void)dead.first_polygon_attribute_flags();});
}
void dispatch(const std::filesystem::path& root){
    Fixture f(root);Inputs canonical,legacy;AssetStore assets(root);ModelResources models(assets,root);
    const auto car=*models.resolve(models.load("car1")),shadow=*models.resolve(models.load("car1sh"));
    unsigned selected=0;models.at(shadow).bind_current_detail_level(selected);selected=3;f.model().current_detail_level().set(3);
    check(canonical.run(f)&&append_vehicle_shadow_draw(legacy.presentation,legacy.shadow,models,car,shadow,legacy.background,legacy.matrices,legacy.pose),"dispatch did not append");
    check(std::bit_cast<std::array<unsigned,168*21>>(canonical.background.records)==std::bit_cast<std::array<unsigned,168*21>>(legacy.background.records),"registry queue dispatch mismatch");
    check(canonical.background.cursor==legacy.background.cursor&&canonical.matrices.cursor==legacy.matrices.cursor&&canonical.matrices.count==legacy.matrices.count&&
        std::bit_cast<std::array<unsigned,384*16>>(canonical.matrices.matrices)==std::bit_cast<std::array<unsigned,384*16>>(legacy.matrices.matrices),"registry matrix dispatch mismatch");
    check(selected==3u&&f.model().current_detail_level().value()==3u,"shadow dispatch wrote separate LOD state");
}
}
int main(int argc,char** argv)try{
    check(argc==3,"expected shadow contract and retail root");const std::string_view name=argv[1];
    if(name=="footprint")footprint(argv[2]);else if(name=="pointers_and_order")pointers_and_order(argv[2]);
    else if(name=="lifetime_and_gate")lifetime_and_gate(argv[2]);else if(name=="dispatch")dispatch(argv[2]);else throw FormatError("unknown shadow contract");
    std::cout<<"canonical_shadow_contract="<<name<<" errors=0 activation=false\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
