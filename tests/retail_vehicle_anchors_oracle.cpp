#include "retail_vehicle_anchors_state.hpp"
#include <fstream>
#include <iostream>
using namespace sgn;
using namespace sgn::retail;
namespace {
constexpr std::size_t car_size=0x7800,globals=6*car_size,total=globals+4;
std::uint32_t get(const Bytes& b,std::size_t at){std::uint32_t v;std::memcpy(&v,b.data()+at,4);return v;}
void put(Bytes& b,std::size_t at,std::uint32_t v){std::memcpy(b.data()+at,&v,4);}
struct Reader {
    std::ifstream file;
    explicit Reader(const char* path):file(path,std::ios::binary){if(!file)throw std::runtime_error("cannot open corpus");}
    Bytes bytes(std::size_t size){if(size>32*1024*1024)throw std::runtime_error("excessive corpus field");Bytes b(size);if(!file.read(reinterpret_cast<char*>(b.data()),size))throw std::runtime_error("truncated corpus");return b;}
    std::uint32_t word(){return get(bytes(4),0);}
};
Bytes execute(Bytes image,unsigned op,const Bytes& pmd,std::array<unsigned,6> lods,std::uint32_t null_mask){
    std::array<VehicleAnchorState,6> a{};std::array<VehiclePresentationState,6> p{};
    std::array<VehicleTrailInputs,6> t{};std::array<VehicleRaceState,6> r{};
    std::array<std::optional<VehicleAnchorModelView>,6> models;
    for(unsigned i=0;i<6;++i)if(!(null_mask&(1u<<i)))models[i].emplace(pmd,lods[i]);
    auto transfer=[&](bool read){for(unsigned i=0;i<6;++i){std::span b(image.data()+i*car_size,car_size);anchor_oracle::transfer(b,a[i],p[i],t[i],r[i],read);}};
    transfer(true);
    auto make=[&](unsigned i){return VehicleAnchorVehicle{r[i],p[i],t[i],a[i],models[i]?&*models[i]:nullptr};};
    std::array<VehicleAnchorVehicle,6> views{{make(0),make(1),make(2),make(3),make(4),make(5)}};
    auto current=get(image,globals);if(current>=6)throw std::runtime_error("invalid current slot");VehicleAnchorWorld world{views,current};
    switch(op){
        case 0:initialize_vehicle_extent_anchors(world);break;
        case 1:initialize_vehicle_effect_anchors(world);break;
        case 2:initialize_vehicle_pair_a_anchors(world);break;
        case 3:initialize_vehicle_pair_b_anchors(world);break;
        default:throw std::runtime_error("unknown anchor operation");
    }
    transfer(false);put(image,globals,current);return image;
}
}
int main(int argc,char**argv){try {
    if(argc!=2)throw std::runtime_error("usage: retail_vehicle_anchors_oracle corpus");Reader input(argv[1]);auto signature=input.bytes(8);
    if(std::string(signature.begin(),signature.end())!="SGNANCH1")throw std::runtime_error("invalid anchor corpus signature");
    auto cases=input.word();if(!cases||cases>20000)throw std::runtime_error("invalid case count");std::uint64_t words=0;unsigned rejected=0;
    for(unsigned i=0;i<cases;++i){auto op=input.word(),reject=input.word(),null_mask=input.word();
        if(op>3||reject>1||null_mask>63)throw std::runtime_error("invalid anchor metadata");
        std::array<unsigned,6> lods;for(auto& lod:lods)lod=input.word();auto pmd=input.bytes(input.word());auto before=input.bytes(total),expected=input.bytes(total);
        bool threw=false;Bytes actual;
        try{actual=execute(std::move(before),op,pmd,lods,null_mask);}catch(const FormatError&){if(!reject)throw;threw=true;}
        if(threw){++rejected;continue;}if(reject)throw std::runtime_error("expected native bounds rejection");
        for(std::size_t at=0;at<total;at+=4){++words;if(get(actual,at)!=get(expected,at)){
            std::cerr<<"case "<<i<<" op "<<op<<" at 0x"<<std::hex<<at<<" actual "<<get(actual,at)<<" source "<<get(expected,at)<<'\n';return 1;}}
    }
    if(input.file.peek()!=std::char_traits<char>::eof())throw std::runtime_error("trailing corpus data");
    std::cout<<"{\"passed\":true,\"cases\":"<<cases<<",\"words\":"<<words<<",\"rejected\":"<<rejected<<",\"mismatches\":0,\"ulp\":0}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}}
