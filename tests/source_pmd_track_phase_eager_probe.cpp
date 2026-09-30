#include "sgn/track_part_animation.hpp"
#include "sgn/model_resources.hpp"
#include <iostream>
using namespace sgn;using namespace sgn::retail;
int main(int argc,char** argv)try{
    if(argc!=3)throw FormatError("expected frozen eager track probe and retail root");
    SourceAllocationBank bank;bank.initialize_42eab0();AssetStore assets(argv[2]);ModelResources models(assets,argv[2]);const auto handle=*models.resolve(models.load_source(bank,"track1"));
    const auto count=std::string_view(argv[1])=="count0"?0u:1u;models.source_at(handle).descriptor().field<unsigned>(0x24).set(count);
    //The real pre-migration producer from the world preamble; no substitute callback.
    //This tests that producer only. No RaceSession/world execution is claimed.
    auto& model=models.at(handle);const auto parts=model.part_count(model.source_lod_index(0));
    advance_track_part_animations({},static_cast<unsigned>(parts));return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}
