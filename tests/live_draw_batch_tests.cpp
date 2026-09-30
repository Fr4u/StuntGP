#include "sgn/live_draw_batch.hpp"
#include <cstring>
#include <iostream>
#include <tuple>

namespace {
using namespace sgn;
void require(bool value,const char* error){if(!value)throw FormatError(error);}
using DrawVertex=std::tuple<std::uint32_t,std::uint32_t,std::uint32_t,bool,bool,LiveBlend>;
std::vector<DrawVertex> expand(const LiveGeometry& geometry){
    std::vector<DrawVertex> result;
    for(std::size_t i=0;i<geometry.model.surfaces.size();++i){
        const auto& d=geometry.model.surfaces[i];
        for(std::uint32_t j=0;j<d.count;++j)
            result.emplace_back(d.first+j,d.texture,d.part,d.translucent,d.two_sided,geometry.materials[i].blend);
    }
    return result;
}
void check(LiveGeometry& geometry){
    const auto stream=expand(geometry);const auto vertices=geometry.model.vertices;
    coalesce_live_draws(geometry);
    require(stream==expand(geometry),"draw state or primitive order changed");
    require(vertices.size()==geometry.model.vertices.size(),"vertex count changed");
    if(!vertices.empty())require(std::memcmp(vertices.data(),geometry.model.vertices.data(),vertices.size()*sizeof(Vertex))==0,"vertex bytes changed");
    const auto once=geometry.model.surfaces.size();coalesce_live_draws(geometry);
    require(once==geometry.model.surfaces.size()&&stream==expand(geometry),"batching is not idempotent");
}
}
int main(int argc,char** argv)try{
    using namespace sgn;
    LiveGeometry synthetic;synthetic.model.parts=2;synthetic.model.textures={"a","b"};synthetic.model.vertices.resize(36);
    synthetic.model.surfaces={{0,3,0,false,false,0},{3,3,0,false,false,0},
        {6,3,0,true,false,0},{9,3,0,true,false,0},{12,3,0,true,false,0},
        {15,3,0,true,true,0},{18,3,1,true,true,0},{21,3,1,true,true,1},
        {27,3,1,true,true,1},{24,3,1,true,true,1}};
    synthetic.materials={{LiveBlend::opaque},{LiveBlend::opaque},{LiveBlend::alpha},
        {LiveBlend::alpha},{LiveBlend::add},{LiveBlend::add},{LiveBlend::add},
        {LiveBlend::add},{LiveBlend::add},{LiveBlend::add}};
    check(synthetic);require(synthetic.model.surfaces.size()==8,"different draw boundaries were merged");
    unsigned rejected=0;
    auto invalid=synthetic;invalid.materials.pop_back();try{coalesce_live_draws(invalid);}catch(const FormatError&){++rejected;}
    invalid=synthetic;invalid.model.surfaces.back().count=2;try{coalesce_live_draws(invalid);}catch(const FormatError&){++rejected;}
    invalid=synthetic;invalid.model.surfaces.back().first=36;try{coalesce_live_draws(invalid);}catch(const FormatError&){++rejected;}
    require(rejected==3,"malformed draw accepted");
    unsigned actual=0,model_count=0,unsupported=0;std::size_t before=0,after=0;
    if(argc==2){AssetStore assets(argv[1]);retail::ModelResources models(assets,argv[1]);
        for(const auto& name:assets.models()){
            const auto bytes=read_file(std::filesystem::path(argv[1])/"meshdata"/(name+".PMD"));
            if(bytes.size()>=9&&std::string_view(reinterpret_cast<const char*>(bytes.data()),9)=="PMD V1.82"){
                ++unsupported;continue;
            }
            const auto texture_set=name.starts_with("track")&&name.ends_with("m")?"trackset00":"";
            const auto token=models.load(name,texture_set);auto& model=models.at(*models.resolve(token));++model_count;
            for(unsigned lod=0;lod<model.lod_count();++lod){
                auto geometry=model_live_geometry(model,lod);const auto original=geometry.model.surfaces.size();
                before+=original;check(geometry);after+=geometry.model.surfaces.size();++actual;
                if(name=="track1"||name=="car1"||name=="car16")
                    std::cerr<<name<<" lod="<<lod<<" draws="<<original<<" -> "<<geometry.model.surfaces.size()<<'\n';
            }
        }
        require(before>after&&model_count==78&&actual==110&&unsupported==10,"actual asset coverage or batching differs");
    }else if(argc!=1)throw FormatError("expected optional retail root");
    std::cout<<"{\"passed\":true,\"actual_models\":"<<model_count<<",\"actual_lods\":"<<actual<<",\"unsupported_v182\":"<<unsupported<<",\"draws_before\":"<<before<<",\"draws_after\":"<<after<<",\"rejected\":"<<rejected<<"}\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
