#pragma once
// New analytic receiving for retained canvas restoration. Main executes it.
namespace {
struct ResizeBitmap {
    const Bytes& bytes;std::uint32_t width{},height{},offset{};bool top_down{};
    std::uint32_t word(std::size_t at) const {
        require(at+4<=bytes.size(),"resize BMP word outside file");
        return std::uint32_t(bytes[at])|(std::uint32_t(bytes[at+1])<<8)|
            (std::uint32_t(bytes[at+2])<<16)|(std::uint32_t(bytes[at+3])<<24);
    }
    explicit ResizeBitmap(const Bytes& data):bytes(data){
        require(bytes.size()>=54&&bytes[0]=='B'&&bytes[1]=='M',"resize receiving BMP signature");
        offset=word(10);width=word(18);const auto signed_height=std::bit_cast<std::int32_t>(word(22));
        require(width>0&&width<=16384&&signed_height!=0&&signed_height>=-16384&&signed_height<=16384,
            "resize receiving BMP dimensions");
        top_down=signed_height<0;height=static_cast<std::uint32_t>(top_down?-signed_height:signed_height);
        require(bytes[26]==1&&bytes[27]==0&&bytes[28]==32&&bytes[29]==0&&word(30)==0&&
            offset<=bytes.size()&&std::size_t(width)*height*4<=bytes.size()-offset,"resize receiving BMP layout");
    }
    std::array<unsigned,3> rgb(unsigned x,unsigned y) const {
        require(x<width&&y<height,"resize receiving pixel outside image");
        const auto row=top_down?y:height-1-y;const auto at=offset+(std::size_t(row)*width+x)*4;
        return {bytes[at+2],bytes[at+1],bytes[at]};
    }
};
bool close_resize_rgb(const std::array<unsigned,3>& a,const std::array<unsigned,3>& b){
    for(unsigned c=0;c<3;++c)if(std::abs(static_cast<int>(a[c])-static_cast<int>(b[c]))>1)return false;
    return true;
}
void require_resized_panel(const Bytes& pixels,const Bytes& reference,unsigned width,unsigned height,
    unsigned logical_width,unsigned logical_height,bool right,unsigned samples){
    const ResizeBitmap actual(pixels),old(reference);
    require(actual.width==width&&actual.height==height&&old.width==logical_width&&old.height==logical_height,
        "aspect-fit output/reference extent");
    const auto background=old.rgb(0,0),inside=old.rgb(right?280u:80u,128);
    require(!close_resize_rgb(background,inside),"resize reference lacks a distinct panel");
    // Independent analytic rectangle, not output of the production viewport
    // helper. Source integer centers map through (coordinate+0.5)*scale once.
    const double scale=std::min(double(width)/logical_width,double(height)/logical_height);
    const double x0=(width-logical_width*scale)/2,y0=(height-logical_height*scale)/2;
    const double left=x0+((right?208.0:16.0)+0.5)*scale,right_edge=left+150.0*scale;
    const double top=y0+((right?56.0:16.0)+0.5)*scale,bottom=y0+224.5*scale;
    constexpr double edge_tolerance=2.0; //Physical pixels, accommodates 1x/4x MSAA edge coverage.
    unsigned min_x=width,min_y=height,max_x=0,max_y=0,filled=0,interior_checks=0,background_checks=0;
    for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){
        const auto color=actual.rgb(x,y);const double px=x+0.5,py=y+0.5;
        if(!close_resize_rgb(color,background)){
            min_x=std::min(min_x,x);min_y=std::min(min_y,y);max_x=std::max(max_x,x);max_y=std::max(max_y,y);++filled;
        }
        if(px>left+edge_tolerance&&px<right_edge-edge_tolerance&&py>top+edge_tolerance&&py<bottom-edge_tolerance){
            require(close_resize_rgb(color,inside),"aspect-fit panel interior/gamma/texture mismatch");++interior_checks;
        }else if(px<left-edge_tolerance||px>right_edge+edge_tolerance||py<top-edge_tolerance||py>bottom+edge_tolerance){
            require(close_resize_rgb(color,background),"aspect-fit bars or background differ from reference gamma(0)");++background_checks;
        }
    }
    require(filled&&interior_checks&&background_checks,"resize pixel coverage checks were vacuous");
    require(std::abs(min_x+0.5-left)<=edge_tolerance&&std::abs(max_x+0.5-right_edge)<=edge_tolerance&&
        std::abs(min_y+0.5-top)<=edge_tolerance&&std::abs(max_y+0.5-bottom)<=edge_tolerance,
        "resized panel has wrong position/aspect or duplicate half-pixel shift");
    if(samples==1){
        // Single-sample selected edges are away from raster subpixel ties.
        // Exact top-left coverage catches shifts hidden by the MSAA tolerance.
        require(min_x==static_cast<unsigned>(std::ceil(left-0.5))&&min_y==static_cast<unsigned>(std::ceil(top-0.5))&&
            max_x==static_cast<unsigned>(std::ceil(right_edge-0.5)-1)&&max_y==static_cast<unsigned>(std::ceil(bottom-0.5)-1),
            "single-sample resize changed exact half-pixel edge coverage");
    }
}
template<class F>void require_resize_rejection(F action,const char* message){
    bool rejected=false;
    try{action();}catch(const FormatError& error){require(std::string_view(error.what())==message,"resize rejected at the wrong boundary");rejected=true;}
    require(rejected,"resize fixture did not reject invalid canvas");
}
template<class F>void require_projection_rejection(F action){
    require_resize_rejection(action,"polygon frame must be projected for the actual target size");
}
void resize_boundary_cases(HWND owner,RenderOptions options,RetailMaterialFileSystem& files,const std::filesystem::path& folder){
    std::filesystem::create_directories(folder);
    // A distinct hidden HWND prevents two simultaneous flip-model swap chains
    // from sharing a window. This is a separate platform fixture, no source call.
    Window window{CreateWindowExW(0,L"SGN-Surface-Pair-Receiving",L"Retained canvas receiving",WS_OVERLAPPEDWINDOW,
        0,0,384,256,owner,nullptr,GetModuleHandleW(nullptr),nullptr)};
    require(window.handle!=nullptr,"resize boundary receiving window");
    Renderer renderer(window.handle,options);renderer.resize(384,256);
    MaterialTransparencyStorage transparency;MaterialD3D12 backend(renderer,transparency,false,1);backend.reset_transparency_430690();
    auto world=std::make_unique<WorldMaterialLoadingState>();const auto roots=source_graphics_roots_from_pe();WorldMaterialLoadingContext loading;
    loading.source_graphics_roots=&roots;loading.maximum_dimension=256;load_world_materials({},*world,loading,files,backend);world->published_material_count=16;
    PolygonDeviceContext device(device_seed(backend.resource_primary_token(15)));PolygonSubmissionCapture capture(device,backend,world->names.materials);
    replace_chart(backend,0xfc00,0xfc00);auto a=capture_panel(capture,false,101);
    auto b=capture_panel(capture,true,102,512,320);auto ramp=identity_sdr_gamma();
    ramp[0]=8192;ramp[256]=4096;ramp[512]=12288; //Witness gamma(0) bars, not an absolute-black claim.
    renderer.draw_polygon_submission(*a,ramp);const auto reference_a=read_capture(renderer,folder/"logical-a-384x256.bmp");
    renderer.resize(512,320);renderer.draw_polygon_submission(*b,ramp);const auto reference_b=read_capture(renderer,folder/"logical-b-512x320.bmp");
    require(!renderer.polygon_surface_roles().active,"direct mismatch fixture accidentally entered surface mode");
    require_projection_rejection([&]{renderer.draw_polygon_submission(*a,ramp);});
    require(!renderer.polygon_surface_roles().active,"direct mismatch activated surface roles");
    healthy(renderer);
    // Actual capture with zero commands, distinct from absent front/back.
    capture.begin_source_frame(103,limits);auto empty=capture.seal_source_frame();
    require(empty&&empty->commands==0,"drawless actual capture was not empty");
    // A real resource replacement separates these two captured draw segments.
    capture.begin_source_frame(104,limits);require(device.begin_scene()==0,"mixed canvas BeginScene");panel(device,false);
    Image replacement;replacement.width=replacement.height=256;replacement.rgba.assign(256u*256u*4u,255);
    capture.upload_to_destination(15,replacement);panel(device,true,512,320);require(device.end_scene()==0,"mixed canvas EndScene");
    auto mixed=capture.seal_source_frame();require(mixed->segments.size()==2,"mixed extent requires two actual resource segments");
    capture.begin_source_frame(106,limits);require(device.begin_scene()==0,"same canvas BeginScene");panel(device,false);
    capture.upload_to_destination(15,replacement);panel(device,true);require(device.end_scene()==0,"same canvas EndScene");
    auto same_canvas=capture.seal_source_frame();require(same_canvas->segments.size()==2,"same canvas requires two actual resource segments");
    auto invalid=capture_panel(capture,false,105,0,256);
    const auto source=device.snapshot();const auto epoch=backend.resource_epoch();
    renderer.resize(500,500);const auto presents=renderer.stats().presents;
    const auto pair=renderer.restore_polygon_surfaces(a.get(),ramp,b.get(),ramp);
    require(pair.front_image&&pair.back_image&&pair.front_image!=pair.back_image&&renderer.stats().presents==presents,
        "different valid front/back canvases were not restored without Present");
    require(renderer.present_polygon_front(),"cross-canvas front A");
    require_resized_panel(read_capture(renderer,folder/"cross-canvas-a.bmp"),reference_a,500,500,384,256,false,renderer.stats().samples);
    (void)renderer.flip_polygon_surfaces();require(renderer.present_polygon_front(),"cross-canvas front B");
    require_resized_panel(read_capture(renderer,folder/"cross-canvas-b.bmp"),reference_b,500,500,512,320,true,renderer.stats().samples);
    renderer.resize(501,501);const auto updates=renderer.stats().live_updates,uploads=renderer.stats().uploaded_texture_bytes;
    const auto before_rejection_presents=renderer.stats().presents;
    require_resize_rejection([&]{(void)renderer.restore_polygon_surfaces(a.get(),ramp,mixed.get(),ramp);},
        "retained submission mixes logical canvas extents");
    require_resize_rejection([&]{(void)renderer.restore_polygon_surfaces(a.get(),ramp,invalid.get(),ramp);},
        "retained submission has invalid logical canvas");
    require(!renderer.polygon_surface_roles().front_image&&!renderer.polygon_surface_roles().back_image&&
        renderer.stats().live_updates==updates&&renderer.stats().uploaded_texture_bytes==uploads&&renderer.stats().presents==before_rejection_presents,
        "invalid second canvas produced first GPU image or changed presentation");
    (void)renderer.restore_polygon_surfaces(same_canvas.get(),ramp,nullptr,ramp);
    require(renderer.present_polygon_front(),"same-canvas multi-segment recipe was rejected");
    const auto same_pixels=read_capture(renderer,folder/"same-canvas-two-segments.bmp");const ResizeBitmap same_view(same_pixels);
    require(close_resize_rgb(same_view.rgb(104,250),{255,255,255})&&close_resize_rgb(same_view.rgb(365,250),{255,255,255})&&
        close_resize_rgb(same_view.rgb(0,0),ResizeBitmap(reference_a).rgb(0,0)),"same-canvas segments or gamma background were lost");
    renderer.resize(504,504);const auto only_back=renderer.restore_polygon_surfaces(nullptr,ramp,a.get(),ramp);
    require(!only_back.front_image&&only_back.back_image&&!renderer.present_polygon_front(),"absent front became a clear image");
    (void)renderer.flip_polygon_surfaces();require(renderer.present_polygon_front(),"restored back did not become front");
    require_resized_panel(read_capture(renderer,folder/"only-back-a.bmp"),reference_a,504,504,384,256,false,renderer.stats().samples);
    renderer.resize(502,502);const auto drawless=renderer.restore_polygon_surfaces(empty.get(),ramp,nullptr,ramp);
    require(drawless.front_image&&!drawless.back_image&&renderer.present_polygon_front(),"drawless recipe did not produce its clear image");
    const auto blank=read_capture(renderer,folder/"drawless-gamma-clear.bmp");const ResizeBitmap blank_view(blank),reference_view(reference_a);
    const auto clear=reference_view.rgb(0,0);require(clear[0]>0&&clear[1]>0&&clear[2]>0,"nonzero gamma(0) fixture is ineffective");
    for(unsigned y=0;y<blank_view.height;++y)for(unsigned x=0;x<blank_view.width;++x)
        require(close_resize_rgb(blank_view.rgb(x,y),clear),"drawless restore inferred a canvas or lost captured clear gamma");
    renderer.resize(503,503);const auto before_empty_updates=renderer.stats().live_updates,before_empty_presents=renderer.stats().presents;
    const auto absent=renderer.restore_polygon_surfaces(nullptr,ramp,nullptr,ramp);
    require(!absent.front_image&&!absent.back_image&&!renderer.present_polygon_front()&&renderer.stats().live_updates==before_empty_updates&&
        renderer.stats().presents==before_empty_presents,"absent recipes manufactured GPU images");
    require(same_polygon_snapshot(source,device.snapshot())&&epoch==backend.resource_epoch()&&!capture.capturing()&&!capture.poisoned(),
        "canvas restoration changed source/capture/material owners");
    healthy(renderer);
}
}
