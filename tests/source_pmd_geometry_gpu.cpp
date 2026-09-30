#include "source_pmd_geometry_fixture.hpp"
#include "sgn/renderer.hpp"
#include <array>
#include <iostream>
#include <optional>

namespace {struct Window{HWND handle{};~Window(){if(handle)DestroyWindow(handle);}};}
int main(int argc,char** argv){try{
    using namespace geometry_receiving;
    check(argc==4,"expected actual retail root, output, hardware|warp");const bool warp=std::string_view(argv[3])=="warp";check(warp||std::string_view(argv[3])=="hardware","adapter selector");
    const auto output=std::filesystem::absolute(argv[2]);std::filesystem::create_directories(output);Fixture f(argv[1]);
    WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"SGN-Canonical-PMD-Receiving";check(RegisterClassW(&wc)!=0,"RegisterClass");
    Window window{CreateWindowExW(0,wc.lpszClassName,L"PMD receiving",WS_OVERLAPPEDWINDOW,0,0,960,540,nullptr,nullptr,wc.hInstance,nullptr)};check(window.handle!=nullptr,"CreateWindow");
    Renderer renderer(window.handle,{true,warp,false,false,true});renderer.resize(960,540);
    std::uint64_t captures=0,submitted=0,guards=0;
    for(const std::string name:{"car1","track1"}){
        const auto id=f.load(name);const auto model=f.model(id);
        if(name=="car1"){
            // Explicit receiving selection, not a guessed Session/RNG default.
            // The existing source413190/413220 suffix pass precedes upload.
            std::array<LevelCarSlot,6> selected{};std::array<std::int8_t,6> random_liveries{};
            selected[0].source_word00=id;selected[0].settings.livery=2;selected[0].settings.tyre=1;
            remap_level_vehicle_textures(f.resources,selected,random_liveries);
        }
        const SourcePmdGeometryAdapter adapter(model,name=="track1"?"TRACKSET02":"");auto geometry=adapter.capture(0);
        std::optional<std::string> selected_body_texture;std::optional<unsigned> selected_body_gpu_slot;
        // Bounded presentation camera for transport qualification only. No retail camera behavior claim.
        const auto center=(geometry.model.minimum+geometry.model.maximum)*0.5f;const auto extent=geometry.model.maximum-geometry.model.minimum;
        const float radius=std::max({std::abs(extent.x),std::abs(extent.y),std::abs(extent.z),100.0f});
        CameraState camera;camera.position={center.x+radius,center.y+radius,center.z+radius,0};camera.target={center.x,center.y,center.z,0};camera.up={0,1,0,0};
        camera.lens=400;camera.near_clip=32;camera.far_factor=10;camera.half_width=480;camera.half_height=270;camera_view_basis(camera);camera_projection(camera);CameraModelTransform outer;
        View view;view.world_to_view=camera_world_view(camera);view.retail_viewport=View::RetailViewport{{camera.projection[0],camera.projection[5],camera.projection[10],camera.projection[14],32},std::max(40960.0f,radius*10),true};
        renderer.load(geometry.model,f.assets);const auto texture_bytes=renderer.stats().uploaded_texture_bytes;
        for(unsigned slot=0;slot<4;++slot){geometry=adapter.capture(slot);
            if(name=="car1"){
                unsigned body_polygons=0;
                for(const auto& surface:geometry.model.surfaces)if(surface.part==0){
                    const auto texture=canonical(geometry.model.textures.at(surface.texture));
                    if(texture.find("grid")==std::string::npos)continue;
                    check(texture.ends_with("b1grid2"),"body LOD did not resolve the explicit selected livery");
                    const auto gpu_slot=renderer.texture_index(0,surface.texture);
                    if(!selected_body_texture){selected_body_texture=texture;selected_body_gpu_slot=gpu_slot;}
                    check(texture==*selected_body_texture&&gpu_slot==*selected_body_gpu_slot,"source LOD changed resolved body skin or GPU binding");++body_polygons;
                }
                check(body_polygons>0,"body LOD has no livery-bearing polygon");
            }
            const auto views=adapter.capture_views(slot,camera,outer);const SceneInstance instance{0,{},views.parts};const LiveInstanceView live{views.model_to_view};
            renderer.stage_mesh(0,geometry);for(unsigned frame=0;frame<3;++frame){renderer.draw_scene(view,std::span(&instance,1),"","",1,std::span(&live,1));++submitted;}
            renderer.capture(output/(name+"-lod"+std::to_string(slot)+".bmp"));++captures;check(renderer.stats().uploaded_texture_bytes==texture_bytes,"LOD unexpectedly reloaded shared textures");}
        const auto p=model.part(0,0);const auto h=p.attributes().at(0).value();const auto first=std::uint32_t(h[6])|(std::uint32_t(h[7])<<8);const auto field=p.coordinates().at(model.corner_indices().at(first).value());auto c=field.value();c[0]=std::bit_cast<std::uint32_t>(std::bit_cast<float>(c[0])+0.25f);c[3]^=0x123456;field.set(c);
        geometry=adapter.capture(0);const auto views=adapter.capture_views(0,camera,outer);const SceneInstance instance{0,{},views.parts};const LiveInstanceView live{views.model_to_view};
        renderer.stage_mesh(0,geometry);(void)f.bank.release_42e980(model.allocation());allocation_rejects([&]{(void)adapter.capture(0);});++guards;
        // Submitted transport is renderer-owned and can complete after canonical source free.
        for(unsigned frame=0;frame<3;++frame){renderer.draw_scene(view,std::span(&instance,1),"","",1,std::span(&live,1));++submitted;}
        renderer.capture(output/(name+"-mutated-source-freed.bmp"));++captures;renderer.finish();renderer.check_health();check(renderer.stats().debug_errors==0,"D3D12 debug diagnostics");
    }
    const auto stats=renderer.stats();check(stats.debug_active&&submitted==30&&captures==10&&guards==2,"GPU receiving coverage/debug layer unavailable");
    std::cout<<"{\"frames\":"<<submitted<<",\"captures\":"<<captures<<",\"stale_guards\":"<<guards<<",\"live_updates\":"<<stats.live_updates<<",\"debug_errors\":"<<stats.debug_errors<<",\"explicit_livery\":2,\"explicit_tyre\":1,\"visual_skin_parity_qualified\":false,\"original_gpu_parity\":false,\"errors\":0}\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
