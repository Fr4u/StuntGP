#include "sgn/renderer.hpp"
#include "sgn/live_scene_geometry.hpp"
#include <array>
#include <filesystem>
#include <iostream>

namespace {
struct Window {
    HWND handle{};
    ~Window() { if (handle) DestroyWindow(handle); }
};
}
int main(int argc, char** argv) {
    try {
        if(argc!=4) throw std::invalid_argument("expected retail root, output directory, hardware|warp");
        const bool warp=std::string_view(argv[3])=="warp";
        if(!warp && std::string_view(argv[3])!="hardware") throw std::invalid_argument("unknown adapter");
        const auto output=std::filesystem::absolute(argv[2]);
        std::filesystem::create_directories(output);
        WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"SGN-Pose-Probe";
        if(!RegisterClassW(&wc)) throw std::runtime_error("RegisterClass failed");
        Window window{CreateWindowExW(0,wc.lpszClassName,L"SGN pose regression",WS_OVERLAPPEDWINDOW,0,0,640,480,nullptr,nullptr,wc.hInstance,nullptr)};
        if(!window.handle) throw std::runtime_error("CreateWindow failed");
        // A hidden diagnostic window still renders and exposes explicit readback.
        sgn::AssetStore assets(argv[1]);
        auto authored=assets.model("car16");
        sgn::Model triangle;
        triangle.vertices={{{-1,-1,0},{0,0,1},0.5f,0.5f},{{1,-1,0},{0,0,1},0.5f,0.5f},{{0,1,0},{0,0,1},0.5f,0.5f}};
        triangle.textures={authored.textures.at(0)};
        triangle.parts=1;
        triangle.surfaces={{0,3,0,false,true,0}};
        sgn::Renderer renderer(window.handle,{true,warp,false,false,true});
        renderer.resize(640,480);
        renderer.load(triangle,assets);
        sgn::View view{{0,0,-5},{0,0,0},0.1f,100};
        std::array<sgn::SceneInstance::Part,1> parts{};
        sgn::SceneInstance instance{0,{},parts};
        const auto draw=[&](const char* name) {
            renderer.draw_scene(view,std::span(&instance,1),"","",1);
            renderer.capture(output/(std::string(name)+".bmp"));
            renderer.finish();renderer.collect_diagnostics();renderer.check_health();
            if(renderer.stats().debug_errors) throw std::runtime_error("GPU diagnostics reported a warning or error");
        };
        parts[0].visible=false;draw("hidden");
        parts[0].visible=true;parts[0].pose.forward={};draw("rank2");
        parts[0].pose.right={};parts[0].pose.up={};draw("rank0");
        // Exercise actual live GPU culling, including the recovered viewport.
        // A white texture makes the center-pixel visibility unambiguous.
        renderer.replace_texture(renderer.texture_index(0,0),sgn::Image{1,1,sgn::Bytes(4,255)});
        sgn::LiveGeometry live;live.model=triangle;live.materials={{sgn::LiveBlend::opaque}};
        for(auto& v:live.model.vertices)v.color=0xffffffffu;
        parts[0]={};unsigned checked=0;
        const auto visible=[&](const std::string& name){
            draw(name.c_str());const auto bytes=sgn::read_file(output/(name+".bmp"));
            const std::size_t center=54+(std::size_t(240)*640+320)*4;
            if(bytes.size()<center+4)throw std::runtime_error("truncated culling readback");
            ++checked;return bytes[center]>240&&bytes[center+1]>240&&bytes[center+2]>240;
        };
        for(bool source:{false,true}){
            view={{0,0,-5},{0,0,0},0.1f,100};
            live.model.vertices=triangle.vertices;for(auto& v:live.model.vertices)v.color=0xffffffffu;
            if(source){
                view.world_to_view=std::array<float,16>{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
                view.retail_viewport=sgn::View::RetailViewport{{1,1,1,0,32},1000,false};
                for(auto& v:live.model.vertices){v.position.x*=80;v.position.y*=80;v.position.z=200;}
            }
            const std::string prefix=source?"live-source-":"live-inspect-";
            live.model.surfaces[0].two_sided=false;renderer.stage_mesh(0,live);
            const bool forward=visible(prefix+"forward");
            std::swap(live.model.vertices[1],live.model.vertices[2]);renderer.stage_mesh(0,live);
            const bool reverse=visible(prefix+"reverse");
            if(forward==reverse)throw std::runtime_error("live one-sided winding was ignored");
            if(!reverse)std::swap(live.model.vertices[1],live.model.vertices[2]);
            instance.pose.right={-1,0,0};renderer.stage_mesh(0,live);
            if(!visible(prefix+"mirrored"))throw std::runtime_error("live mirrored front face vanished");
            instance.pose={};live.model.surfaces[0].two_sided=true;renderer.stage_mesh(0,live);
            if(!visible(prefix+"two-sided"))throw std::runtime_error("live two-sided front face vanished");
            std::swap(live.model.vertices[1],live.model.vertices[2]);renderer.stage_mesh(0,live);
            if(!visible(prefix+"two-sided-reverse"))throw std::runtime_error("live two-sided back face vanished");
        }
        std::cout<<"{\"passed\":true,\"frames\":"<<3+checked<<",\"live_culling_cases\":"<<checked<<",\"debug_errors\":"<<renderer.stats().debug_errors<<",\"warp\":"<<(warp?"true":"false")<<"}\n";
        return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
