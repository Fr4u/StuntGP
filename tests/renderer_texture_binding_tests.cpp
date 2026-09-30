#include "sgn/renderer.hpp"
#include "sgn/live_scene_geometry.hpp"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>

namespace {
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
struct Window {HWND handle{};~Window(){if(handle)DestroyWindow(handle);}};
std::array<std::uint8_t,3> diagnostic_color(unsigned slot){
    require(slot<511,"diagnostic color capacity exceeded");
    ++slot; //reserve zero so an absent draw cannot satisfy the first binding
    return {static_cast<std::uint8_t>(((slot>>0)&7u)*255u/7u),
        static_cast<std::uint8_t>(((slot>>3)&7u)*255u/7u),
        static_cast<std::uint8_t>(((slot>>6)&7u)*255u/7u)};
}
}

int main(int argc,char** argv){try{
    using namespace sgn;
    require(argc==4,"expected retail root, new output directory, hardware|warp");
    const bool warp=std::string_view(argv[3])=="warp";
    require(warp||std::string_view(argv[3])=="hardware","invalid adapter");
    const auto output=std::filesystem::absolute(argv[2]);
    require(!std::filesystem::exists(output),"preserve previous evidence: choose a new output directory");
    std::filesystem::create_directories(output);
    AssetStore assets(argv[1]);
    std::vector<Model> models;
    for(unsigned car=1;car<=6;++car){
        const auto name="car"+std::to_string(car);
        const auto first=assets.model(name);
        for(unsigned lod=0;lod<first.lod_count;++lod){
            // Renderer-only fixture, not a gameplay or skin fix. Real six-car
            // texture tables are deliberately reordered per LOD to expose a
            // shared local-index cache. A simple face permits exact GPU sampling.
            auto model=assets.model(name,lod);
            require(model.textures.size()>2,"fixture requires distinct local materials 0 and 2");
            if(lod==1)std::rotate(model.textures.begin(),model.textures.begin()+1,model.textures.end());
            if(lod==2)std::reverse(model.textures.begin(),model.textures.end());
            model.vertices={{{-2,-2,0},{0,0,1},0.5f,0.5f},{{2,-2,0},{0,0,1},0.5f,0.5f},{{0,2,0},{0,0,1},0.5f,0.5f}};
            model.surfaces={{0,3,0,false,true,0}};
            model.parts=1;models.push_back(std::move(model));
        }
    }
    require(models.size()==18,"expected three authored LODs for all six start cars");
    WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"SGN-Texture-Binding-Probe";
    require(RegisterClassW(&wc)!=0,"RegisterClass failed");
    Window window{CreateWindowExW(0,wc.lpszClassName,L"SGN texture binding regression",WS_OVERLAPPEDWINDOW,
        0,0,640,480,nullptr,nullptr,wc.hInstance,nullptr)};
    require(window.handle!=nullptr,"CreateWindow failed");
    Renderer renderer(window.handle,{true,warp,false,false,true});renderer.resize(640,480);renderer.load_scene(models,assets);
    std::map<std::string,unsigned> bindings;
    std::map<unsigned,std::string> reverse;
    std::uint64_t image_bytes{};unsigned local_bindings{};
    const auto check_bindings=[&]{
        for(unsigned mesh=0;mesh<models.size();++mesh)
            for(unsigned local=0;local<models[mesh].textures.size();++local)
                require(renderer.texture_index(mesh,local)==bindings.at(models[mesh].textures[local]),"mesh-local texture identity changed");
    };
    for(unsigned mesh=0;mesh<models.size();++mesh){
        for(unsigned local=0;local<models[mesh].textures.size();++local){
            const auto& name=models[mesh].textures[local];const auto slot=renderer.texture_index(mesh,local);++local_bindings;
            const auto [binding,inserted]=bindings.emplace(name,slot);
            require(binding->second==slot,"equal texture paths received different descriptors");
            if(inserted){
                require(reverse.emplace(slot,name).second,"distinct texture paths share one descriptor");
                const auto expected=assets.texture(name);const auto actual=renderer.read_texture(slot);
                require(actual.width==expected.width&&actual.height==expected.height&&actual.rgba==expected.rgba,"GPU base image differs from its named retail texture");
                image_bytes+=actual.rgba.size();
            }
        }
    }
    for(const auto& [slot,name]:reverse){
        (void)name;const auto c=diagnostic_color(slot);
        renderer.replace_texture(slot,Image{1,1,Bytes{c[0],c[1],c[2],255}});
    }
    const auto texture_bytes=renderer.stats().uploaded_texture_bytes;
    unsigned frames{},rejected{};std::ofstream samples(output/"samples.csv");
    require(bool(samples),"cannot write sample evidence");
    samples<<"pass,mesh,local,descriptor,expected_r,expected_g,expected_b,actual_r,actual_g,actual_b\n";
    const View view{{0,0,-5},{0,0,0},0.1f,100};
    // Revisit every mesh with 0 -> 2 -> 0, reversing mesh order each pass.
    // A mistaken global/local double remap or mesh cache changes these pixels.
    for(unsigned pass=0;pass<3;++pass){
        for(unsigned order=0;order<models.size();++order){
            const auto mesh=pass%2?static_cast<unsigned>(models.size())-order-1:order;
            const unsigned local=pass==1?2:0;LiveGeometry live{models[mesh],{{LiveBlend::opaque}}};
            live.model.surfaces[0].texture=local;
            renderer.stage_mesh(mesh,live);check_bindings();
            const SceneInstance instance{mesh,{}, {}};
            renderer.draw_scene(view,std::span(&instance,1),"","",96);
            const auto file=output/("pass"+std::to_string(pass)+"-mesh"+std::to_string(mesh)+".bmp");renderer.capture(file);
            const auto bytes=read_file(file);constexpr std::size_t center=54+(std::size_t(240)*640+320)*4;
            require(bytes.size()>=center+4,"truncated framebuffer readback");
            const auto slot=bindings.at(models[mesh].textures[local]);const auto expected=diagnostic_color(slot);
            const std::array<std::uint8_t,3> actual{bytes[center+2],bytes[center+1],bytes[center]};
            samples<<pass<<','<<mesh<<','<<local<<','<<slot;
            for(auto channel:expected)samples<<','<<unsigned(channel);
            for(auto channel:actual)samples<<','<<unsigned(channel);
            samples<<'\n';samples.flush();
            for(unsigned channel=0;channel<3;++channel)
                require(std::abs(int(actual[channel])-int(expected[channel]))<=2,"draw selected the wrong mesh-local texture descriptor");
            ++frames;
        }
    }
    LiveGeometry invalid{models[0],{{LiveBlend::opaque}}};
    invalid.model.textures[0]+="__changed_test_binding";
    try{renderer.stage_mesh(0,invalid);}catch(const std::invalid_argument&){++rejected;}
    invalid={models[0],{{LiveBlend::opaque}}};invalid.model.surfaces[0].texture=static_cast<unsigned>(invalid.model.textures.size());
    try{renderer.stage_mesh(0,invalid);}catch(const std::invalid_argument&){++rejected;}
    require(rejected==2,"invalid changed binding or material index accepted");check_bindings();
    require(renderer.stats().uploaded_texture_bytes==texture_bytes,"staging geometry reuploaded textures");
    renderer.finish();renderer.collect_diagnostics();renderer.check_health();
    require(renderer.stats().debug_active&&renderer.stats().debug_errors==0,"GPU debug diagnostics");
    std::cout<<"{\"passed\":true,\"adapter\":\""<<(warp?"warp":"hardware")<<"\",\"meshes\":"<<models.size()
        <<",\"texture_descriptors\":"<<bindings.size()<<",\"mesh_local_bindings\":"<<local_bindings
        <<",\"retail_image_bytes_verified\":"<<image_bytes<<",\"pixel_checked_frames\":"<<frames
        <<",\"rejected_invalid_updates\":"<<rejected<<",\"debug_errors\":"<<renderer.stats().debug_errors<<"}\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
