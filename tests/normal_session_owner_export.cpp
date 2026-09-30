#include "normal_session_model_export.hpp"
#include "normal_session_native_observer.hpp"
#include "normal_session_frame_export.hpp"
#include "normal_session_frame_observer.hpp"
#include "normal_session_camera_export.hpp"
#include "normal_session_camera_observer.hpp"
#include "normal_session_race_export.hpp"
#include "normal_session_race_observer.hpp"
#include "normal_session_vehicle_visual_export.hpp"
#include "normal_session_vehicle_visual_observer.hpp"
#include "normal_session_vehicle_audio_export.hpp"
#include "normal_session_vehicle_audio_observer.hpp"
#include "normal_session_vehicle_physics_export.hpp"
#include "normal_session_vehicle_physics_observer.hpp"
#include "normal_session_vehicle_parameters_export.hpp"
#include "normal_session_vehicle_parameters_observer.hpp"
#include "normal_session_ui_export.hpp"
#include "normal_session_ui_observer.hpp"
#include "sgn/session_world_materials.hpp"
#include "sgn/material_d3d12.hpp"
#include "sgn/retail_material_filesystem.hpp"
#include <iostream>

namespace no=normal_session_owner;
using namespace sgn;
using namespace sgn::retail;
namespace {
struct Window {HWND value{};~Window(){if(value)DestroyWindow(value);}};
bool safe_name(std::string_view name){return !name.empty()&&std::all_of(name.begin(),name.end(),[](char c){return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-';});}
}
int main(int argc,char** argv)try{
    no::require(argc==3||(argc==4&&(std::string_view(argv[3])=="--world-pools"||std::string_view(argv[3])=="--scene-owners")),"owner export expects retail root, fresh output directory and optional --world-pools or --scene-owners");
    const bool world_pools=argc==4;
    const bool scene_owners=argc==4&&std::string_view(argv[3])=="--scene-owners";
    const unsigned wire_version=scene_owners?3u:world_pools?2u:1u;
    const auto output=std::filesystem::absolute(argv[2]);no::require(!std::filesystem::exists(output),"owner export output exists");std::filesystem::create_directories(output);
    WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"SGN-Real-Session-Model-Owners";
    no::require(RegisterClassW(&wc)!=0,"owner hidden window registration");Window window{CreateWindowExW(0,wc.lpszClassName,L"StuntGP NEW owner capture",WS_OVERLAPPEDWINDOW,0,0,1280,720,nullptr,nullptr,wc.hInstance,nullptr)};
    no::require(window.value!=nullptr,"owner hidden window");Renderer renderer(window.value,{true,true,false,false,true,1});
    no::require(renderer.stats().samples==1,"actual WARP single sample");
    RaceSessionLaunch launch;launch.mode=SessionMode::normal_race;launch.level_name="TRACK1";launch.width=1280;launch.height=720;
    launch.master_gain=0;launch.sound_percent=launch.music_percent=0;launch.window=reinterpret_cast<std::uint64_t>(window.value);
    auto session=std::make_unique<RaceSession>(argv[1],launch);auto& s=session->state();SessionWorldMaterials materials(*session);
    const auto roots=source_graphics_roots_from_pe();WorldMaterialLoadingContext loading;loading.source_graphics_roots=&roots;loading.scene=s.scene_mode;loading.quality=0;loading.maximum_dimension=4096;
    const auto end=std::find(s.level.track_texture.begin(),s.level.track_texture.end(),'\0');no::require(end!=s.level.track_texture.end(),"actual track prefix terminator");loading.track_prefix={s.level.track_texture.begin(),end};
    MaterialTransparencyStorage transparency;MaterialD3D12 backend(renderer,transparency,false,1);backend.reset_transparency_430690();
    materials.load_with_fonts(loading,backend);materials.publish_count_42e772();const auto& fonts=materials.validate_fonts_for(*session);
    no::require(fonts.count==2&&materials.state().reload.font_cursor==2,"actual font initialization incomplete");
    no::require(backend.resource_entries()==materials.state().names.materials.size()&&backend.resource_entries()==materials.state().names.physical_upload_count+4,"actual GPU/physical/font ownership");
    normal_native_observer::capture(*session,argv[1],output/"native-observations-before.bin");
    if(world_pools)normal_native_observer::capture_frame(*session,output/"native-frame-before.bin");
    if(scene_owners){
        normal_native_observer::capture_camera(*session,output/"native-camera-before.bin");
        normal_native_observer::capture_race_owners(*session,output/"native-race-before.bin");
        normal_native_observer::capture_vehicle_visual(*session,output/"native-visual-before.bin");
        normal_native_observer::capture_vehicle_audio_owners(*session,output/"native-vehicle-audio-before.bin");
        normal_native_observer::capture_vehicle_physics_owners(*session,output/"native-physics-before.bin");
        normal_native_observer::capture_vehicle_parameters_owners(*session,output/"native-vehicle-parameters-before.bin");
        normal_native_observer::capture_ui_scalars(*session,materials,output/"native-ui-before.bin");
    }
    no::ModelsExport exporter(*session,argv[1]);no::NativeAddresses addresses;auto capture=exporter.snapshot(&addresses,wire_version);
    if(world_pools)no::export_frame_owners(s,capture,addresses,exporter);
    if(scene_owners){
        no::export_vehicle_prefix_owners(*session,capture,addresses);
        no::export_replay_owners(*session,capture,addresses);
        no::export_vehicle_race_owners(*session,capture,addresses);
        no::export_vehicle_visual_owners(*session,capture,addresses,exporter);
        no::export_vehicle_audio_owners(*session,capture,addresses);
        no::export_vehicle_physics_owners(*session,capture,addresses);
        no::export_controller_hud_owners(*session,capture,addresses);
        no::export_race_clock_text_owners(*session,capture,addresses);
        no::export_ui_scalar_owners(*session,materials,capture,addresses);
        no::export_camera_owners(*session,capture,addresses,exporter);
        // Keep every parameter owner; its precheck uses the explicit format
        // policy selected before the first model owner was appended.
        no::export_vehicle_parameters_owners(*session,capture,addresses);
        normal_native_observer::capture_camera(*session,output/"native-camera-after.bin");
        normal_native_observer::capture_race_owners(*session,output/"native-race-after.bin");
        normal_native_observer::capture_vehicle_visual(*session,output/"native-visual-after.bin");
        normal_native_observer::capture_vehicle_audio_owners(*session,output/"native-vehicle-audio-after.bin");
        normal_native_observer::capture_vehicle_physics_owners(*session,output/"native-physics-after.bin");
        normal_native_observer::capture_vehicle_parameters_owners(*session,output/"native-vehicle-parameters-after.bin");
        normal_native_observer::capture_ui_scalars(*session,materials,output/"native-ui-after.bin");
    }
    if(world_pools)normal_native_observer::capture_frame(*session,output/"native-frame-after.bin");
    normal_native_observer::capture(*session,argv[1],output/"native-observations-after.bin");
    std::ofstream wire(output/"native-owners.bin",std::ios::binary);const std::array<no::NamedImage,1> wire_images{{{"real-normal-race-before-first-parent",&capture}}};no::save_file(wire,std::span<const no::NamedImage>(wire_images));wire.flush();no::require(bool(wire),"model owner wire flush");
    renderer.finish();renderer.collect_diagnostics();renderer.check_health();no::require(renderer.stats().debug_active&&renderer.stats().debug_errors==0,"actual WARP loader diagnostics");
    std::ofstream report(output/"native-result.json");report<<"{\"passed\":true,\"real_session\":true,\"mode\":\"normal_race\",\"level\":\"TRACK1\",\"width\":1280,\"height\":720,\"real_warp_material_font_loading\":true,\"actual_samples\":1,\"fonts\":"<<fonts.count
        <<",\"gpu_resources\":"<<backend.resource_entries()<<",\"physical_records\":"<<materials.state().names.materials.size()<<",\"snapshot_allocations\":"<<capture.allocations().size()<<",\"world_pools_exported\":"<<(world_pools?"true":"false")<<",\"scene_owners_exported\":"<<(scene_owners?"true":"false")<<",\"gpu_debug_errors\":0,\"normal_parent_calls\":0,\"original_runs\":0,\"complete_normal_codec\":false,\"models\":[";
    for(std::size_t i=0;i<session->models().size();++i){auto& m=session->models().at(i);no::require(safe_name(m.source_name()),"unsafe actual source model name for receipt");if(i)report<<',';
        report<<"{\"handle\":"<<i<<",\"identity\":"<<session->models().identity(i)<<",\"name\":\""<<m.source_name()<<"\",\"lod_count\":"<<m.lod_count()<<'}';}
    report<<"],\"physical_bindings\":[";unsigned bound{};
    for(unsigned i=0;i<6;++i){const auto& binding=s.vehicles.models[i];if(i)report<<',';report<<"{\"slot\":"<<i<<",\"body\":"<<session->models().identity(binding.model)<<",\"shadow\":"<<session->models().identity(binding.shadow)<<'}';bound+=binding.model.has_value()+binding.shadow.has_value();}
    report<<"],\"registered_physical_models\":"<<bound<<",\"track_identity\":"<<session->models().identity(session->scene_resources().model_handle(3))<<",\"sky_identity\":"<<session->models().identity(session->scene_resources().model_handle(2))<<"}\n";
    report.flush();no::require(bool(report),"model owner receipt flush");std::cout<<"real Session model owner export complete: models="<<session->models().size()<<" physical="<<bound<<" allocations="<<capture.allocations().size()<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
