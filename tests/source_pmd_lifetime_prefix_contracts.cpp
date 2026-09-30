#include "source_pmd_brake_lights_fixture.hpp"
#include "sgn/vehicle_model_detail.hpp"
#include <cstring>
#include <limits>
#ifndef SGN_LATE_REGISTRY_PREFIX
#define SGN_LATE_REGISTRY_PREFIX 1
#endif
using namespace sgn;using namespace sgn::brake_test;
namespace {
struct Data {VehicleState vehicle{};VehiclePresentationState presentation{};VehicleTrailInputs trails{};VehicleModelEffects effects{};};
static_assert(std::is_trivially_copyable_v<Data>);
struct Failure {std::string type,message;unsigned site{};};
template<class F>Failure failure(F fn){try{fn();}catch(const SourceAllocationError& e){return {"SourceAllocationError",e.what(),e.source_site};}
    catch(const FormatError& e){return {"FormatError",e.what(),0};}catch(const std::out_of_range& e){return {"out_of_range",e.what(),0};}
    throw std::runtime_error("expected lifetime failure did not occur");}
Bytes bits(const Data& d){const auto* p=reinterpret_cast<const std::uint8_t*>(&d);return {p,p+sizeof(d)};}
void patch(Bytes& b,std::size_t at,unsigned word){std::memcpy(b.data()+at,&word,4);}
enum class Owner {presentation,brake};
void observe(Owner owner,bool registry,ModelResources& models,std::size_t handle,SourcePmdModelView model,
    std::int32_t initial,unsigned gate,std::int32_t after,bool expect_prefix,const char* type,unsigned site,const char* message){
    auto d=std::make_unique<Data>();d->vehicle.brake_signal=initial;d->trails.field_28a0=gate;d->effects.cached_brake=0x76543210u;
    d->presentation={0x12345678u,0x23456789u,0x3456789au,0x456789abu,0x56789abcu};
    auto expected=bits(*d);if(expect_prefix){if(owner==Owner::presentation)patch(expected,offsetof(Data,presentation)+offsetof(VehiclePresentationState,field_3134),1);
        else patch(expected,offsetof(Data,vehicle)+offsetof(VehicleState,brake_signal),std::bit_cast<unsigned>(after));}
    const auto result=failure([&]{if(owner==Owner::presentation){if(registry)prepare_vehicle_presentation(d->presentation,{},models,handle);else prepare_vehicle_presentation(d->presentation,{},model);}
        else{if(registry)update_vehicle_brake_lights(d->vehicle,d->trails,d->effects,models,handle);else update_vehicle_brake_lights(d->vehicle,d->trails,d->effects,model);}});
    check(result.type==type&&result.site==site,"lifetime exception type/site changed");if(message)check(result.message==message,"lifetime exception diagnostic changed");
    std::cout<<"owner="<<(owner==Owner::presentation?"presentation":"brake")<<" route="<<(registry?"registry":"direct")<<" initial="<<initial<<" gate="<<gate<<" scalar="
        <<(owner==Owner::presentation?static_cast<std::int64_t>(d->presentation.field_3134):d->vehicle.brake_signal)<<" cache="<<d->effects.cached_brake
        <<" host_type="<<result.type<<" host_site="<<std::hex<<result.site<<std::dec<<'\n';
    check(bits(*d)==expected,"retained lifetime prefix differs from expected complete state");
}
constexpr const char* released="allocation borrow is null, foreign, expired or already released";
constexpr const char* wrong_thread="source allocation bank belongs to its creating thread";
constexpr const char* metadata="expired PMD model metadata";
void matrix(ModelResources& models,std::size_t handle,SourcePmdModelView model,bool late,const char* message,unsigned site=0,bool direct_only=false){
    for(bool registry:{false,true}){if(direct_only&&registry)continue;
        const bool prefix=!registry||late;observe(Owner::presentation,registry,models,handle,model,152,0,120,prefix,"SourceAllocationError",site,message);
        //Expected values follow literal wrapping SUB32, signed clamp and exact-one gate.
        const std::array<std::array<std::int32_t,3>,9> cases{{{152,0,120},{32,0,0},{31,0,0},{0,1,120},{-1,1,120},{std::numeric_limits<std::int32_t>::min(),0,2147483616},{-2147483647,1,2147483617},{121,2,89},{0,2,0}}};
        for(auto c:cases)observe(Owner::brake,registry,models,handle,model,c[0],static_cast<unsigned>(c[1]),c[2],prefix,"SourceAllocationError",site,message);
    }
}
void run(std::string_view mode,const std::filesystem::path& root,bool force_late){
    Fixture f(root);const auto m=f.view();const auto ref=m.allocation();const bool late=force_late||SGN_LATE_REGISTRY_PREFIX!=0;
    if(mode=="released"||mode=="scope_released"||mode=="generation"){
        if(mode=="scope_released")f.bank.release_42eaa0();else f.bank.release_42e980(ref);
        std::optional<std::size_t> new_handle;Bytes new_before;SourceAllocationRef new_ref;
        if(mode=="generation"){f.bank.begin_42ea80();new_handle=*f.models.resolve(f.models.load_source(f.bank,"car1"));new_ref=f.models.source_at(*new_handle).allocation();check(new_ref!=ref,"allocation generation revived old identity");new_before=snapshot(f.bank,new_ref);}
        matrix(f.models,f.handle,m,late,released);
        if(new_handle)check(new_before==snapshot(f.bank,new_ref),"expired producer changed unrelated new allocation");return;
    }
    if(mode=="wrong_thread"){const auto before=snapshot(f.bank,ref);std::exception_ptr error;std::thread t([&]{try{matrix(f.models,f.handle,m,late,wrong_thread);}catch(...){error=std::current_exception();}});t.join();if(error)std::rethrow_exception(error);
        check(before==snapshot(f.bank,ref),"wrong-thread producer touched PMD bytes");return;}
    if(mode=="metadata_expired"){SourcePmdModelView expired;SourceAllocationRef live_ref;{
        ModelResources scoped(f.assets,root);const auto h=*scoped.resolve(scoped.load_source(f.bank,"car1"));expired=scoped.source_at(h);live_ref=expired.allocation();}
        const auto before=snapshot(f.bank,live_ref);matrix(f.models,f.handle,expired,true,metadata,0x42efb0,true);
        check(before==snapshot(f.bank,live_ref),"expired metadata touched still-live PMD");return;}
    if(mode=="default_view"){matrix(f.models,f.handle,SourcePmdModelView{},true,metadata,0x42efb0,true);return;}
    if(mode=="invalid_handle"){for(Owner owner:{Owner::presentation,Owner::brake})observe(owner,true,f.models,std::numeric_limits<std::size_t>::max(),m,152,0,120,false,"out_of_range",0,nullptr);return;}
    if(mode=="public_guard"){f.bank.release_42e980(ref);const auto error=failure([&]{(void)f.models.source_at(f.handle);});check(error.type=="SourceAllocationError"&&error.site==0&&error.message==released,"public source_at no longer validates immediately");
        const auto direct=failure([&]{m.check();});check(direct.type==error.type&&direct.site==error.site&&direct.message==error.message,"direct view check weakened");return;}
    if(mode=="invalid_descriptor"||mode=="invalid_lod"){
        const char* text;if(mode=="invalid_descriptor"){m.bytes().field<unsigned>(0x4c).set(0xffffffffu);text="PMD normalized source offset overflow";}
        else{m.current_detail_level().set(4);text="PMD source LOD slot";}
        const auto before=snapshot(f.bank,ref);for(bool registry:{false,true})for(Owner owner:{Owner::presentation,Owner::brake})observe(owner,registry,f.models,f.handle,m,152,0,120,true,"FormatError",0,text);
        check(before==snapshot(f.bank,ref),"first-descriptor failure wrote PMD");return;}
    if(mode=="presentation_equivalence"||mode=="brake_equivalence"){f.bank.release_42e980(ref);const auto owner=mode=="presentation_equivalence"?Owner::presentation:Owner::brake;
        observe(owner,true,f.models,f.handle,m,152,0,120,true,"SourceAllocationError",0,released);return;}
    throw std::runtime_error("unknown lifetime prefix contract");
}
}
int main(int argc,char** argv)try{check(argc==3,"expected lifetime prefix contract and retail root");run(argv[1],argv[2],false);std::cout<<"PASS lifetime prefix "<<argv[1]<<" late_registry="<<SGN_LATE_REGISTRY_PREFIX<<'\n';return 0;}
catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
