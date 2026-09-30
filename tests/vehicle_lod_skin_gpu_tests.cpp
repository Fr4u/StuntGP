#include "sgn/race_session.hpp"
#include "sgn/renderer.hpp"
#include "sgn/live_scene_geometry.hpp"
#include "sgn/live_draw_batch.hpp"
#include "sgn/vehicle_model_detail.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <fstream>
#include <iostream>
#include <map>
#include <set>

namespace {
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
struct Window {HWND handle{};~Window(){if(handle)DestroyWindow(handle);}};
// Test fingerprint, not a cryptographic provenance hash. The runner separately
// hashes source inputs, executables and captures with SHA-256.
std::uint64_t geometry_fingerprint(const sgn::LiveGeometry& geometry){
    std::uint64_t hash=14695981039346656037ull;
    const auto word=[&](std::uint32_t value){for(unsigned i=0;i<4;++i){hash^=(value>>(i*8))&255u;hash*=1099511628211ull;}};
    for(const auto& vertex:geometry.model.vertices){
        for(float value:{vertex.position.x,vertex.position.y,vertex.position.z,vertex.normal.x,vertex.normal.y,vertex.normal.z,vertex.u,vertex.v})
            word(std::bit_cast<std::uint32_t>(value));
        word(vertex.color);
    }
    for(const auto& surface:geometry.model.surfaces){word(surface.first);word(surface.count);word(surface.texture);word(surface.part);word(surface.translucent);word(surface.two_sided);}
    for(const auto& name:geometry.model.textures){for(unsigned char c:name){hash^=c;hash*=1099511628211ull;}word(0);}
    for(const auto& material:geometry.materials)word(static_cast<unsigned>(material.blend));
    return hash;
}
}

int main(int argc,char** argv){try{
    using namespace sgn;using namespace sgn::retail;
    require(argc==4,"expected retail root, new output directory, hardware|warp");
    const bool warp=std::string_view(argv[3])=="warp";
    require(warp||std::string_view(argv[3])=="hardware","invalid adapter");
    const auto output=std::filesystem::absolute(argv[2]);
    require(!std::filesystem::exists(output),"preserve previous evidence: choose a new output directory");
    std::filesystem::create_directories(output);
    RaceSessionLaunch launch;launch.mode=SessionMode::normal_race;launch.master_gain=0;
    RaceSession session(argv[1],launch);auto& state=session.state();
    const auto initial_random=state.random.state,initial_tick=state.simulation_tick;
    std::array<ModelResource*,6> cars{};std::vector<Model> meshes;
    for(unsigned slot=0;slot<cars.size();++slot){
        require(state.vehicles.models[slot].model.has_value(),"normal Session did not load six actual cars");
        cars[slot]=&session.models().at(*state.vehicles.models[slot].model);
        require(canonical(std::string(cars[slot]->source_name()))=="car"+std::to_string(slot+1),"unexpected Session car roster");
        require(cars[slot]->lod_count()==3,"expected three authored LODs");
        for(unsigned lod=0;lod<3;++lod)meshes.push_back(model_live_geometry(*cars[slot],lod).model);
    }
    WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"SGN-Lod-Skin-Probe";
    require(RegisterClassW(&wc)!=0,"RegisterClass failed");
    Window window{CreateWindowExW(0,wc.lpszClassName,L"SGN actual vehicle LOD skins",WS_OVERLAPPEDWINDOW,
        0,0,960,720,nullptr,nullptr,wc.hInstance,nullptr)};
    require(window.handle!=nullptr,"CreateWindow failed");
    Renderer renderer(window.handle,{true,warp,false,false,true});renderer.resize(960,720);renderer.load_scene(meshes,session.assets());
    const auto texture_bytes=renderer.stats().uploaded_texture_bytes;
    // Fixed capture camera and model transform keep size, pose and framing
    // identical across LODs. The separate selector camera below is a declared
    // argument fixture for original 413E10/42B580, never a production override.
    CameraState camera;camera.position={350,180,650,0};camera.target={0,0,0,0};camera.up={0,1,0,0};
    camera.lens=400;camera.near_clip=32;camera.far_factor=10;camera.half_width=480;camera.half_height=360;
    camera_view_basis(camera);camera_projection(camera);
    View view;view.world_to_view=camera_world_view(camera);
    view.retail_viewport=View::RetailViewport{{camera.projection[0],camera.projection[5],camera.projection[10],camera.projection[14],32},40960,true};
    CameraState selector;selector.lens=3.2f;const CameraVector forward{0,0,1,0};
    constexpr std::array<unsigned,5> sequence{0,1,2,1,0};constexpr std::array<float,3> distances{1000,4000,7500};
    std::array<std::array<std::uint64_t,3>,6> geometry_hashes{};
    std::array<std::array<Bytes,3>,6> images;
    std::array<std::string,6> body_textures;
    std::array<std::array<unsigned,3>,6> body_triangles{};
    std::ofstream rows(output/"lod-materials.tsv");require(bool(rows),"cannot write LOD evidence");
    rows<<"step\tslot\tmodel\tsource_lod\tphysical_lod\tselector_distance\tbody_triangles\tvertices\tgeometry_fnv1a64\tlivery\ttyre\tbody_texture\n";
    unsigned frames{},returns{};
    for(unsigned step=0;step<sequence.size();++step){
        for(unsigned slot=0;slot<cars.size();++slot){
            auto& model=*cars[slot];auto vehicle=state.vehicles.physics[slot];auto presentation=state.vehicles.presentation[slot];
            auto particles=state.vehicles.particles[slot];const auto wanted=sequence[step];
            vehicle.position={0,0,distances[wanted],0};vehicle.previous_position=vehicle.position;
            auto& selected=state.selected_lod[slot];
            prepare_vehicle_model_detail(vehicle,presentation,particles,
                vehicle_presentation_effects(state.vehicles.model_effects[slot],state.vehicles.anchors[slot]),
                model,selected,selector,forward,state.impact_particles.rate_selector);
            require(selected==wanted,"source distance fixture selected unexpected LOD");
            const auto lod=model.source_lod_index(selected);require(lod==wanted,"source descriptor points to unexpected physical LOD");
            auto geometry=model_live_geometry(model,lod);std::set<std::string> grids;unsigned triangles{};
            for(const auto& surface:geometry.model.surfaces)if(surface.part==0){
                triangles+=surface.count/3;const auto& texture=geometry.model.textures.at(surface.texture);
                if(texture.find("grid")!=std::string::npos)grids.insert(texture);
            }
            require(grids.size()==1,"body has no single resolved livery texture");
            const auto& grid=*grids.begin();
            if(step==0)body_textures[slot]=grid;else require(body_textures[slot]==grid,"source-selected LOD changed the body skin");
            body_triangles[slot][lod]=triangles;
            coalesce_live_draws(geometry);const auto fingerprint=geometry_fingerprint(geometry);
            if(step<3)geometry_hashes[slot][lod]=fingerprint;
            else require(geometry_hashes[slot][lod]==fingerprint,"return to previous LOD changed geometry or texture selection");
            const unsigned mesh=slot*3+lod;renderer.stage_mesh(mesh,geometry);
            CameraModelTransform transform;const auto& pivot=model.source_model_vector_10();transform.pivot={pivot.x,pivot.y,pivot.z,pivot.w};
            const auto views=model_live_views(model,lod,camera,transform);
            const SceneInstance instance{mesh,{},views.parts};const LiveInstanceView live{views.model_to_view};
            renderer.draw_scene(view,std::span(&instance,1),"","",96,std::span(&live,1));
            const auto file=output/("car"+std::to_string(slot+1)+"-step"+std::to_string(step)+"-lod"+std::to_string(lod)+".bmp");
            renderer.capture(file);auto pixels=read_file(file);
            if(step<3)images[slot][lod]=std::move(pixels);
            else{require(images[slot][lod]==pixels,"GPU image became stale or changed after returning to previous LOD");++returns;}
            rows<<step<<'\t'<<slot<<'\t'<<model.source_name()<<'\t'<<selected<<'\t'<<lod<<'\t'<<distances[wanted]<<'\t'<<triangles<<'\t'
                <<geometry.model.vertices.size()<<'\t'<<fingerprint<<'\t'<<state.cars[slot].settings.livery<<'\t'<<state.cars[slot].settings.tyre<<'\t'<<grid<<'\n';
            rows.flush();++frames;
        }
    }
    for(unsigned slot=0;slot<cars.size();++slot){
        require(body_triangles[slot][0]>body_triangles[slot][1]&&body_triangles[slot][1]>body_triangles[slot][2],"distance did not reduce actual body geometry");
        require(images[slot][0]!=images[slot][1]&&images[slot][1]!=images[slot][2],"different LODs produced identical frames; possible missing draw or stale mesh");
    }
    require(state.random.state==initial_random&&state.simulation_tick==initial_tick,"presentation test advanced gameplay RNG or clock");
    require(renderer.stats().uploaded_texture_bytes==texture_bytes,"LOD switching reloaded texture resources");
    renderer.finish();renderer.collect_diagnostics();renderer.check_health();
    require(renderer.stats().debug_active&&renderer.stats().debug_errors==0,"GPU debug diagnostics");
    std::cout<<"{\"passed\":true,\"adapter\":\""<<(warp?"warp":"hardware")<<"\",\"actual_session\":true,\"cars\":6,\"unique_lods\":18,\"source_lod_selections\":"<<frames
        <<",\"exact_geometry_and_image_returns\":"<<returns<<",\"startup_random_state\":"<<initial_random
        <<",\"simulation_ticks\":"<<initial_tick<<",\"texture_bytes\":"<<texture_bytes<<",\"debug_errors\":"<<renderer.stats().debug_errors<<"}\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
