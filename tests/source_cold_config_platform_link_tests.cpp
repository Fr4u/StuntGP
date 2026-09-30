// Main-only receiving: link solely to race_session. The platform owns a
// SourceBootConfigState with an out-of-line constructor and raw-cache deleter.
#include "sgn/race_session.hpp"
#include "sgn/source_allocation_bank.hpp"
#include <filesystem>
#include <iostream>
#include <string>
int main(){
    try{
        sgn::retail::RaceSessionPlatform platform(sgn::retail::SourceColdConstructionOnly{},
            std::filesystem::temp_directory_path()/"sgn-cfg-transitive-link-fixture",0.0f);
        const auto& cfg=platform.state().boot_config;
        const auto h=platform.host_readiness();
        if(std::string(cfg.base_path.data())!="D:\\Game\\"||cfg.files.archive||!cfg.boundary.is_null||
            platform.source_allocations().initialized()||h.assets||h.audio_device||h.audio_stream||h.window_input)
            throw std::runtime_error("cold CFG construction/transitive link contract");
        std::cout<<"race_session PUBLIC source_boot_config transitive construction/destruction: PASS\n";return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
