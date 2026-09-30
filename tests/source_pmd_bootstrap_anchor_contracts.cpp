#include "retail_vehicle_anchors_state.hpp"
#include "sgn/retail_vehicle_bootstrap.hpp"
#include "sgn/model_anchor_adapter.hpp"
#include "sgn/source_pmd_geometry.hpp"
#include "sgn/retail_model_matrices.hpp"
#include <bit>
#include <iostream>
#include <memory>
#include <thread>
namespace {
using namespace sgn;using namespace sgn::retail;
void check(bool condition,const char* text){if(!condition)throw std::runtime_error(text);}
template<class Error,class F>void rejects(F fn){bool caught=false;try{fn();}catch(const Error&){caught=true;}check(caught,"missing typed anchor rejection");}
Bytes snapshot(SourceAllocationBank& bank,SourceAllocationRef allocation){const auto b=bank.bytes(allocation);return {b.begin(),b.end()};}
struct Fixture {
    SourceAllocationBank bank;AssetStore assets;ModelResources models;std::size_t handle;
    explicit Fixture(const std::filesystem::path& root,std::string name="car1"):assets(root),models(assets,root){bank.initialize_42eab0();handle=*models.resolve(models.load_source(bank,name));}
    SourcePmdModelView view(){return models.source_at(handle);}
};
void same_vector(const VehicleVector& a,const VehicleVector& b){check(std::bit_cast<std::array<unsigned,4>>(a)==std::bit_cast<std::array<unsigned,4>>(b),"anchor vector bits differ");}
void current_alias(const std::filesystem::path& root){
    Fixture f(root);const auto view=f.view();ModelAnchorAdapter current(view);const auto descriptor=view.descriptor();
    for(unsigned slot=0;slot<4;++slot){view.current_detail_level().set(slot);check(current.selected_lod()==slot,"adapter cached descriptor5C");
        const auto expected=view.part(view.source_lod_index(slot),11).transform().value();same_vector(current.part(11,current.selected_lod()).position,expected.position);}
    const auto lane1=descriptor.field<unsigned>(0x4c).value();descriptor.field<unsigned>(0x48).set(lane1);
    auto transformed=view.part(view.source_lod_index(0),23).transform();auto value=transformed.value();value.position={123,234,345,std::bit_cast<float>(0x7fc12345u)};transformed.set(value);
    //Literal slot0 for paired trails follows the current48 pointer too.
    same_vector(current.part(23,0).position,value.position);
    descriptor.field<unsigned>(0x54).set(lane1);view.current_detail_level().set(3);same_vector(current.part(23,current.selected_lod()).position,value.position);
    value.position.x=567;transformed.set(value);check(current.part(23,3).position.x==567,"adapter copied transform state");
    view.current_detail_level().set(4);rejects<FormatError>([&]{(void)current.part(11,current.selected_lod());});
}
void ordering(const std::filesystem::path& root){
    Fixture f(root);auto storage=std::make_unique<VehicleStorage>();unsigned current=5;
    for(unsigned i=0;i<6;++i){storage->race[i].active=i==2?1:(i==1?2:0);storage->models[i].model=i==2?std::optional(f.handle):std::optional(std::size_t(-1));storage->trail_inputs[i].pair_b_available={9,9};}
    initialize_vehicle_model_anchors(*storage,f.models,current);check(current==0,"anchor chain did not reset physical current");
    for(unsigned i=0;i<6;++i)if(i!=2)check(storage->trail_inputs[i].pair_b_available==std::array<unsigned,2>{0,0},"pairB did not clear inactive slots before active gate");
    //Existing source count tests return before selected5C for a one-part shadow.
    Fixture shadow(root,"car1sh");shadow.view().current_detail_level().set(0xffffffff);storage->models[2].model=shadow.handle;
    storage->anchors[2].effect_available={9,9};initialize_vehicle_model_anchors(*storage,shadow.models,current);
    check(storage->presentation[2].field_3134==0&&storage->anchors[2].effect_available==std::array<unsigned,2>{0,0},"early part-count gate read invalid selectedLOD");
    storage->race[2].active=0;initialize_vehicle_model_anchors(*storage,shadow.models,current);
}
void lifetime(const std::filesystem::path& root){
    Fixture f(root),other(root);const auto view=f.view();ModelAnchorAdapter current(view),other_current(other.view());
    f.bank.release_42eaa0();rejects<SourceAllocationError>([&]{(void)current.selected_lod();});rejects<SourceAllocationError>([&]{(void)current.part(11,0);});
    check(other_current.part_count()==other.view().part_count(),"release crossed bank owners");
    f.bank.begin_42ea80();(void)f.models.load_source(f.bank,"car1");rejects<SourceAllocationError>([&]{(void)current.part_count();});
    bool rejected=false;std::thread worker([&]{try{(void)other_current.maximum_vertex_z(11,0);}catch(const SourceAllocationError&){rejected=true;}});worker.join();check(rejected,"foreign thread anchor read accepted");
    std::optional<ModelAnchorAdapter> dead;{Fixture temporary(root);dead.emplace(temporary.view());}rejects<SourceAllocationError>([&]{(void)dead->selected_lod();});
}
using Image=std::array<std::uint8_t,0x7800>;
Image image(VehicleStorage& storage,unsigned slot){Image b{};anchor_oracle::transfer(b,storage.anchors[slot],storage.presentation[slot],storage.trail_inputs[slot],storage.race[slot],false);return b;}
void six_slot_chain(const std::filesystem::path& root){
    SourceAllocationBank bank;bank.initialize_42eab0();AssetStore assets(root);ModelResources models(assets,root),legacy(assets,root);
    auto a=std::make_unique<VehicleStorage>(),b=std::make_unique<VehicleStorage>();unsigned ac=5,bc=3;
    std::vector<std::pair<SourceAllocationRef,Bytes>> before;
    for(unsigned i=0;i<6;++i){const auto name="car"+std::to_string(i+1);const auto handle=*models.resolve(models.load_source(bank,name));const auto old=*legacy.resolve(legacy.load(name));
        a->models[i].model=handle;b->models[i].model=old;a->race[i].active=b->race[i].active=i==4?2:1;
        const auto view=models.source_at(handle);view.current_detail_level().set(i%4);legacy.at(old).current_detail_level()=i%4;
        before.emplace_back(view.allocation(),snapshot(bank,view.allocation()));}
    initialize_vehicle_model_anchors(*a,models,ac);initialize_vehicle_model_anchors(*b,legacy,bc);
    check(ac==0&&bc==0,"chain current owner mismatch");for(unsigned i=0;i<6;++i)check(image(*a,i)==image(*b,i),"six-slot chain differs from unchanged legacy arithmetic");
    for(const auto& [allocation,bytes]:before)check(bytes==snapshot(bank,allocation),"anchor chain mutated canonicalPMD");
}
void views(const std::filesystem::path& root){
    Fixture f(root);AssetStore assets(root);ModelResources legacy(assets,root);const auto handle=*legacy.resolve(legacy.load("car1"));auto& old=legacy.at(handle);
    const auto view=f.view();CameraState camera;camera.position={7,11,13,0};camera.world_to_camera_reference=0;CameraMatrix identity;initialize_model_matrix_identity(identity);
    camera.world_to_camera_rotation=identity;CameraModelTransform outer;outer.position={17,19,23,0};outer.scale={2,3,4,0};
    for(unsigned slot=0;slot<4;++slot){view.current_detail_level().set(slot);old.current_detail_level()=slot;ViewMatrixArena a,b;reset_view_matrix_cursor(a);reset_view_matrix_cursor(b);
        allocate_model_part_matrices(view,slot,a);allocate_model_part_matrices(old,slot,b);
        const auto actual=model_live_views(f.models,f.handle,f.models.current_detail_level(f.handle),camera,outer,a,identity);
        const auto expected=model_live_views(legacy,handle,legacy.current_detail_level(handle),camera,outer,b,identity);
        check(actual.parts.size()==expected.parts.size()&&actual.model_to_view.size()==expected.model_to_view.size(),"currentLOD view shape mismatch");
        for(unsigned i=0;i<actual.parts.size();++i){check(actual.parts[i].visible==expected.parts[i].visible,"currentLOD visibility mismatch");
            check(std::bit_cast<std::array<unsigned,16>>(actual.model_to_view[i])==std::bit_cast<std::array<unsigned,16>>(expected.model_to_view[i]),"currentLOD published matrix mismatch");}
    }
}
}
int main(int argc,char** argv)try{
    check(argc==3,"expected anchor contract and retail root");const std::string_view mode=argv[1];
    if(mode=="current_alias")current_alias(argv[2]);else if(mode=="ordering")ordering(argv[2]);else if(mode=="lifetime")lifetime(argv[2]);else if(mode=="six_slot_chain")six_slot_chain(argv[2]);else if(mode=="views")views(argv[2]);else throw std::runtime_error("unknown anchor contract");
    std::cout<<"canonical_bootstrap_anchor_contract="<<mode<<" errors=0 activation=false\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
