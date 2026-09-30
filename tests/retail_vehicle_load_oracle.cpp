#include "retail_vehicle_load_state.hpp"
#include "vehicle_oracle_state.hpp"
#include "retail_race_state.hpp"
#include <fstream>
#include <iostream>
#include <memory>
using namespace sgn;
using namespace sgn::retail;
namespace {
constexpr std::size_t car_size=0x7800,globals=6*car_size,total=globals+4;
std::uint32_t get(const Bytes& b,std::size_t at){std::uint32_t v;std::memcpy(&v,b.data()+at,4);return v;}
void put(Bytes& b,std::size_t at,std::uint32_t v){std::memcpy(b.data()+at,&v,4);}
struct Reader {
    std::ifstream file;
    explicit Reader(const char* path):file(path,std::ios::binary){if(!file)throw std::runtime_error("cannot open corpus");}
    Bytes bytes(std::size_t size){if(size>2*total)throw std::runtime_error("excessive field");Bytes b(size);if(!file.read(reinterpret_cast<char*>(b.data()),size))throw std::runtime_error("truncated corpus");return b;}
    std::uint32_t word(){return get(bytes(4),0);}
    std::string text(){auto n=word();if(!n||n>128)throw std::runtime_error("invalid config name length");auto b=bytes(n);return {b.begin(),b.end()};}
};
void ownership(){VehicleParameters p{};VehicleState s{};VehicleRaceState r{};VehiclePositionHistory h{};VehicleLoadState l{};
    std::array<Bytes,3> images{Bytes(car_size,0xa5),Bytes(car_size,0xa5),Bytes(car_size,0xa5)};
    oracle::transfer(images[0],p,s,false);race_oracle::transfer(images[1],r,false);load_oracle::transfer(images[2],h,l,false);
    for(std::size_t at=0;at<car_size;++at){unsigned count=0;for(const auto& b:images)count+=b[at]!=0xa5;if(count>1)throw std::runtime_error("overlapping loader/physics/race ownership");}}
Bytes execute(Bytes b,unsigned op,unsigned target,std::int32_t config,std::int32_t control,const std::string& name,const AssetStore& assets){
    auto p=std::make_unique<std::array<VehicleParameters,6>>();auto s=std::make_unique<std::array<VehicleState,6>>();
    std::array<VehicleRaceState,6> r{};std::array<VehiclePositionHistory,6> h{};std::array<VehicleLoadState,6> l{};
    auto transfer=[&](bool read){for(unsigned i=0;i<6;++i){std::span image(b.data()+i*car_size,car_size);oracle::transfer(image,(*p)[i],(*s)[i],read);race_oracle::transfer(image,r[i],read);load_oracle::transfer(image,h[i],l[i],read);}};
    transfer(true);std::array<VehicleLoadVehicle,6> views{{{(*p)[0],(*s)[0],r[0],h[0],l[0]},
        {(*p)[1],(*s)[1],r[1],h[1],l[1]},{(*p)[2],(*s)[2],r[2],h[2],l[2]},
        {(*p)[3],(*s)[3],r[3],h[3],l[3]},{(*p)[4],(*s)[4],r[4],h[4],l[4]},
        {(*p)[5],(*s)[5],r[5],h[5],l[5]}}};
    auto current=get(b,globals);if(current>=6)throw std::runtime_error("invalid current slot");VehicleLoadWorld world{views,current};
    if(op==0){auto raw=assets.resource(name);l[target].sounds=decode_vehicle_sound_ids(std::string_view(reinterpret_cast<const char*>(raw.data()),raw.size()));}
    else if(op==1)reset_loaded_position_history((*s)[target],h[target]);
    else if(op==2)reset_loaded_vehicle_histories(views);
    else if(op==3)load_vehicle_configuration(world,assets,target,config,control);
    else throw std::runtime_error("invalid loader operation");
    transfer(false);put(b,globals,current);return b;
}
}
int main(int argc,char**argv){try {
    if(argc!=3)throw std::runtime_error("usage: retail_vehicle_load_oracle corpus asset_root");
    ownership();Reader input(argv[1]);auto signature=input.bytes(8);if(std::string(signature.begin(),signature.end())!="SGNLOAD1")throw std::runtime_error("invalid loader corpus signature");
    auto cases=input.word();if(!cases||cases>10000)throw std::runtime_error("invalid case count");AssetStore assets(argv[2]);std::uint64_t words=0;
    for(unsigned i=0;i<cases;++i){auto op=input.word(),target=input.word();auto config=std::bit_cast<std::int32_t>(input.word()),control=std::bit_cast<std::int32_t>(input.word());auto name=input.text();
        if(op>3||target>=6)throw std::runtime_error("invalid loader metadata");auto before=input.bytes(total),expected=input.bytes(total);
        auto actual=execute(std::move(before),op,target,config,control,name,assets);
        for(std::size_t at=0;at<total;at+=4){++words;if(get(actual,at)!=get(expected,at)){std::cerr<<"case "<<i<<" op "<<op<<" "<<name<<" at 0x"<<std::hex<<at<<" actual "<<get(actual,at)<<" source "<<get(expected,at)<<'\n';return 1;}}
    }
    if(input.file.peek()!=std::char_traits<char>::eof())throw std::runtime_error("trailing corpus data");
    std::cout<<"{\"passed\":true,\"cases\":"<<cases<<",\"words\":"<<words<<",\"mismatches\":0,\"ulp\":0,\"single_state_ownership\":true}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}}
