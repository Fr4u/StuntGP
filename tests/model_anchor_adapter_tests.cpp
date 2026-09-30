#include "sgn/model_anchor_adapter.hpp"
#include <bit>
#include <cstring>
#include <iostream>

namespace {
using namespace sgn;using namespace sgn::retail;
void require(bool condition,const char* reason){if(!condition)throw std::runtime_error(reason);}
bool same(const VehicleVector& a,const VehicleVector& b){return std::memcmp(&a,&b,16)==0;}
struct Owned {
    std::array<VehicleRaceState,6> race{};std::array<VehiclePresentationState,6> presentation{};
    std::array<VehicleTrailInputs,6> trails{};std::array<VehicleAnchorState,6> anchors{};unsigned current=5;
    std::array<VehicleAnchorVehicle,6> view(const VehicleAnchorModel& model){return {{{race[0],presentation[0],trails[0],anchors[0],&model},
        {race[1],presentation[1],trails[1],anchors[1],&model},{race[2],presentation[2],trails[2],anchors[2],&model},
        {race[3],presentation[3],trails[3],anchors[3],&model},{race[4],presentation[4],trails[4],anchors[4],&model},
        {race[5],presentation[5],trails[5],anchors[5],&model}}};}
};
void equal(const Owned& a,const Owned& b){
    require(a.current==b.current,"anchor changed current selector");
    for(unsigned j=0;j<6;++j){require(a.presentation[j].field_3134==b.presentation[j].field_3134,"anchor enable differs");
        const auto& x=a.anchors[j];const auto& y=b.anchors[j];require(same(x.position,y.position)&&same(x.forward,y.forward)&&std::bit_cast<unsigned>(x.extent)==std::bit_cast<unsigned>(y.extent),"extent anchor differs");
        require(x.effect_available==y.effect_available&&a.trails[j].pair_b_available==b.trails[j].pair_b_available,"effect flags differ");
        for(unsigned k=0;k<2;++k)require(same(x.effect_position[k],y.effect_position[k])&&same(x.effect_base_scale[k],y.effect_base_scale[k])&&same(a.trails[j].pair_a[k],b.trails[j].pair_a[k])&&same(a.trails[j].pair_b[k],b.trails[j].pair_b[k]),"anchor vector differs");
    }
}
}
int main(int argc,char** argv){try{
    if(argc!=2)throw std::runtime_error("expected retail data root");const std::filesystem::path root=argv[1];AssetStore assets(root);ModelResources store(assets,root);
    unsigned views=0,reads=0,rejections=0,compositions=0;
    for(const auto& name:assets.models()){
        const auto bytes=read_file(root/"meshdata"/(name+".PMD"));if(std::string(reinterpret_cast<const char*>(bytes.data()),9)!="PMD V1.83")continue;
        const std::string set=name.starts_with("track")&&name.ends_with("m")?"trackset00":"";auto& resource=store.at(*store.resolve(store.load(name,set)));
        for(unsigned lod=0;lod<resource.lod_count();++lod){VehicleAnchorModelView authored(bytes,lod);ModelAnchorAdapter live(resource,lod);++views;
            require(authored.part_count()==live.part_count()&&authored.selected_lod()==live.selected_lod(),"model view selector differs");
            for(unsigned index=0;index<authored.part_count()+2;++index){bool a_ok=true,b_ok=true;VehicleAnchorPart a,b;float az=0,bz=0;
                try{a=authored.part(index,lod);az=authored.maximum_vertex_z(index,lod);}catch(const std::exception&){a_ok=false;}
                try{b=live.part(index,lod);bz=live.maximum_vertex_z(index,lod);}catch(const std::exception&){b_ok=false;}
                require(a_ok==b_ok,"adjacent/unrelocated source boundary differs");if(!a_ok){++rejections;continue;}
                require(a.flags==b.flags&&same(a.position,b.position)&&same(a.scale,b.scale)&&same(a.forward,b.forward)&&std::bit_cast<unsigned>(az)==std::bit_cast<unsigned>(bz),"live source model input differs");++reads;
            }
            for(auto operation:{initialize_vehicle_extent_anchors,initialize_vehicle_effect_anchors,initialize_vehicle_pair_a_anchors,initialize_vehicle_pair_b_anchors}){
                Owned a,b;for(unsigned j=0;j<6;++j){a.race[j].active=b.race[j].active=(j%3==0?1:(j%3==1?0:2));a.anchors[j].extent=b.anchors[j].extent=19.5f;a.presentation[j].field_3134=b.presentation[j].field_3134=91;}
                auto av=a.view(authored),bv=b.view(live);bool a_ok=true,b_ok=true;
                try{operation({av,a.current});}catch(const std::exception&){a_ok=false;}
                try{operation({bv,b.current});}catch(const std::exception&){b_ok=false;}
                require(a_ok==b_ok,"live anchor initializer domain differs");equal(a,b);++compositions;
            }
            auto borrowed=resource.part(lod,0);const auto saved=borrowed.transform.position;const auto saved_flag=borrowed.flags;
            borrowed.transform.position={23,45,67};borrowed.flags=17;const auto observed=live.part(0,lod);
            require(observed.flags==17&&observed.position.x==23&&observed.position.y==45&&observed.position.z==67,"adapter copied stale model state");
            borrowed.transform.position=saved;borrowed.flags=saved_flag;
        }
    }
    require(views==110&&reads>1556&&rejections>0&&compositions==440,"anchor adapter coverage");
    std::cout<<"views="<<views<<" authored_reads="<<reads<<" rejected_adjacent_reads="<<rejections<<" initializer_compositions="<<compositions<<" errors=0\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
