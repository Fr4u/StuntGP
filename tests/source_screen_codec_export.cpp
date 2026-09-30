// AUTHORED ONLY. Emits observations from real source-cold world storage.
#include "normal_session_ui_export.hpp"
#include "normal_session_ui_observer.hpp"
#include "sgn/source_screen_base.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace sgn::retail;
int main(int argc,char** argv)try{
    normal_session_owner::require(argc==3,"expected retail root and fresh output directory");
    const std::filesystem::path out=argv[2];
    normal_session_owner::require(!std::filesystem::exists(out),"screen codec output must be fresh");
    std::filesystem::create_directories(out);
    auto platform=std::make_shared<RaceSessionPlatform>(SourceColdConstructionOnly{},argv[1],0.0f);
    auto& s=platform->state();
    for(unsigned i=0;i<256;++i){auto& p=s.screen_polygons[i];p.texture=0x10200000u+i;p.flags=0x33440000u+i;p.alpha=0x55660000u+i;p.vertex_count=0x77880000u+i;
        for(unsigned j=0;j<8;++j){auto& v=p.vertices[j];v.x=float(i);v.y=float(j);v.z=-float(i+j);v.retained_w=0xaabb0000u+i*8+j;v.u=.25f;v.v=.75f;v.color=0xff120000u+i*8+j;v.outcode=0xccdd0000u+i*8+j;}}
    unsigned captures{};
    for(const auto count:std::array<std::uint32_t,5>{0u,1u,255u,256u,0xffffffffu})for(const bool published:{false,true}){
        s.screen_polygon_count=count;
        if(published)(void)publish_source_screen_base_42e4dc(platform);else (void)reset_source_scene_screen_base_42e572(platform);
        const auto name=std::to_string(count)+(published?"-live":"-null");
        normal_native_observer::Observations before;normal_native_observer::capture_ui_screen(s,before);before.save(out/(name+"-before.bin"));
        normal_session_owner::Image image(3);normal_session_owner::NativeAddresses addresses;
        normal_session_owner::export_ui_screen_owners(s,image,addresses);
        std::ofstream file(out/(name+"-owners.bin"),std::ios::binary);
        const std::array<normal_session_owner::NamedImage,1> images{{{name,&image}}};normal_session_owner::save_file(file,images);file.flush();
        normal_session_owner::require(bool(file),"screen codec wire write");
        normal_native_observer::Observations after;normal_native_observer::capture_ui_screen(s,after);after.save(out/(name+"-after.bin"));++captures;
    }
    std::cout<<"{\"captures\":"<<captures<<",\"full_Session_calls\":0,\"source13alloc_claim\":false}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
