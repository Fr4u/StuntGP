// Main-only future receiving target. Actual Renderer/material/capture/adapter;
// analytic host panels, no RaceApplication activation or retail pixel oracle.
#include "sgn/renderer_loading_surfaces.hpp"
#include "sgn/polygon_submission_capture.hpp"
#include "sgn/light_gamma_d3d12.hpp"
#include "material_texture_replacement_gpu_helpers.hpp"
#include <ddraw.h>
#include <unordered_map>
#include <type_traits>

namespace {
static_assert(std::is_nothrow_copy_assignable_v<SdrGammaRamp>);
static_assert(std::is_nothrow_move_assignable_v<ImmutablePolygonSubmission>);
static_assert(noexcept(std::declval<PolygonSurfaceRecipes&>().exchange()));
constexpr PolygonSubmissionCaptureLimits limits{{{16,4096},40},3};
void panel(PolygonDeviceContext& device,bool right,std::int32_t canvas_width=384,std::int32_t canvas_height=256){
    const float left=right?208.0f:16.0f;const float top=right?56.0f:16.0f;
    std::array<PolygonSubmissionVertex,4> vertices{};unsigned index=0;
    for(const auto [x,y]:std::array<std::array<float,2>,4>{{{left,top},{left+150,top},{left+150,224},{left,224}}})
        vertices[index++].words={std::bit_cast<std::uint32_t>(x),std::bit_cast<std::uint32_t>(y),0x3f000000,0x3f800000,0xffffffff,0,0x3f000000,0x3f000000,0,0};
    ViewportState viewport;viewport.full_width=canvas_width;viewport.full_height=canvas_height;
    require(device.draw(6,0x2c4,vertices,0x1c,viewport)==0,"surface panel capture");
}
ImmutablePolygonSubmission capture_panel(PolygonSubmissionCapture& capture,bool right,std::uint64_t tick,
    std::int32_t canvas_width=384,std::int32_t canvas_height=256){
    capture.begin_source_frame(tick,limits);
    try{
        require(capture.device().begin_scene()==0,"surface panel BeginScene");panel(capture.device(),right,canvas_width,canvas_height);
        require(capture.device().end_scene()==0,"surface panel EndScene");return capture.seal_source_frame();
    }catch(...){capture.discard_failed_source_frame();throw;}
}
void replace_chart(MaterialD3D12& backend,std::uint16_t first,std::uint16_t second){
    std::vector<std::uint16_t> words(256u*256u);
    for(unsigned y=0;y<256;++y)for(unsigned x=0;x<256;++x)words[y*256+x]=((x/16+y/16)&1u)?first:second;
    backend.replace_primary(15,{256,256,words}); //Explicit analytic platform fixture.
}
Bytes read_capture(Renderer& renderer,const std::filesystem::path& path){renderer.capture(path);healthy(renderer);return read_file(path);}
Bytes refresh(RendererLoadingSurfaces& surface,Renderer& renderer,const std::filesystem::path& path){
    require(surface.draw(renderer),"expected a real selected front");return read_capture(renderer,path);
}
void suspend(RendererLoadingSurfaces& surface,PolygonSubmissionCapture& capture,PolygonSurfaceRecipes& recipes){
    surface.prepare_suspend();capture.suspend_renderer_for_recreation(recipes.submissions);surface.commit_suspend();
}
void bind(RendererLoadingSurfaces& surface,PolygonSubmissionCapture& capture,PolygonSurfaceRecipes& recipes,Renderer& renderer){
    surface.prepare_bind(renderer);const auto roles=capture.rebind_renderer(renderer,recipes.submissions,recipes.ramps);
    surface.commit_bind(renderer,roles);
}
struct ResourceGraph {
    std::vector<std::vector<std::size_t>> ordinals;
    std::vector<std::uint64_t> epochs;
    std::vector<std::uint32_t> tokens;
    std::vector<std::vector<std::uint16_t>> words;
    std::vector<ImmutablePolygonDeviceFrame> commands; //CPU-only source command objects.
};
ResourceGraph graph(MaterialD3D12& backend,const PolygonSurfaceRecipes& recipes,const std::filesystem::path& folder){
    ResourceGraph result;std::unordered_map<ID3D12Resource*,std::size_t> ids;
    const auto observe=[&](const PolygonGpuTextures& bank){
        std::vector<std::size_t> ordinals;result.epochs.push_back(bank.epoch);
        for(const auto& item:bank.entries){
            require(bool(item.resource),"missing actual graph resource");
            const auto [found,inserted]=ids.emplace(item.resource.Get(),ids.size());
            if(inserted){auto words=read_words(item);require(item.recovery&&words==item.recovery->words,"CPU retained texture differs from GPU");
                result.tokens.push_back(item.token);write_words(folder/("resource-"+std::to_string(found->second)+".u16"),words);
                result.words.push_back(std::move(words));}
            ordinals.push_back(found->second);
        }
        result.ordinals.push_back(std::move(ordinals));
    };
    observe(*backend.bindings_snapshot());
    for(const auto& recipe:recipes.submissions){require(bool(recipe),"graph requires both recipes");
        for(const auto& segment:recipe->segments){observe(*segment.textures);result.commands.push_back(segment.device_frame);}}
    return result; //Local pointer map and every GPU-bank borrow die here.
}
void require_graph(const ResourceGraph& before,const ResourceGraph& after){
    require(before.ordinals==after.ordinals&&before.epochs==after.epochs&&before.tokens==after.tokens&&
        before.words==after.words&&before.commands==after.commands,"two-role rebind changed versions, sharing, pixels or CPU commands");
}
void require_suspended(RendererLoadingSurfaces& surface,Renderer& renderer,const PolygonDeviceContext& device){
    const auto before=device.snapshot();
    require(surface.surface_operation_432a60(0,1)==static_cast<std::uint32_t>(DDERR_SURFACELOST)&&
        surface.begin_scene()==static_cast<std::uint32_t>(DDERR_SURFACELOST)&&
        surface.end_scene()==static_cast<std::uint32_t>(DDERR_SURFACELOST)&&
        surface.finalize_423430(0)==static_cast<std::uint32_t>(DDERR_SURFACELOST),"suspended adapter accepted an operation");
    bool rejected=false;try{(void)surface.draw(renderer);}catch(const FormatError&){rejected=true;}
    require(rejected&&same_polygon_snapshot(before,device.snapshot()),"suspended adapter changed context or displayed pixels");
}
}
#include "source_surface_resize_receiving.hpp"
int main(int argc,char** argv)try{
    require(argc==4,"expected --hardware|--warp retail-root fresh-output");
    const bool warp=std::string_view(argv[1])=="--warp";require(warp||std::string_view(argv[1])=="--hardware","surface adapter option");
    const auto output=std::filesystem::absolute(argv[3]);require(!std::filesystem::exists(output),"surface receiving output must be fresh");
    std::filesystem::create_directories(output);AssetStore assets(argv[2]);RetailMaterialFileSystem files(assets,argv[2]);
    WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"SGN-Surface-Pair-Receiving";
    require(RegisterClassW(&wc)!=0,"surface class");
    Window window{CreateWindowExW(0,wc.lpszClassName,L"StuntGP NEW surface pair receiving",WS_OVERLAPPEDWINDOW,0,0,384,256,nullptr,nullptr,wc.hInstance,nullptr)};
    require(window.handle!=nullptr,"surface window");
    std::ofstream results(output/"cases.csv");require(bool(results),"surface results file");
    results<<"case,requested_samples,actual_samples,resource_versions,source_callbacks_replayed\n";
    unsigned passed=0;
    for(unsigned requested:{1u,4u})for(bool split:{false,true}){
        const auto label=std::to_string(requested)+(split?"-split":"-alias");const auto folder=output/label;
        std::filesystem::create_directories(folder);RenderOptions options{true,warp,false,false,true};options.requested_samples=requested;
        auto renderer=std::make_unique<Renderer>(window.handle,options);renderer->resize(384,256);
        const auto actual_samples=renderer->stats().samples;
        MaterialTransparencyStorage transparency;MaterialD3D12 backend(*renderer,transparency,split,1);backend.reset_transparency_430690();
        auto world=std::make_unique<WorldMaterialLoadingState>();const auto roots=source_graphics_roots_from_pe();WorldMaterialLoadingContext loading;
        loading.source_graphics_roots=&roots;loading.maximum_dimension=256;load_world_materials({},*world,loading,files,backend);world->published_material_count=16;
        require(world->names.materials.size()==16&&backend.resource_entries()==16,"actual normal material prelude");
        PolygonDeviceContext device(device_seed(backend.resource_primary_token(15)));
        PolygonSubmissionCapture capture(device,backend,world->names.materials);
        replace_chart(backend,0xfc00,0xc400);auto a=capture_panel(capture,false,11);
        draw_material(*renderer,*backend.bindings_snapshot(),backend.resource_primary_token(15),folder/"material-witness.bmp");healthy(*renderer);
        replace_chart(backend,0x83e0,0x8200);auto b=capture_panel(capture,true,22);
        replace_chart(backend,0x801f,0x8010);const auto latest_epoch=backend.resource_epoch();
        require(latest_epoch>b->segments.back().textures->epoch&&b->segments.back().textures->epoch>a->segments.back().textures->epoch,"three distinct material histories required");
        auto ramp_a=identity_sdr_gamma(),ramp_b=identity_sdr_gamma();
        for(unsigned i=0;i<256;++i){ramp_a[i]=static_cast<std::uint16_t>(i*193u);ramp_b[256+i]=static_cast<std::uint16_t>(i*129u);}
        // Pixel witnesses use the existing direct path BEFORE surface mode.
        renderer->draw_polygon_submission(*a,ramp_a);const auto pixels_a=read_capture(*renderer,folder/"reference-a.bmp");
        renderer->draw_polygon_submission(*b,ramp_b);const auto pixels_b=read_capture(*renderer,folder/"reference-b.bmp");
        require(pixels_a!=pixels_b,"A/B witnesses indistinguishable");
        PolygonSurfaceRecipes recipes;RendererLoadingSurfaces surface(*renderer,device,recipes);LightGammaD3D12 gamma;
        const auto empty_presents=renderer->stats().presents;
        prepare_loading_surface_432a60(surface);
        require(!surface.draw(*renderer)&&!recipes.submissions[0]&&!recipes.submissions[1]&&renderer->stats().presents==empty_presents,"empty Flip invented a presented image");
        gamma.apply(ramp_a);surface.completed_back(a,gamma.snapshot());
        const auto a_id=renderer->polygon_surface_roles().back_image;
        require(a_id&&!renderer->polygon_surface_roles().front_image&&recipes.submissions[1]==a&&!surface.draw(*renderer)&&renderer->stats().presents==empty_presents,"stage A prematurely displayed");
        prepare_loading_surface_432a60(surface);
        require(recipes.submissions[0]==a&&!recipes.submissions[1]&&recipes.ramps[0]==ramp_a&&renderer->polygon_surface_roles().front_image==a_id,"CPU/GPU Flip A mismatch");
        require(refresh(surface,*renderer,folder/"a.bmp")==pixels_a,"front A differs from actual direct witness");
        gamma.apply(ramp_b);const auto before_b=renderer->stats().presents;surface.completed_back(b,gamma.snapshot());
        const auto b_id=renderer->polygon_surface_roles().back_image;
        require(b_id&&b_id!=a_id&&renderer->stats().presents==before_b&&recipes.submissions[0]==a&&recipes.submissions[1]==b,"stage B changed front or Presented");
        require(read_capture(*renderer,folder/"after-stage-b.bmp")==pixels_a,"stage overwrote last physical capture");
        const auto source_state=device.snapshot();const auto live_updates=renderer->stats().live_updates;
        gamma.apply(identity_sdr_gamma());const auto applies=gamma.apply_count();
        for(unsigned i=0;i<20;++i)require(surface.draw(*renderer),"front refresh rejected");
        require(read_capture(*renderer,folder/"refresh-a.bmp")==pixels_a&&renderer->stats().presents==before_b+20&&
            renderer->stats().live_updates==live_updates&&renderer->stats().draws==0&&same_polygon_snapshot(source_state,device.snapshot())&&
            backend.resource_epoch()==latest_epoch&&gamma.apply_count()==applies&&recipes.ramps[0]==ramp_a,"refresh replayed source/geometry/gamma or changed pair");
        // Actual source wrapper plus explicit host exception after its return.
        // This proves the commit is not delayed until a caller later returns.
        bool threw=false;try{prepare_loading_surface_432a60(surface);throw FormatError("receiving exception after accepted Flip");}
        catch(const FormatError&){threw=true;}
        require(threw&&recipes.submissions[0]==b&&recipes.submissions[1]==a&&recipes.ramps[0]==ramp_b&&recipes.ramps[1]==ramp_a&&
            renderer->polygon_surface_roles().front_image==b_id&&renderer->polygon_surface_roles().back_image==a_id,"exception rolled back or delayed paired Flip");
        std::uint32_t finalized=0;
        require(surface.finalize_423430(0)==static_cast<std::uint32_t>(DDERR_NOTLOCKED),"unmapped surface falsely unlocked successfully");
        finalize_loading_frame_431a80(finalized,surface);require(finalized==1&&same_polygon_snapshot(source_state,device.snapshot()),"source ignored-Unlock result handling changed");
        renderer->inject_failure_for_test(RenderFailurePoint::polygon_frame_present);const auto old_presents=renderer->stats().presents;
        bool present_failed=false;try{(void)surface.draw(*renderer);}catch(const GraphicsError& error){require(error.result==E_FAIL,"Present injection result");present_failed=true;}
        require(present_failed&&renderer->stats().presents==old_presents&&recipes.submissions[0]==b&&renderer->polygon_surface_roles().front_image==b_id,"failed Present published wrong state");
        require(read_capture(*renderer,folder/"failed-present-last-capture.bmp")==pixels_a,"failed Present replaced last successful capture in observed path");
        require(refresh(surface,*renderer,folder/"b.bmp")==pixels_b,"front B refresh retry differs");
        prepare_loading_surface_432a60(surface);require(refresh(surface,*renderer,folder/"back-a.bmp")==pixels_a,"old actual back A was lost");
        prepare_loading_surface_432a60(surface);require(refresh(surface,*renderer,folder/"front-b-again.bmp")==pixels_b,"second role exchange lost B");
        const auto before_folder=folder/"before-graph";std::filesystem::create_directories(before_folder);
        const auto before=graph(backend,recipes,before_folder);const auto ramps=recipes.ramps;
        const auto generation=world->names.materials.generation();const auto cursor=world->names.materials.source_cursor();
        const auto transparency_before=transparency;
        require(before.words.size()==backend.bindings_snapshot()->entries.size()+2,"resource graph must retain three primary versions");
        a.reset();b.reset(); //Only the two actual role recipes may own the old GPU frames.
        renderer->remove_device_for_test(true);suspend(surface,capture,recipes);
        require(!recipes.submissions[0]&&!recipes.submissions[1]&&recipes.ramps==ramps,"suspension retained GPU recipes or changed ramps");
        require_suspended(surface,*renderer,device);renderer.reset();
        renderer=std::make_unique<Renderer>(window.handle,options);renderer->resize(384,256);
        renderer->inject_failure_for_test(RenderFailurePoint::polygon_upload_wait);
        bool upload_failed=false;try{bind(surface,capture,recipes,*renderer);}catch(const std::exception&){upload_failed=true;}
        require(upload_failed&&!recipes.submissions[0]&&!recipes.submissions[1]&&!backend.bound_to_renderer(*renderer)&&
            renderer->stats().pending_polygon_uploads==1&&!capture.poisoned(),"partial upload recovery published CPU roles");healthy(*renderer);
        require_suspended(surface,*renderer,device);
        // Fail AFTER Execute of the second image, with the first already fenced.
        renderer->inject_failure_for_test(RenderFailurePoint::polygon_restore_back_signal);
        bool second_failed=false;try{bind(surface,capture,recipes,*renderer);}catch(const GraphicsError& error){require(error.result==E_FAIL,"second image Signal injection result");second_failed=true;}
        const auto partial=renderer->polygon_surface_roles();
        require(second_failed&&!partial.front_image&&!partial.back_image&&!recipes.submissions[0]&&!recipes.submissions[1]&&
            recipes.ramps==ramps&&!backend.bound_to_renderer(*renderer)&&renderer->stats().presents==0&&!capture.poisoned(),"partial second-image recovery published a half pair");
        require_suspended(surface,*renderer,device);healthy(*renderer);renderer->remove_device_for_test(true);renderer.reset();
        renderer=std::make_unique<Renderer>(window.handle,options);renderer->resize(384,256);bind(surface,capture,recipes,*renderer);
        require(renderer->stats().samples==actual_samples&&renderer->stats().presents==0&&backend.bound_to_renderer(*renderer)&&recipes.ramps==ramps&&
            backend.resource_epoch()==latest_epoch&&same_polygon_snapshot(source_state,device.snapshot())&&
            world->names.materials.generation()==generation&&world->names.materials.source_cursor()==cursor&&
            transparency.bytes==transparency_before.bytes&&transparency.cursor==transparency_before.cursor,"rebind altered source owners or Presented");
        const auto after_folder=folder/"after-graph";std::filesystem::create_directories(after_folder);
        require_graph(before,graph(backend,recipes,after_folder));
        require(refresh(surface,*renderer,folder/"recreated-b.bmp")==pixels_b,"recreated front B/ramp mismatch");
        prepare_loading_surface_432a60(surface);require(refresh(surface,*renderer,folder/"recreated-a.bmp")==pixels_a,"recreated back A/ramp mismatch");
        // NEW resize policy: centered aspect-fit of each retained logical
        // canvas. Full-target clear/gamma and all CPU source owners are retained.
        const auto* front_recipe=recipes.submissions[0].get();const auto* back_recipe=recipes.submissions[1].get();
        const auto resized_ramps=recipes.ramps;const auto before_resize_presents=renderer->stats().presents;
        renderer->resize(512,320);require(!renderer->polygon_surface_roles().front_image&&!renderer->polygon_surface_roles().back_image,"resize retained wrong-size pixels");
        require(surface.draw(*renderer)&&renderer->stats().presents==before_resize_presents+1&&recipes.submissions[0].get()==front_recipe&&
            recipes.submissions[1].get()==back_recipe&&recipes.ramps==resized_ramps&&same_polygon_snapshot(source_state,device.snapshot()),"resize changed recipes or reran source");
        const auto resized_a=read_capture(*renderer,folder/"resized.bmp");
        require_resized_panel(resized_a,pixels_a,512,320,384,256,false,actual_samples);
        const auto mismatch_images=renderer->polygon_surface_roles();const auto mismatch_updates=renderer->stats().live_updates;
        require_projection_rejection([&]{surface.completed_back(recipes.submissions[1],recipes.ramps[1]);});
        require(renderer->polygon_surface_roles().front_image==mismatch_images.front_image&&
            renderer->polygon_surface_roles().back_image==mismatch_images.back_image&&renderer->stats().live_updates==mismatch_updates&&
            recipes.submissions[0].get()==front_recipe&&recipes.submissions[1].get()==back_recipe,"source-back mismatch changed retained roles");
        prepare_loading_surface_432a60(surface);
        require_resized_panel(refresh(surface,*renderer,folder/"resized-b.bmp"),pixels_b,512,320,384,256,true,actual_samples);
        prepare_loading_surface_432a60(surface);
        for(const auto [w,h]:std::array<std::array<unsigned,2>,3>{{{320,512},{511,319},{192,128}}}){
            renderer->resize(w,h);const auto image=refresh(surface,*renderer,folder/("resized-a-"+std::to_string(w)+"x"+std::to_string(h)+".bmp"));
            require_resized_panel(image,pixels_a,w,h,384,256,false,actual_samples);
            require(recipes.submissions[0].get()==front_recipe&&recipes.submissions[1].get()==back_recipe&&recipes.ramps==resized_ramps&&
                same_polygon_snapshot(source_state,device.snapshot()),"aspect-fit resize rewrote source canvas or recipes");
        }
        renderer->resize(384,256);
        require(refresh(surface,*renderer,folder/"resize-roundtrip-a.bmp")==pixels_a,"same-size raster roundtrip changed A");
        renderer->resize(512,320);renderer->resize(384,256); //Both notifications before the next adapter entry.
        require(refresh(surface,*renderer,folder/"resize-coalesced-a.bmp")==pixels_a,"coalesced resize lost recipes at original extent");
        // Ordinary back production Signal failure must retain both CPU recipes.
        const auto prior_images=renderer->polygon_surface_roles();const auto* prior_front=recipes.submissions[0].get();const auto* prior_back=recipes.submissions[1].get();
        renderer->inject_failure_for_test(RenderFailurePoint::polygon_frame_signal);
        bool stage_failed=false;try{surface.completed_back(recipes.submissions[1],recipes.ramps[1]);}
        catch(const GraphicsError& error){require(error.result==E_FAIL,"stage Signal injection result");stage_failed=true;}
        require(stage_failed&&recipes.submissions[0].get()==prior_front&&recipes.submissions[1].get()==prior_back&&
            renderer->polygon_surface_roles().front_image==prior_images.front_image&&renderer->polygon_surface_roles().back_image==prior_images.back_image,"failed stage committed a CPU/GPU recipe");
        healthy(*renderer); //Retire and inspect the real queued work before removal.
        renderer->remove_device_for_test(true);suspend(surface,capture,recipes);renderer.reset();
        renderer=std::make_unique<Renderer>(window.handle,options);renderer->resize(384,256);bind(surface,capture,recipes,*renderer);
        require(refresh(surface,*renderer,folder/"after-stage-failure-a.bmp")==pixels_a,"stage-failure recovery changed selected A");
        renderer->inject_failure_for_test(RenderFailurePoint::polygon_frame_signal);const auto refresh_presents=renderer->stats().presents;
        bool refresh_failed=false;try{(void)surface.draw(*renderer);}catch(const GraphicsError& error){require(error.result==E_FAIL,"refresh Signal injection result");refresh_failed=true;}
        require(refresh_failed&&renderer->stats().presents==refresh_presents&&recipes.ramps==resized_ramps,"failed refresh altered CPU pair or Presented");
        healthy(*renderer);
        renderer->remove_device_for_test(true);suspend(surface,capture,recipes);renderer.reset();
        renderer=std::make_unique<Renderer>(window.handle,options);renderer->resize(384,256);bind(surface,capture,recipes,*renderer);
        require(refresh(surface,*renderer,folder/"after-refresh-failure-a.bmp")==pixels_a,"refresh failure lost retained front");
        healthy(*renderer);results<<label<<','<<requested<<','<<actual_samples<<','<<before.words.size()<<",0\n";results.flush();require(bool(results),"surface result flush");++passed;
    }
    require(passed==4,"surface alias/split sample matrix");
    // All matrix Renderer owners have been destroyed before this independent
    // single-sample canvas-boundary fixture creates its device/swap chain.
    RenderOptions boundary_options{true,warp,false,false,true};boundary_options.requested_samples=1;
    resize_boundary_cases(window.handle,boundary_options,files,output/"resize-boundaries");
    std::cout<<"passed=1 surface_pair_cases=4 actual_application_execution=0 original_pixel_oracle=0 hdr_vrr_qualified=0\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
