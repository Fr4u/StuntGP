#include "sgn/renderer.hpp"
#include <dxgi1_6.h>
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>

using namespace sgn;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct Window {HWND handle{};~Window(){if(handle)DestroyWindow(handle);}};
PolygonDeviceState seed(){ //New platform fixture, not recovered D3D3 defaults.
    PolygonDeviceState s;
    for(auto [i,v]:std::initializer_list<std::pair<unsigned,unsigned>>{{4,1},{7,1},{8,3},{9,2},{14,1},{15,1},{19,2},{20,1},{22,1},{23,7},{24,1},{25,7},{27,0},{28,0},{29,0}})s.render[i]=v;
    for(unsigned stage=0;stage<2;++stage)
        for(auto [i,v]:std::initializer_list<std::pair<unsigned,unsigned>>{{1,stage?1u:4u},{2,2},{3,0},{4,4},{5,2},{6,0},{11,stage},{13,3},{14,3},{16,1},{17,1},{18,0}})s.stage[stage][i]=v;
    s.render[41]=0;s.texture[0]=101;s.texture[1]=0;return s;
}
retail::PolygonSubmissionVertex vertex(float x,float y,float u,float v){
    return {{{std::bit_cast<std::uint32_t>(x),std::bit_cast<std::uint32_t>(y),0x3f000000,0x3f800000,
        0xffffffff,0,std::bit_cast<std::uint32_t>(u),std::bit_cast<std::uint32_t>(v),0x7fc23456,0xdeadbeef}}};
}
CapturedPolygonFrame frame(unsigned width,unsigned height){
    CapturedPolygonDraw draw;draw.fvf=0x2c4;draw.flags=0x1c;draw.type=4;draw.indexed=true;
    draw.viewport.full_width=width;draw.viewport.full_height=height;
    const float x=static_cast<float>(width)/8,y=static_cast<float>(height)/8;
    draw.vertices={vertex(x,y,0,0),vertex(7*x,y,1,0),vertex(7*x,7*y,1,1),vertex(x,7*y,0,1)};
    draw.indices={0,1,2,0,2,3};CapturedPolygonFrame result;
    result.source_tick=73;result.resource_epoch=11;result.commands.push_back(std::move(draw));return result;
}
void healthy(Renderer& renderer){
    renderer.finish();renderer.collect_diagnostics();renderer.check_health();
    if(!renderer.diagnostics().empty()){
        for(const auto& diagnostic:renderer.diagnostics())std::cerr<<diagnostic.id<<": "<<diagnostic.message<<'\n';
        throw std::runtime_error("window GPU debug warning/error");
    }
    require(renderer.stats().debug_active&&renderer.stats().debug_errors==0,"window debug layer was not clean and active");
}
constexpr std::array<std::uint16_t,4> pixels{0xfc00,0x81e0,0x8007,0xffff};
std::shared_ptr<PolygonGpuTextures> textures(Renderer& renderer){
    auto bank=std::make_shared<PolygonGpuTextures>();bank->epoch=11;
    bank->entries.push_back(renderer.upload_polygon_texture(101,2,2,pixels));return bank;
}
std::shared_ptr<PolygonGpuTextures> foreign_texture(bool primary_warp){
    using Microsoft::WRL::ComPtr;
    const auto check=[](HRESULT result){require(SUCCEEDED(result),"foreign-device fixture creation failed");};
    ComPtr<IDXGIFactory6> factory;check(CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG,IID_PPV_ARGS(&factory)));
    ComPtr<IDXGIAdapter1> adapter;
    if(!primary_warp)check(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter)));
    else for(unsigned index=0;;++index){ComPtr<IDXGIAdapter1> candidate;
        const auto result=factory->EnumAdapterByGpuPreference(index,DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,IID_PPV_ARGS(&candidate));
        if(result==DXGI_ERROR_NOT_FOUND)break;check(result);DXGI_ADAPTER_DESC1 description{};check(candidate->GetDesc1(&description));
        if(!(description.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)&&SUCCEEDED(D3D12CreateDevice(candidate.Get(),D3D_FEATURE_LEVEL_11_0,__uuidof(ID3D12Device),nullptr))){adapter=candidate;break;}}
    require(adapter!=nullptr,"foreign-device hardware adapter absent");ComPtr<ID3D12Device> device;
    check(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device)));
    D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_DEFAULT;heap.CreationNodeMask=heap.VisibleNodeMask=1;
    D3D12_RESOURCE_DESC description{};description.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;description.Width=description.Height=2;
    description.DepthOrArraySize=description.MipLevels=1;description.Format=DXGI_FORMAT_B5G5R5A1_UNORM;description.SampleDesc.Count=1;
    PolygonGpuTexture texture;texture.token=101;texture.format=description.Format;
    check(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&description,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,nullptr,IID_PPV_ARGS(&texture.resource)));
    auto bank=std::make_shared<PolygonGpuTextures>();bank->epoch=11;bank->entries.push_back(std::move(texture));return bank;
}
std::size_t check_pixels(const Bytes& bytes,unsigned width,unsigned height,std::ofstream& csv,const std::string& label){
    BITMAPFILEHEADER file{};BITMAPINFOHEADER info{};
    require(bytes.size()>=sizeof(file)+sizeof(info),"truncated window capture header");
    std::memcpy(&file,bytes.data(),sizeof(file));std::memcpy(&info,bytes.data()+sizeof(file),sizeof(info));
    require(file.bfType==0x4d42&&file.bfOffBits==54&&info.biWidth==static_cast<LONG>(width)&&info.biHeight==-static_cast<LONG>(height)&&info.biBitCount==32,"capture dimensions or orientation mismatch");
    require(bytes.size()==54+std::size_t(width)*height*4,"capture byte count mismatch");
    struct Sample {unsigned x,y;std::array<unsigned,3> expected;};
    const std::array<Sample,5> samples{{{width/4,height/4,{255,0,0}},
        {3*width/4,height/4,{0,123,0}},{width/4,3*height/4,{0,0,58}},
        {3*width/4,3*height/4,{255,255,255}},{2,2,{0,0,0}}}};
    for(const auto& sample:samples){
        const auto offset=54+(std::size_t(sample.y)*width+sample.x)*4;
        const std::array<unsigned,3> actual{bytes[offset+2],bytes[offset+1],bytes[offset]};
        csv<<label<<','<<sample.x<<','<<sample.y;
        for(auto value:sample.expected)csv<<','<<value;for(auto value:actual)csv<<','<<value;csv<<'\n';csv.flush();
        for(unsigned channel=0;channel<3;++channel)
            require(std::abs(static_cast<int>(actual[channel])-static_cast<int>(sample.expected[channel]))<=1,"window pixel differs from packed texture/capture orientation");
    }
    return samples.size()*3;
}
}

int main(int argc,char** argv){try{
    require(argc==3,"expected --warp|--hardware and fresh output directory");
    const bool warp=std::string_view(argv[1])=="--warp";
    require(warp||std::string_view(argv[1])=="--hardware","unknown adapter");
    const auto output=std::filesystem::absolute(argv[2]);
    require(!std::filesystem::exists(output),"window evidence output already exists");std::filesystem::create_directories(output);
    WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"SGN-Polygon-Window-Probe";
    require(RegisterClassW(&wc)!=0,"window class registration failed");
    Window window{CreateWindowExW(0,wc.lpszClassName,L"StuntGP NEW polygon window qualification",WS_OVERLAPPEDWINDOW,
        0,0,640,480,nullptr,nullptr,wc.hInstance,nullptr)};
    require(window.handle!=nullptr,"hidden probe window creation failed");
    auto renderer=std::make_unique<Renderer>(window.handle,RenderOptions{true,warp,false,false,true});
    auto bank=textures(*renderer);auto state=seed();
    std::ofstream samples(output/"samples.csv");require(bool(samples),"cannot write window sample evidence");
    samples<<"case,x,y,expected_r,expected_g,expected_b,actual_r,actual_g,actual_b\n";
    const std::array<std::array<unsigned,2>,7> sizes{{{640,480},{720,1280},{1920,1080},{3440,1440},{3840,2160},{7680,2160},{640,480}}};
    std::size_t channel_checks=0,presented=0,replays=0;unsigned rejected=0,injected_failures=0,capture_rejections=0;
    for(std::size_t size_index=0;size_index<sizes.size();++size_index){
        const auto [width,height]=sizes[size_index];renderer->resize(width,height);
        require(renderer->stats().width==width&&renderer->stats().height==height,"resize dimensions not applied");
        const auto capture=frame(width,height);const auto saved=std::get<CapturedPolygonDraw>(capture.commands[0]).vertices;
        renderer->vsync(false);for(unsigned repeat=0;repeat<6;++repeat){renderer->draw_polygons(capture,state,bank);++presented;}
        const auto name=std::to_string(size_index)+"-"+std::to_string(width)+"x"+std::to_string(height);
        const auto first_file=output/(name+"-sync0.bmp");renderer->capture(first_file);const auto first=read_file(first_file);
        channel_checks+=check_pixels(first,width,height,samples,name+"-sync0");
        renderer->vsync(true);for(unsigned repeat=0;repeat<4;++repeat){renderer->draw_polygons(capture,state,bank);++presented;}
        const auto second_file=output/(name+"-sync1.bmp");renderer->capture(second_file);const auto second=read_file(second_file);
        require(first==second,"changing presentation synchronization changed immutable frame pixels");++replays;
        const auto& retained=std::get<CapturedPolygonDraw>(capture.commands[0]).vertices;
        require(capture.source_tick==73&&capture.resource_epoch==11&&saved.size()==retained.size()&&std::memcmp(saved.data(),retained.data(),saved.size()*sizeof(saved[0]))==0,"presentation mutated source frame");
        require(std::isfinite(renderer->stats().gpu_ms_last)&&renderer->stats().gpu_ms_last>0,"GPU timestamp not reported after frame reuse");
        healthy(*renderer);
    }
    auto stable=frame(640,480);
    // Rejected inputs must leave the command list reusable and never present.
    for(unsigned test=0;test<6;++test){auto invalid=stable;auto invalid_state=state;
        if(test==0)invalid.resource_epoch=12;
        if(test==1)std::get<CapturedPolygonDraw>(invalid.commands[0]).viewport.full_width=639;
        if(test==2)std::get<CapturedPolygonDraw>(invalid.commands[0]).indices[0]=99;
        if(test==3)invalid_state.texture[0]=999;
        if(test==4)invalid_state.render[28]=1;
        if(test==5)invalid_state.render[41]=1;
        const auto before=renderer->stats().presents;
        try{renderer->draw_polygons(invalid,invalid_state,bank);}catch(const FormatError&){++rejected;}
        require(renderer->stats().presents==before,"rejected frame was presented");
        renderer->draw_polygons(stable,state,bank);++presented;
    }
    try{(void)renderer->upload_polygon_texture(102,3,3,pixels);}catch(const FormatError&){++rejected;}
    require(rejected==7,"invalid window frame/upload accepted");
    const auto reject_capture=[&]{
        bool failed=false;try{renderer->capture(output/"unexpected-capture.bmp");}catch(const std::runtime_error&){failed=true;}
        require(failed&&!std::filesystem::exists(output/"unexpected-capture.bmp"),"capture used unavailable targets/frame");++capture_rejections;
    };
    const auto inspect_frame=[&](unsigned width,unsigned height,const std::string& label){
        renderer->draw_polygons(frame(width,height),state,bank);++presented;
        const auto path=output/(label+".bmp");renderer->capture(path);
        channel_checks+=check_pixels(read_file(path),width,height,samples,label);healthy(*renderer);
    };
    const auto initial_presents=renderer->stats().presents;
    try{renderer->resize(D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION+1,480);}catch(const FormatError&){++injected_failures;}
    require(injected_failures==1&&renderer->stats().width==640&&renderer->stats().presents==initial_presents,"oversized resize changed live targets");
    inspect_frame(640,480,"oversized-resize-rejected");
    renderer->inject_failure_for_test(RenderFailurePoint::resize_buffers);
    try{renderer->resize(800,600);}catch(const GraphicsError& error){require(error.result==E_OUTOFMEMORY,"unexpected resize injection error");++injected_failures;}
    require(injected_failures==2&&renderer->stats().width==640&&renderer->stats().height==480&&renderer->stats().presents==presented,"failed resize lost old dimensions or presented");
    reject_capture();renderer->resize(640,480);inspect_frame(640,480,"failed-resize-recovered");
    renderer->inject_failure_for_test(RenderFailurePoint::target_allocation);
    try{renderer->resize(800,600);}catch(const GraphicsError& error){require(error.result==E_OUTOFMEMORY,"unexpected target injection error");++injected_failures;}
    require(injected_failures==3&&renderer->stats().width==800&&renderer->stats().height==600&&renderer->stats().presents==presented,"failed target allocation changed accepted swap-chain dimensions or presented");
    bool unready=false;try{renderer->draw_polygons(frame(800,600),state,bank);}catch(const FormatError&){unready=true;}
    require(unready&&renderer->stats().presents==presented,"draw used partially created targets");reject_capture();
    renderer->resize(800,600);inspect_frame(800,600,"target-allocation-recovered");
    renderer->resize(864,640);renderer->inject_failure_for_test(RenderFailurePoint::polygon_target_allocation);
    try{renderer->draw_polygons(frame(864,640),state,bank);}catch(const GraphicsError& error){require(error.result==E_OUTOFMEMORY,"unexpected polygon target injection error");++injected_failures;}
    require(injected_failures==4&&renderer->stats().presents==presented,"failed polygon target allocation presented");reject_capture();
    inspect_frame(864,640,"polygon-target-allocation-recovered");renderer->resize(640,480);
    renderer->inject_failure_for_test(RenderFailurePoint::polygon_upload_wait);
    try{(void)renderer->upload_polygon_texture(103,2,2,pixels);}catch(const std::runtime_error&){++injected_failures;}
    require(injected_failures==5&&renderer->stats().pending_polygon_uploads==1&&renderer->stats().presents==presented,"failed upload wait released queued resources or presented");
    healthy(*renderer);require(renderer->stats().pending_polygon_uploads==0,"completed upload was not retired");
    bank=textures(*renderer);inspect_frame(640,480,"upload-wait-recovered");
    auto doomed=textures(*renderer);std::weak_ptr<PolygonGpuTextures> weak=doomed;
    unsigned lifetime_hooks=0;
    renderer->before_polygon_submit_for_test([&]{doomed->entries.clear();doomed.reset();require(weak.expired(),"bank metadata retained before execution");++lifetime_hooks;});
    renderer->draw_polygons(stable,state,doomed);++presented;
    require(lifetime_hooks==1&&weak.expired(),"renderer did not release the bank before queue execution");
    const auto lifetime_file=output/"bank-lifetime.bmp";renderer->capture(lifetime_file);
    channel_checks+=check_pixels(read_file(lifetime_file),640,480,samples,"bank-lifetime");
    // Zero-sized WM_SIZE/minimization requests preserve the last usable targets.
    renderer->resize(0,0);require(renderer->stats().width==640&&renderer->stats().height==480,"zero resize destroyed usable targets");
    renderer->draw_polygons(stable,state,bank);++presented;healthy(*renderer);
    {
        auto foreign=foreign_texture(warp);const auto before=renderer->stats().presents;
        try{renderer->draw_polygons(stable,state,foreign);}catch(const FormatError&){++rejected;}
        require(rejected==8&&renderer->stats().presents==before,"foreign-device texture was accepted");
        renderer->draw_polygons(stable,state,bank);++presented;healthy(*renderer);
    }
    const auto old_stats=renderer->stats();require(old_stats.presents==presented,"presentation counter mismatch");
    renderer->remove_device_for_test(true);bool removal=false;
    try{renderer->draw_polygons(stable,state,bank);}catch(const GraphicsError& error){removal=error.device_lost();}
    require(removal,"removed device was not reported by polygon path");
    renderer.reset();bank.reset();
    // Device recreation requires releasing every old GPU resource. D3D12
    // devices are per-adapter singletons: retaining one removed-device texture
    // prevents creating a replacement device. Only the CPU capture survives.
    renderer=std::make_unique<Renderer>(window.handle,RenderOptions{true,warp,false,false,true});renderer->resize(640,480);
    require(renderer->stats().presents==0,"replacement device presentation counter was not reset");
    bank=textures(*renderer);renderer->draw_polygons(stable,state,bank);++presented;
    const auto restored_file=output/"device-recreated.bmp";renderer->capture(restored_file);
    channel_checks+=check_pixels(read_file(restored_file),640,480,samples,"device-recreated");healthy(*renderer);
    std::cout<<"passed=1 adapter="<<(warp?"WARP":"hardware")<<" samples="<<old_stats.samples
        <<" resize_cases="<<sizes.size()<<" presents="<<presented<<" equal_replays="<<replays
        <<" channel_checks="<<channel_checks<<" rejected="<<rejected<<" injected_failures="<<injected_failures
        <<" capture_rejections="<<capture_rejections<<" before_execute_owner_destructions="<<lifetime_hooks
        <<" recreated_devices=1 debug_warnings_errors=0\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
