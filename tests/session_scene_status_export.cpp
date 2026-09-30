#include "session_scene_status_wire.hpp"
#include "sgn/session_scene_frame.hpp"
#include "sgn/race_session.hpp"
#include "sgn/material_d3d12.hpp"
#include "sgn/material_texture_replacement.hpp"
#include "sgn/race_overlay_textures.hpp"
#include "sgn/retail_material_filesystem.hpp"
#include "sgn/retail_screen_polygons.hpp"
#include <iostream>

using namespace sgn;using namespace sgn::retail;
namespace {
namespace sw=session_status_wire;
struct Window {HWND handle{};~Window(){if(handle)DestroyWindow(handle);}};
struct Sink final:PolygonSubmissionSink {
    const ViewportState& viewport;sw::Events events;
    explicit Sink(const ViewportState& v):viewport(v){}
    std::uint32_t record(std::vector<std::uint32_t> e){sw::require(events.size()<4096,"status event bound");events.push_back(std::move(e));return 0;}
    std::uint32_t set_render_state(std::uint32_t s,std::uint32_t v)override{return record({1,s,v});}
    std::uint32_t set_texture(std::uint32_t s,std::uint32_t v)override{return record({2,s,v});}
    std::uint32_t set_texture_stage_state(std::uint32_t s,std::uint32_t t,std::uint32_t v)override{return record({3,s,t,v});}
    std::uint32_t draw(std::uint32_t type,std::uint32_t fvf,std::span<const PolygonSubmissionVertex> v,std::uint32_t flags,const ViewportState& view)override{
        sw::require(&view==&viewport&&type==6&&fvf==0x2c4&&v.size()==4&&flags==0x14,"status must submit real local glyph packet");
        std::vector<std::uint32_t> e{4,type,fvf,sw::narrow(v.size()),flags};for(const auto& vertex:v)e.insert(e.end(),vertex.words.begin(),vertex.words.end());return record(std::move(e));
    }
    std::uint32_t draw_indexed(std::uint32_t,std::uint32_t,std::span<const PolygonSubmissionVertex>,std::span<const std::uint16_t>,std::uint32_t,const ViewportState&)override{throw FormatError("status unexpectedly submitted indexed geometry");}
};
sw::Snapshot capture(const RaceSession& session,const SessionWorldMaterials& materials,const PolygonSubmissionState& scratch){
    const auto& s=session.state();const auto& fonts=materials.validate_fonts_for(session);const auto& m=materials.validate_for(session);sw::Snapshot out;
    std::array<std::uint32_t,2*retail_font_record_words> font_words{};
    for(unsigned f=0;f<2;++f)for(unsigned at=0;at<retail_font_record_bytes;at+=4){
        auto value=fonts.source_word(f,at);
        if(at==4){sw::require(value>=1&&value<=fonts.scratch_allocations(),"live font scratch identity");value=0x10020000u+(value-1)*1024u;}
        font_words[f*retail_font_record_words+at/4]=value;
    }
    out.field(0x577bd0,"font-records-with-canonical-scratch-identities",font_words);out.field(0x577ad0,"font-index",fonts.index);out.field(0x57b318,"glyph-map",fonts.glyph_map);
    sw::require(fonts.current<=2,"live font selector identity");out.field(0x57b310,"font-current",fonts.current?0x577bd0u+(fonts.current-1)*0x1ba0u:0u);
    out.field(0x57b314,"font-count",fonts.count);out.field(0x57b418,"font-digit-advance",fonts.digit_advance_bits);
    out.field(0x61c5e4,"shared-material-cache",s.ui.selected_material);out.field(0x592050,"shared-specular",scratch.specular_enabled);
    out.field(0x624984,"view-quality",s.view_quality.second);out.field(0x589e08,"legacy-driver-restriction",s.view_quality.legacy_driver_restriction);
    static_assert(sizeof(ViewportState)==38*4);const auto viewport=std::bit_cast<std::array<std::uint32_t,38>>(s.viewport);
    for(unsigned i=0;i<viewport.size();++i)out.field(sw::viewport_addresses[i],"viewport-"+std::to_string(i),viewport[i]);
    out.field(0x576be0,"split-setting",s.split_setting);
    std::array<std::uint32_t,255*7> physical{};
    for(unsigned slot=0;slot<255;++slot)for(unsigned at=0;at<28;at+=4)physical[slot*7+at/4]=m.names.materials.source_word(slot,at);
    out.field(0x6227d4,"all-physical-material-words",physical);out.field(0x6243b8,"physical-material-cursor",m.names.materials.source_cursor());
    out.field(0x576c38,"shared-descriptor",s.ui.descriptor);
    sw::require(scratch.alpha_vertices.size()>=8,"status alpha owner capacity");out.add(0x58d818,"global-alpha-eight",scratch.alpha_vertices.data(),8*sizeof(PolygonSubmissionVertex));
    out.field(0x61299c,"hud-block",s.hud_block);sw::require(s.controls.current<4,"live current controller");
    out.field(0x4722d0,"current-controller",sw::controller_base+s.controls.current*sw::controller_stride);
    for(unsigned i=0;i<4;++i){const auto& c=s.controls.controllers[i];sw::require(c.vehicle<6,"live controller vehicle");
        out.field(sw::controller_base+i*sw::controller_stride,"controller-active-"+std::to_string(i),c.active);
        out.field(sw::controller_base+i*sw::controller_stride+0xac,"controller-vehicle-"+std::to_string(i),sw::vehicle_base+c.vehicle*sw::vehicle_stride);}
    for(unsigned i=0;i<6;++i)out.field(sw::vehicle_base+i*sw::vehicle_stride+0x76a0,"vehicle-status-"+std::to_string(i),s.vehicles.startup[i].fields_768c[5]);
    out.field(0x61c5f0,"screen-allocation",s.screen_base.is_null()?0u:sw::screen_base);out.field(0x61c604,"screen-count",s.screen_polygon_count);out.field(sw::screen_base,"screen-all-records",s.screen_polygons);
    out.field(0x61c508,"world-frame-stamp",s.polygon_frame.frame_stamp);out.field(0x61c3e8,"projection",s.frame_projection);out.field(0x61c594,"world-view",s.frame_world_view);
    out.field(0x61c7cc,"matrix-arena",s.view_matrices.matrices);out.field(0x6227d0,"matrix-count",s.view_matrices.count);
    out.field(0x6227cc,"matrix-cursor",s.view_matrices.cursor?0x61c7ccu+*s.view_matrices.cursor:0u);
    for(unsigned i=0;i<10;++i)out.field(0x485258+i*12,"star-phase-"+std::to_string(i),s.ui.stars.phase[i]);
    return out;
}
}
int main(int argc,char** argv)try{
    sw::require(argc==3,"status exporter expects retail root and fresh output directory");
    const auto output=std::filesystem::absolute(argv[2]);sw::require(!std::filesystem::exists(output),"status output already exists");std::filesystem::create_directories(output);
    WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"SGN-Actual-Session-Status-Oracle";
    sw::require(RegisterClassW(&wc)!=0,"status hidden window registration");Window window{CreateWindowExW(0,wc.lpszClassName,L"StuntGP NEW status oracle",WS_OVERLAPPEDWINDOW,0,0,1280,720,nullptr,nullptr,wc.hInstance,nullptr)};
    sw::require(window.handle!=nullptr,"status hidden window");Renderer renderer(window.handle,{true,true,false,false,true,1});
    sw::require(renderer.stats().samples==1,"actual WARP single sample");
    RaceSessionLaunch launch;launch.level_name="TRACK1";launch.width=1280;launch.height=720;launch.master_gain=0;launch.sound_percent=launch.music_percent=0;launch.window=reinterpret_cast<std::uint64_t>(window.handle);
    auto session=std::make_unique<RaceSession>(argv[1],launch);auto& s=session->state();SessionWorldMaterials materials(*session);
    const auto roots=source_graphics_roots_from_pe();WorldMaterialLoadingContext loading;loading.source_graphics_roots=&roots;loading.scene=s.scene_mode;loading.quality=0;loading.maximum_dimension=4096;
    const auto end=std::find(s.level.track_texture.begin(),s.level.track_texture.end(),'\0');sw::require(end!=s.level.track_texture.end(),"actual track prefix terminator");loading.track_prefix={s.level.track_texture.begin(),end};
    MaterialTransparencyStorage transparency;MaterialD3D12 backend(renderer,transparency,false,1);backend.reset_transparency_430690();
    materials.load_with_fonts(loading,backend);materials.publish_count_42e772();const auto& fonts=materials.validate_fonts_for(*session);
    sw::require(fonts.count==2&&materials.state().reload.font_cursor==2,"actual font initialization incomplete");
    sw::require(backend.resource_entries()==materials.state().names.materials.size()&&backend.resource_entries()==materials.state().names.physical_upload_count+4,"actual GPU/physical/font ownership");
    MaterialTextureReplacement replacement(backend,materials.replacement_storage(*session));RaceOverlayTextures textures(session->assets(),replacement,session->shared_platform());
    PolygonSubmissionState scratch;scratch.depth_scale_bits=0x43000000;prepare_polygon_submission_ui_capacity(scratch,desktop_polygon_submission_budget);
    SessionWorldFrameResources resources;resources.submission_budget=desktop_polygon_submission_budget;
    sw::require(s.controls.current<4,"status current controller");auto& controller=s.controls.controllers[s.controls.current];sw::require(controller.vehicle<6,"status current physical vehicle");
    auto& state=s.vehicles.startup[controller.vehicle].fields_768c[5];s.hud_block=1;controller.active=0;state=1;
    // Explicit caller input, followed by the actual reconstructed425830.
    // The original executor runs its complete425830 on the same packet.
    SourceScreenPolygon packet{};packet.flags=0x48;packet.alpha=128;packet.vertex_count=4;
    for(unsigned i=0;i<4;++i){const bool right=i==1||i==2,bottom=i>=2;auto& v=packet.vertices[i];v.x=right?220.0f:200.0f;v.y=bottom?200.0f:180.0f;v.z=0.5f;v.u=right?1.0f:0.0f;v.v=bottom?1.0f:0.0f;v.color=0xffffff;}
    sw::require(s.screen_polygon_count==0,"status initial screen queue must be empty");const auto initial=capture(*session,materials,scratch);
    auto screen_access=lock_screen_polygon_owner(session->shared_platform());
    sw::require(append_screen_polygon({live_screen_polygon_reader(screen_access),screen_access.count_word()},packet,s.viewport)&&s.screen_polygon_count==1,"actual native425830 did not accept packet");
    std::ofstream out(output/"native.bin",std::ios::binary);sw::magic(out);initial.save(out);sw::write(out,&packet,sizeof(packet));sw::number(out,2);
    std::uint64_t event_count{},draw_count{};std::array<unsigned,2> per_draws{};
    for(unsigned step=0;step<2;++step){
        const unsigned requested=step==0?1u:2u;state=requested;sw::number(out,requested);const auto before=capture(*session,materials,scratch);before.save(out);
        const auto stamp=s.polygon_frame.frame_stamp;Sink sink(s.viewport);
        const auto result=prepare_session_scene_frame(*session,materials,{1,0},scratch,sink,resources,textures);
        sw::require(result.kind==(step==0?SessionSceneFrameKind::race_finished:SessionSceneFrameKind::no_player)&&!result.camera,"actual production parent selected wrong status");
        sw::require(s.screen_polygon_count==1&&s.polygon_frame.frame_stamp==stamp,"status consumed screen queue or ran world");
        capture(*session,materials,scratch).save(out);sw::save_events(out,sink.events);event_count+=sink.events.size();
        for(const auto& e:sink.events)if(e[0]==4){++per_draws[step];++draw_count;}
        sw::require(per_draws[step]>0,"status text did not draw");
    }
    out.flush();sw::require(bool(out),"native status output flush");renderer.finish();renderer.collect_diagnostics();renderer.check_health();
    sw::require(renderer.stats().debug_active&&renderer.stats().debug_errors==0,"actual WARP loader diagnostics");
    std::ofstream receipt(output/"native-result.json");receipt<<"{\"passed\":true,\"real_session\":true,\"real_warp_material_font_loading\":true,\"actual_samples\":1,\"fonts\":"<<fonts.count<<",\"gpu_resources\":"<<backend.resource_entries()<<",\"physical_records\":"<<materials.state().names.materials.size()<<",\"original_matched\":false,\"native_parent_calls\":2,\"native425830_setup_calls\":1,\"api_events\":"<<event_count<<",\"draws\":"<<draw_count<<",\"step_draws\":["<<per_draws[0]<<','<<per_draws[1]<<"],\"retained_nonempty_screen_queue\":true,\"normal_setup_calls\":0,\"gpu_debug_errors\":0}\n";
    receipt.flush();sw::require(bool(receipt),"native status receipt flush");std::cout<<"actual Session status export complete; API="<<event_count<<" draws="<<draw_count<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
