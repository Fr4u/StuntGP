#include "sgn/race_overlay_textures.hpp"
#include <iostream>
using namespace sgn;using namespace sgn::retail;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct Window{HWND handle{};~Window(){if(handle)DestroyWindow(handle);}};
struct Device final:TextureReplacementDevice{
    std::uint16_t width{256};unsigned uploads{};std::uint32_t observed_selection{};RaceOverlayTextures* bank{};bool fail{};
    std::uint16_t destination_width(std::uint32_t) const override{return width;}
    void upload_to_destination(std::uint32_t,const Image&) override{++uploads;observed_selection=bank->selected_overlay_texture();if(fail)throw std::runtime_error("injected texture device failure");}
};
template<class F>void rejects(F f){bool failed=false;try{f();}catch(const std::exception&){failed=true;}require(failed,"invalid texture operation accepted");}
Model quad(){Model m;m.parts=1;m.textures={"Overlays\\Start0"};m.minimum={-1,-1,0};m.maximum={1,1,0};
    m.vertices={{{-1,-1,0},{0,0,1},0,1},{{-1,1,0},{0,0,1},0,0},{{1,1,0},{0,0,1},1,0},
                {{-1,-1,0},{0,0,1},0,1},{{1,1,0},{0,0,1},1,0},{{1,-1,0},{0,0,1},1,1}};
    m.surfaces={{0,6,0,false,true,0}};return m;
}
}
int main(int argc,char** argv)try{
    require(argc==4,"expected retail root, report directory, hardware|warp");const bool warp=std::string_view(argv[3])=="warp";require(warp||std::string_view(argv[3])=="hardware","invalid texture adapter");
    const auto output=std::filesystem::absolute(argv[2]);std::filesystem::create_directories(output);AssetStore assets(argv[1]);unsigned contracts=0;
    auto probe_platform=std::make_shared<RaceSessionPlatform>(SourceColdConstructionOnly{},argv[1],0.0f);
    Device probe;RaceOverlayTextures bank(assets,probe,probe_platform);probe.bank=&bank;require(bank.selected_overlay_texture()==0,"selection not initialized by source reset");++contracts;
    for(unsigned id=16;id<=25;++id){bank.replace_source_texture(15,id);require(probe.observed_selection==id&&bank.source_image(id).rgba.size()==262144,"selection not published before upload");++contracts;}
    const auto uploads=probe.uploads;bank.replace_source_texture(15,20);require(probe.uploads==uploads+1,"texture callee invented duplicate suppression");++contracts;
    probe.width=255;const auto count=probe.uploads;bank.replace_source_texture(15,16);require(probe.uploads==count&&bank.selected_overlay_texture()==20,"width mismatch changed image or selection");++contracts;
    probe.width=256;bank.replace_source_texture(14,17);require(bank.selected_overlay_texture()==20,"non15destination changed selection");++contracts;
    probe.fail=true;rejects([&]{bank.replace_source_texture(15,18);});require(bank.selected_overlay_texture()==18&&probe.observed_selection==18,"device failure rolled back source selection");++contracts;
    probe.fail=false; // Invalid-ID guards must not pass via injected upload failure.
    rejects([&]{bank.replace_source_texture(15,15);});++contracts;
    rejects([&]{bank.replace_source_texture(15,30);});++contracts;
    WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"SGN-Overlay-Texture-Probe";
    require(RegisterClassW(&wc)!=0,"RegisterClass failed");Window window{CreateWindowExW(0,wc.lpszClassName,L"Stunt GP texture qualification",WS_OVERLAPPEDWINDOW,0,0,640,480,nullptr,nullptr,wc.hInstance,nullptr)};require(window.handle!=nullptr,"CreateWindow failed");
    Renderer renderer(window.handle,{true,warp,false,false,true});renderer.resize(640,480);const auto model=quad();renderer.load(model,assets);
    RendererTextureBindings bindings(renderer);const auto native=renderer.texture_index(0,0);bindings.bind(15,native);auto render_platform=std::make_shared<RaceSessionPlatform>(SourceColdConstructionOnly{},argv[1],0.0f);RaceOverlayTextures textures(assets,bindings,render_platform);
    const View view{{0,0,3},{0,0,0}};std::uint64_t compared=0;std::vector<Bytes> frames;
    for(unsigned id=16;id<=25;++id){textures.replace_source_texture(15,id);require(bindings.native_texture(15)==native&&renderer.texture_index(0,0)==native,"replacement changed mesh shader binding identity");
        const auto image=renderer.read_texture(native);const auto& expected=textures.source_image(id);require(image.width==256&&image.height==256&&image.rgba==expected.rgba,"GPU destination pixels differ from actual source image");compared+=image.rgba.size();
        for(unsigned i=0;i<3;++i)renderer.draw(view,"","",1);const auto path=output/("overlay-"+std::to_string(id)+".bmp");renderer.capture(path);frames.push_back(read_file(path));}
    for(unsigned i=0;i<frames.size();++i)for(unsigned j=i+1;j<frames.size();++j)require(frames[i]!=frames[j],"distinct original overlays did not change rendered image");
    Image narrow{8,7,Bytes(8*7*4)};for(unsigned i=0;i<narrow.rgba.size();++i)narrow.rgba[i]=static_cast<std::uint8_t>(i*17);
    const auto narrow_slot=renderer.append_texture(narrow);bindings.bind(15,narrow_slot);const auto bytes=renderer.stats().uploaded_texture_bytes;textures.replace_source_texture(15,16);
    require(textures.selected_overlay_texture()==25&&renderer.stats().uploaded_texture_bytes==bytes&&renderer.read_texture(narrow_slot).rgba==narrow.rgba,"actual GPU width mismatch performed upload");++contracts;
    bindings.bind(15,native);bindings.bind(14,narrow_slot);Image same_width{256,128,Bytes(256*128*4)};const auto tall=renderer.append_texture(same_width);bindings.bind(14,tall);textures.replace_source_texture(14,16);
    require(renderer.texture_dimensions(tall)[1]==256&&textures.selected_overlay_texture()==25,"adapter added height guard or changed15selection");++contracts;
    rejects([&]{bindings.bind(90,9999);});rejects([&]{renderer.replace_texture(native,Image{256,256,{}});});rejects([&]{(void)bindings.destination_width(90);});contracts+=3;
    require(renderer.read_texture(native).rgba==textures.source_image(25).rgba,"invalid replacement changed existing destination");renderer.finish();renderer.check_health();require(renderer.stats().debug_errors==0,"texture GPU debug errors");
    std::cout<<"{\"passed\":true,\"contracts\":"<<contracts<<",\"images\":10,\"gpu_compared_bytes\":"<<compared<<",\"frames\":"<<renderer.stats().presents<<",\"debug_errors\":"<<renderer.stats().debug_errors<<",\"errors\":0}\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
