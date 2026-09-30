#include "source_pmd_geometry_fixture.hpp"
#include "sgn/scene_resources.hpp"
#include "sgn/source_pmd_track_normals.hpp"
#include "sgn/world_model_draw.hpp"
#include <iostream>
#include <set>
#include <thread>

namespace {
using namespace geometry_receiving;
void bindings(const std::filesystem::path& root){
    SourceAllocationBank bank;bank.initialize_42eab0();AssetStore assets(root);SceneResources scene(assets,root);
    auto strip=std::make_unique<VehicleStripGeometry>();scene.bind(*strip);
    const std::array<LevelAssetRequest,6> requests{{{"sky1",2},{"track1",3},{"car1",4},{"car1sh",5},{"car1",6},{"car1sh",7}}};
    std::array<SelectionRecord,6> selection{};selection[1].kind=selection[4].kind=2;
    std::array<LevelCarSlot,6> cars{};cars[0].source_word00=777;
    const auto handles=scene.bind_source_models(bank,requests,selection,cars);
    check(handles.sky==scene.model_handle(2)&&handles.track==scene.model_handle(3),"source SceneResources lost static bindings");
    check(cars[0].source_word00==777&&scene.model_handle(4)==scene.models().resolve(cars[1].source_word00)&&
        scene.model_handle(6)==scene.models().resolve(cars[4].source_word00),"source binding did not retain physical selection holes");
    check(scene.models().size()==6&&bank.live_count()==6&&&scene.strip_geometry()==strip.get(),"duplicate or displaced model/strip owner");
    const auto car=scene.models().source_at(*scene.model_handle(4)),other=scene.models().source_at(*scene.model_handle(6));
    check(car.allocation()!=other.allocation(),"equal-name requests collapsed source allocation identity");
    cars[0].source_word00=0;cars[1].settings.livery=2;cars[4].settings.livery=7;
    cars[1].settings.tyre=cars[4].settings.tyre=1;const std::array<std::int8_t,6> random{};
    remap_level_vehicle_textures(scene.models(),cars,random);
    for(unsigned slot=0;slot<4;++slot){
        const auto mesh=model_live_geometry(scene.models(),*scene.model_handle(4),slot,"");
        for(const auto& surface:mesh.model.surfaces)if(surface.part==0)
            check(mesh.model.textures[surface.texture].ends_with("grid2"),"selected canonical livery changed across LOD");
        check(other.texture_name(0).ends_with("grid7")&&scene.models().source_lod_index(*scene.model_handle(4),slot)==car.source_lod_index(slot),"other instance/LOD identity lost");
    }
    format_rejects([&]{(void)scene.models().at(*scene.model_handle(4));});
    format_rejects([&]{(void)scene.models().load("car2");});
    SourceAllocationBank foreign;foreign.initialize_42eab0();
    format_rejects([&]{scene.bind_source_models(foreign,requests,selection,cars);});
    auto normals=TrackNormalAllocation{0,177};const auto track=scene.models().source_at(*handles.track);
    const auto before=snapshot(bank,track.allocation());const auto view=bind_source_track_normal_allocation(track.track(),normals);
    check(normals.cursor==177u&&before==snapshot(bank,track.allocation()),"normal bind generated data or reset576CC8");
    check(view.normals().location()==track.normals().location()&&normals.first_generated==track.bytes().field<std::uint32_t>(0xe4).value(),"normal base/generated pointer wrong");
    if(view.normals().size()>normals.first_generated){const auto field=view.normals().at(normals.first_generated);auto normal=field.value();normal.x=3;field.set(normal);
        check(track.track().generated_normals().at(0).value().x==3,"generated normals are a copied array");}
    WorldModelDrawState draws;draws.template_pose=world_model_template();ViewMatrixArena arena;reset_view_matrix_cursor(arena);
    SourceSpritePacketStorage sprites;seed_world_model_draws(draws,scene.models(),handles.track,handles.sky,sprites,0,1,arena);
    check(draws.background.cursor==3u&&draws.background.records[0].model_identity==scene.models().identity(handles.track)&&
        draws.background.records[1].model_identity==scene.models().identity(handles.sky)&&draws.background.records[2].model_identity==sprite_model_identity&&
        draws.background.records[3].model_identity==0,"actual seed consumer rejected canonical registry");
    bank.release_scope_42ea00();allocation_rejects([&]{(void)view.normals();});allocation_rejects([&]{(void)scene.models().source_pivot(*handles.track);});
}
void publication(const std::filesystem::path& root){
    Fixture f(root);const auto id=f.load();const auto handle=*f.resources.resolve(id);const auto model=f.model(id);
    CameraState camera;camera.position={1,2,3,0};camera.world_to_camera_rotation=vehicle_identity();CameraModelTransform outer;
    ViewMatrixArena arena;reset_view_matrix_cursor(arena);CameraMatrix shared=vehicle_identity();
    const auto before=snapshot(f.bank,model.allocation());allocate_model_part_matrices(model,0,arena);
    const auto after=snapshot(f.bank,model.allocation());std::set<std::uint32_t> writes;
    for(unsigned i=0;i<model.part_count();++i){const auto offset=model.part(0,i).transform().location().offset;
        for(unsigned b=0;b<8;++b)writes.insert(offset+64+b);}
    for(std::uint32_t i=0;i<before.size();++i)if(before[i]!=after[i])check(writes.contains(i),"matrix publication wrote beyond canonical tail words");
    const auto captured=model_live_views(f.resources,handle,0,camera,outer,arena,shared);
    const auto stable=snapshot(f.bank,model.allocation());const auto count=arena.count;const auto cursor=arena.cursor;
    const auto p=model.part(0,0);auto transform=p.transform().value();transform.forward.x+=57;p.transform().set(transform);
    const auto unchanged=model_live_views(f.resources,handle,0,camera,outer,arena,shared);
    check(captured.model_to_view==unchanged.model_to_view,"published consumer regenerated forward/up rotation");
    const auto token=transform.tail[0];auto& matrix=token==1?shared:arena.matrices[(token-2)/64];matrix[0]+=0.5f;
    const auto changed=model_live_views(f.resources,handle,0,camera,outer,arena,shared);
    check(changed.model_to_view!=captured.model_to_view&&count==arena.count&&cursor==arena.cursor,"published consumer ignored arena owner or allocated matrices");
    check(stable.size()==snapshot(f.bank,model.allocation()).size(),"capture changed canonical extent");
    bool rejected=false;std::thread foreign([&]{try{(void)model_live_views(f.resources,handle,0,camera,outer,arena,shared);}catch(const SourceAllocationError&){rejected=true;}});foreign.join();check(rejected,"foreign published view thread accepted");
    (void)f.bank.release_42e980(model.allocation());allocation_rejects([&]{(void)model_live_views(f.resources,handle,0,camera,outer,arena,shared);});
}
}
int main(int argc,char** argv)try{
    check(argc==3,"expected bindings|publication and retail root");
    if(std::string_view(argv[1])=="bindings")bindings(argv[2]);else if(std::string_view(argv[1])=="publication")publication(argv[2]);else throw std::runtime_error("unknown receiving case");
    std::cout<<"canonical_session_consumer_contract="<<argv[1]<<" errors=0 activation=false\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
