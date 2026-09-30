#include "sgn/scene_resources.hpp"
#include <bit>
#include <iostream>
#include <memory>

namespace {void require(bool b,const char* message){if(!b)throw std::runtime_error(message);}}
int main(int argc,char** argv){try{
    using namespace sgn;using namespace sgn::retail;
    if(argc!=2)throw std::runtime_error("expected retail data root");AssetStore assets(argv[1]);SceneResources scene(assets,argv[1]);
    auto geometry=std::make_unique<VehicleStripGeometry>();VehicleStripHeaders headers;for(auto& h:headers.items)h={7,8,9};
    geometry->texture_id=19;geometry->normals[0].w=std::bit_cast<float>(0x7fc01234u);
    initialize_vehicle_strip(*geometry,headers,scene);
    const auto& live=scene.strip_geometry();require(&live==geometry.get(),"generated geometry was copied at publication");
    require(live.texture_id==19&&std::bit_cast<unsigned>(live.normals[127].w)==0x7fc01234u,"source persistent texture/normal W lost");
    require(live.polygons[127].first_corner==508&&live.corner_indices[508]==510&&live.corners[511].vertex==511,"publisher retained the pre-expansion snapshot");
    require(live.vertex_count==0&&live.polygon_count==0&&live.corner_count==0&&live.corner_indices[0]==0&&live.corner_indices[1]==0,"publisher invented drawable geometry");
    geometry->vertices[511].x=123;require(live.vertices[511].x==123,"live generated vertex updates lost");
    for(const auto& h:headers.items)require(h.field_00==0&&h.field_04==0&&h.field_08==0,"real strip header reset was skipped");
    std::array<LevelCarSlot,6> cars{};const std::array<LevelAssetRequest,3> requests{{{"track1",3},{"car16",4},{"car1",5}}};
    std::array<SelectionRecord,6> selection{};selection[0].kind=2;scene.bind_models(requests,selection,cars);
    require(scene.models().size()==3,"scene duplicated resource allocations while publishing bindings");
    require(scene.model_handle(3).has_value()&&scene.model_handle(4)==scene.models().resolve(cars[0].source_word00)&&scene.model_handle(5)==scene.models().resolve(cars[0].source_word04),"scene model identities disconnected");
    require(&scene.strip_geometry()==geometry.get(),"loading PMD resources invalidated generated binding");
    const auto old_track=scene.model_handle(3);const std::array<LevelAssetRequest,1> more{{{"car1",4}}};scene.bind_models(more,selection,cars);
    require(scene.model_handle(3)==old_track,"unrequested persistent scene slot changed");
    auto next=std::make_unique<VehicleStripGeometry>();next->texture_id=25;initialize_vehicle_strip(*next,headers,scene);
    require(&scene.strip_geometry()==next.get()&&scene.strip_geometry().texture_id==25&&geometry->texture_id==19,"world geometry rebind modified old owner");
    std::cout<<"live_strip_bindings=2 pm_models=3 active_counts_preserved=3 geometry_snapshots=0 errors=0\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
