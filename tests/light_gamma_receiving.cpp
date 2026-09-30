// Future real GPU qualification. Analytic platform color chart, not game art.
#include "sgn/light_gamma_d3d12.hpp"
#include "sgn/material_d3d12.hpp"
#include "sgn/polygon_submission_capture.hpp"
#include "sgn/retail_material_filesystem.hpp"
#include <algorithm>
#include <bit>
#include <cstring>
#include <fstream>
#include <iostream>

using namespace sgn;using namespace sgn::retail;
namespace {
void require(bool value,const char* message){if(!value)throw FormatError(message);}
struct Window {HWND handle{};~Window(){if(handle)DestroyWindow(handle);}};
void healthy(Renderer& renderer){
    renderer.finish();renderer.collect_diagnostics();renderer.check_health();
    for(const auto& diagnostic:renderer.diagnostics())std::cerr<<diagnostic.message<<'\n';
    require(renderer.stats().debug_active&&renderer.stats().debug_errors==0,"gamma GPU debug warning/error");
}
PolygonDeviceState seed(std::uint32_t token){
    // Explicit platform fixture; source states and game art are not inferred.
    PolygonDeviceState state;
    for(auto [i,v]:std::initializer_list<std::pair<unsigned,unsigned>>{{4,1},{7,1},{8,3},{9,2},{14,1},{15,1},{19,2},{20,1},{22,1},{23,7},{24,1},{25,7},{27,0},{28,0},{29,0}})state.render[i]=v;
    for(unsigned stage=0;stage<2;++stage)
        for(auto [i,v]:std::initializer_list<std::pair<unsigned,unsigned>>{{1,stage?1u:3u},{2,2},{3,0},{4,3},{5,2},{6,0},{11,stage},{13,3},{14,3},{16,1},{17,1},{18,0}})state.stage[stage][i]=v;
    state.render[41]=0;state.texture[0]=token;state.texture[1]=0;return state;
}
void chart(PolygonDeviceContext& device,unsigned width,unsigned height){
    ViewportState viewport;viewport.full_width=width;viewport.full_height=height;
    for(unsigned i=0;i<256;++i){
        const auto color=0xff000000u|(i<<16)|(((i*73u)%256u)<<8)|(255u-i);
        const float left=static_cast<float>(i)*static_cast<float>(width)/256.0f;
        const float right=static_cast<float>(i+1)*static_cast<float>(width)/256.0f;
        std::array<PolygonSubmissionVertex,4> vertices{};unsigned index=0;
        for(const auto [x,y]:std::array<std::array<float,2>,4>{{{left,16},{right,16},{right,static_cast<float>(height)-16},{left,static_cast<float>(height)-16}}})
            vertices[index++].words={std::bit_cast<std::uint32_t>(x),std::bit_cast<std::uint32_t>(y),0x3f000000,0x3f800000,color,0,0,0,0,0};
        require(device.draw(6,0x2c4,vertices,0x1c,viewport)==0,"gamma chart capture");
    }
}
constexpr PolygonSubmissionCaptureLimits limits{{{300,256u*1024u},600},2};
ImmutablePolygonSubmission capture_chart(PolygonSubmissionCapture& capture,unsigned width,unsigned height,std::uint64_t tick){
    capture.begin_source_frame(tick,limits);
    try{require(capture.device().begin_scene()==0,"gamma scene begin");chart(capture.device(),width,height);
        require(capture.device().end_scene()==0,"gamma scene end");return capture.seal_source_frame();
    }catch(...){capture.discard_failed_source_frame();throw;}
}
Bytes draw(Renderer& renderer,const ImmutablePolygonSubmission& displayed,const SdrGammaRamp* ramp,const std::filesystem::path& path){
    if(ramp)renderer.draw_polygon_submission(*displayed,*ramp);else renderer.draw_polygon_submission(*displayed);
    renderer.capture(path);healthy(renderer);return read_file(path);
}
void write_ramp(const std::filesystem::path& path,const SdrGammaRamp& ramp){
    std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(ramp.data()),sizeof(ramp));
    require(bool(out),"gamma ramp evidence output");
}
std::size_t compare(const Bytes& before,const Bytes& after,const SdrGammaRamp& ramp,unsigned width,unsigned height){
    BITMAPFILEHEADER header{};BITMAPINFOHEADER info{};
    require(before.size()==after.size()&&before.size()==54+std::size_t(width)*height*4,"gamma BMP extent");
    std::memcpy(&header,before.data(),sizeof(header));std::memcpy(&info,before.data()+sizeof(header),sizeof(info));
    require(header.bfOffBits==54&&info.biWidth==static_cast<LONG>(width)&&info.biHeight==-static_cast<LONG>(height)&&info.biBitCount==32,"gamma BMP layout");
    require(std::equal(before.begin(),before.begin()+54,after.begin()),"gamma changed BMP metadata");
    std::size_t changed=0;
    for(std::size_t pixel=54;pixel<before.size();pixel+=4)for(unsigned channel=0;channel<3;++channel){
        const auto offset=pixel+2-channel;
        const auto expected=(std::uint32_t(ramp[channel*256+before[offset]])*255u+32767u)/65535u;
        require(std::abs(int(after[offset])-int(expected))<=1,"actual gamma RGB pixel differs from independent integer lookup");
        if(after[offset]!=before[offset])++changed;
    }
    return changed; //capture() forces BMP alpha255; this receiver qualifies RGB only.
}
void require_palette(const Bytes& baseline){
    std::array<std::array<bool,256>,3> observed{};
    for(std::size_t at=54;at<baseline.size();at+=4)for(unsigned channel=0;channel<3;++channel)observed[channel][baseline[at+2-channel]]=true;
    for(const auto& channel:observed)require(std::all_of(channel.begin(),channel.end(),[](bool value){return value;}),"gamma baseline must exercise all256 inputs per channel");
}
}
int main(int argc,char** argv)try{
    require(argc==4,"expected --hardware|--warp retail-root fresh-output");
    const bool warp=std::string_view(argv[1])=="--warp";require(warp||std::string_view(argv[1])=="--hardware","gamma adapter argument");
    const auto output=std::filesystem::absolute(argv[3]);require(!std::filesystem::exists(output),"gamma output must be fresh");
    std::filesystem::create_directories(output);AssetStore assets(argv[2]);RetailMaterialFileSystem files(assets,argv[2]);
    WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"SGN-Gamma-Receiving";
    require(RegisterClassW(&wc)!=0,"gamma window class");
    Window window{CreateWindowExW(0,wc.lpszClassName,L"StuntGP NEW gamma receiving",WS_OVERLAPPEDWINDOW,0,0,512,192,nullptr,nullptr,wc.hInstance,nullptr)};
    require(window.handle!=nullptr,"gamma window");
    std::ofstream results(output/"cases.csv");require(bool(results),"gamma cases output");
    results<<"case,requested_samples,actual_samples,changed_rgb_channels,apply_count,final_renderer_gamma_presentations\n";
    unsigned cases=0;
    for(unsigned requested:{1u,4u})for(bool split:{false,true}){
        const auto label=std::to_string(requested)+(split?"-split":"-alias");const auto folder=output/label;std::filesystem::create_directories(folder);
        RenderOptions options{true,warp,false,false,true};options.requested_samples=requested;
        auto renderer=std::make_unique<Renderer>(window.handle,options);renderer->resize(512,192);
        const auto actual_samples=renderer->stats().samples;
        MaterialTransparencyStorage transparency;MaterialD3D12 backend(*renderer,transparency,split,1);backend.reset_transparency_430690();
        auto world=std::make_unique<WorldMaterialLoadingState>();const auto roots=source_graphics_roots_from_pe();WorldMaterialLoadingContext loading;
        loading.source_graphics_roots=&roots;loading.maximum_dimension=256;load_world_materials({},*world,loading,files,backend);world->published_material_count=16;
        require(world->names.materials.size()==16&&backend.resource_entries()==16,"gamma actual normal material prelude");
        PolygonDeviceContext device(seed(backend.resource_primary_token(15)));PolygonSubmissionCapture capture(device,backend,world->names.materials);
        LightGammaD3D12 gamma;LightFadeState source;const auto untouched=source;
        ImmutablePolygonSubmission displayed;auto displayed_ramp=identity_sdr_gamma();
        gamma.publish(displayed,displayed_ramp,capture_chart(capture,512,192,1));
        const auto epoch=backend.resource_epoch();
        const auto baseline=draw(*renderer,displayed,nullptr,folder/"baseline.bmp");require_palette(baseline);
        const auto identity=draw(*renderer,displayed,&displayed_ramp,folder/"identity.bmp");
        require(baseline==identity&&renderer->stats().gamma_presentations==1&&gamma.apply_count()==0&&source.last_gamma==untouched.last_gamma&&source.ramp==untouched.ramp,"identity path altered pixels, bypassed gamma or called source");
        // Real unchanged4320C0, concrete receiver, real GPU mapping of its words.
        set_light_gamma(source,3.0f,&gamma);require(gamma.apply_count()==1&&gamma.snapshot()==source.ramp&&source.last_gamma==3.0f,"source gamma publication mismatch");
        const auto source_words=source.ramp;set_light_gamma(source,3.005f,&gamma);
        require(gamma.apply_count()==1&&source.ramp==source_words&&source.last_gamma==3.0f,"source epsilon guard changed");
        gamma.publish(displayed,displayed_ramp,capture_chart(capture,512,192,2));write_ramp(folder/"source-ramp.u16",displayed_ramp);
        const auto corrected=draw(*renderer,displayed,&displayed_ramp,folder/"source-ramp.bmp");
        require(compare(baseline,corrected,displayed_ramp,512,192)>10000,"source gamma pass had no meaningful image effect");
        // Independent, deliberately unequal/nonlinear channel ramps distinguish
        // RGB ordering, every input index and incorrect lookup filtering.
        auto channel_ramp=identity_sdr_gamma();
        for(unsigned i=0;i<256;++i){channel_ramp[i]=static_cast<std::uint16_t>((255u-i)*257u);
            channel_ramp[256+i]=static_cast<std::uint16_t>((i&1u)?65535u:0u);
            channel_ramp[512+i]=static_cast<std::uint16_t>((i*i*65535u+32512u)/65025u);}
        gamma.apply(channel_ramp);gamma.publish(displayed,displayed_ramp,capture_chart(capture,512,192,3));write_ramp(folder/"channel-ramp.u16",displayed_ramp);
        const auto channels=draw(*renderer,displayed,&displayed_ramp,folder/"channels.bmp");
        const auto changed=compare(baseline,channels,displayed_ramp,512,192);require(changed>10000,"channel ramp witness weak");
        const auto retained_ramp=displayed_ramp;const auto calls=gamma.apply_count();const auto cpu=device.snapshot();
        for(unsigned replay=0;replay<8;++replay){renderer->vsync(replay%2==0);renderer->draw_polygon_submission(*displayed,displayed_ramp);}
        renderer->capture(folder/"replay.bmp");healthy(*renderer);
        require(read_file(folder/"replay.bmp")==channels&&gamma.apply_count()==calls&&source.ramp==source_words&&source.last_gamma==3.0f&&same_polygon_snapshot(cpu,device.snapshot())&&backend.resource_epoch()==epoch,"replay reran source or changed frame/ramp");
        // The command list must not retain the borrowed ramp. Change that
        // caller-owned value after Close, before Execute, without renderer reentry.
        auto invocation_ramp=retained_ramp;
        renderer->before_polygon_submit_for_test([&]{invocation_ramp=identity_sdr_gamma();});
        renderer->draw_polygon_submission(*displayed,invocation_ramp);renderer->capture(folder/"borrow-retired.bmp");healthy(*renderer);
        require(read_file(folder/"borrow-retired.bmp")==channels&&invocation_ramp!=retained_ramp&&gamma.apply_count()==calls,"GPU retained or reread caller ramp after preparation");
        // Later source changes gamma, then produces no displayable submission.
        set_light_gamma(source,4.0f,&gamma);const auto newer_ramp=gamma.snapshot();
        require(newer_ramp!=retained_ramp,"newer source gamma must differ from displayed gamma");
        gamma.publish(displayed,displayed_ramp,{});
        require(displayed_ramp==retained_ramp,"null source result published newer gamma");
        require(draw(*renderer,displayed,&displayed_ramp,folder/"null-result.bmp")==channels,"null result changed old displayed pixels");
        // A queued pass with a successful retirement fence, but failed Present,
        // retains its frame resources and can be followed by a healthy draw.
        const auto before_failed_present=renderer->stats().presents;
        renderer->inject_failure_for_test(RenderFailurePoint::polygon_frame_present);bool present_failed=false;
        try{renderer->draw_polygon_submission(*displayed,displayed_ramp);}catch(const GraphicsError& error){require(error.result==E_FAIL,"unexpected gamma Present failure");present_failed=true;}
        require(present_failed&&renderer->stats().presents==before_failed_present,"gamma Present injection did not fail");
        healthy(*renderer);require(draw(*renderer,displayed,&displayed_ramp,folder/"present-retry.bmp")==channels,"gamma Present retry lost frame resources");
        // A queued pass without a retirement fence must keep its GPU owners
        // until the subsequent explicit device removal/renderer destruction.
        renderer->inject_failure_for_test(RenderFailurePoint::polygon_frame_signal);bool signal_failed=false;
        try{renderer->draw_polygon_submission(*displayed,displayed_ramp);}catch(const GraphicsError& error){require(error.result==E_FAIL,"unexpected gamma Signal failure");signal_failed=true;}
        require(signal_failed,"gamma signal injection did not fail");
        // CPU-only gamma pair survives real removal and material rebind retry.
        const auto before_rebind_calls=gamma.apply_count();renderer->remove_device_for_test(true);
        capture.suspend_renderer_for_recreation(displayed);require(!displayed,"rebind still owns old GPU display");renderer.reset();
        renderer=std::make_unique<Renderer>(window.handle,options);renderer->resize(512,192);
        require(renderer->stats().samples==actual_samples,"recreated sample count differs from recorded sample count");
        renderer->inject_failure_for_test(RenderFailurePoint::polygon_upload_wait);bool failed=false;
        try{capture.rebind_renderer(*renderer,displayed);}catch(const std::exception&){failed=true;}
        require(failed&&!displayed&&renderer->stats().pending_polygon_uploads==1&&displayed_ramp==retained_ramp&&gamma.snapshot()==newer_ramp,"failed rebind changed gamma owners");
        healthy(*renderer);capture.rebind_renderer(*renderer,displayed);
        require(draw(*renderer,displayed,&displayed_ramp,folder/"recreated.bmp")==channels&&gamma.apply_count()==before_rebind_calls&&gamma.snapshot()==newer_ramp&&same_polygon_snapshot(cpu,device.snapshot()),"recreated renderer used current gamma or reran source");
        // Resize retires frame-local input copies/CBs. Source gamma stays live,
        // and the next real capture commits it with a new displayable frame.
        renderer->resize(768,240);gamma.publish(displayed,displayed_ramp,capture_chart(capture,768,240,4));
        require(displayed_ramp==newer_ramp,"new source frame did not commit current gamma");
        write_ramp(folder/"resized-ramp.u16",displayed_ramp);
        const auto resized_base=draw(*renderer,displayed,nullptr,folder/"resized-baseline.bmp");
        const auto resized=draw(*renderer,displayed,&displayed_ramp,folder/"resized-gamma.bmp");
        require(compare(resized_base,resized,displayed_ramp,768,240)>10000,"resized gamma pass did not map image");
        // Actual source capture failure after source gamma publication. This
        // invokes the SAME publish only if seal succeeds; the real seal refuses.
        bool source_failed=false,stage_refused=false;
        try{capture.begin_source_frame(5,limits);require(device.begin_scene()==0,"failed-source begin");
            set_light_gamma(source,2.0f,&gamma);stage_refused=device.set_texture_stage_state(2,1,3)!=0;
            (void)device.end_scene();gamma.publish(displayed,displayed_ramp,capture.seal_source_frame());
        }catch(const std::exception&){source_failed=true;capture.discard_failed_source_frame();}
        require(source_failed&&stage_refused&&capture.poisoned()&&displayed_ramp==newer_ramp&&gamma.snapshot()==source.ramp&&source.last_gamma==2.0f,"failed source rolled back gamma or committed display");
        require(draw(*renderer,displayed,&displayed_ramp,folder/"failed-source.bmp")==resized,"failed source changed displayed gamma");
        results<<label<<','<<requested<<','<<actual_samples<<','<<changed<<','<<gamma.apply_count()<<','<<renderer->stats().gamma_presentations<<'\n';
        results.flush();require(bool(results),"gamma results flush");++cases;
    }
    require(cases==4,"gamma alias/split sample matrix");std::cout<<"passed=1 gamma_cases=4 actual_application_execution=0 hdr_qualified=0\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
