#include "sgn/retail_vehicle_anchors.hpp"
#include <bit>
#include <cstring>
#include <fstream>
#include <iostream>
using namespace sgn;
using namespace sgn::retail;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
std::uint32_t get(const Bytes& b,std::size_t at){std::uint32_t value;std::memcpy(&value,b.data()+at,4);return value;}
void put(Bytes& b,std::size_t at,std::uint32_t value){std::memcpy(b.data()+at,&value,4);}
template<class F> void rejects(F&& call){bool rejected=false;try{call();}catch(const FormatError&){rejected=true;}require(rejected,"invalid PMD accepted");}
}
int main(int argc,char**argv){try {
    require(argc==2,"expected original car PMD path");std::ifstream file(argv[1],std::ios::binary);require(bool(file),"cannot open PMD");
    Bytes raw((std::istreambuf_iterator<char>(file)),{});VehicleAnchorModelView model(raw);
    const auto descriptor=0x148+std::size_t(get(raw,0x4c));const auto parts=0x148+std::size_t(get(raw,0x48));
    const auto first=get(raw,descriptor+0x48);const auto p11=parts+(std::size_t(first)+11)*64;
    require(model.part_count()>24,"test fixture needs original25parts");
    for(unsigned n:{0u,9u,327u})rejects([&]{VehicleAnchorModelView m(std::span<const std::uint8_t>(raw.data(),n));});
    rejects([&]{VehicleAnchorModelView m(raw,4);});
    auto mutate=[&](std::size_t at,std::uint32_t value,auto&& test){auto damaged=raw;put(damaged,at,value);rejects([&]{test(damaged);});};
    mutate(0,0,[](const Bytes& b){VehicleAnchorModelView m(b);});
    mutate(0x4c,0xffffffff,[](const Bytes& b){VehicleAnchorModelView m(b);});
    mutate(0xdc,65,[](const Bytes& b){VehicleAnchorModelView m(b);});
    mutate(0x48,0xffffffff,[](const Bytes& b){VehicleAnchorModelView m(b);});
    mutate(0x44,get(raw,0x48),[](const Bytes& b){VehicleAnchorModelView m(b);});
    mutate(descriptor+0x58,4,[](const Bytes& b){VehicleAnchorModelView m(b);});
    mutate(descriptor+0x24,0,[](const Bytes& b){VehicleAnchorModelView m(b);});
    mutate(p11+8,0xffffffff,[](const Bytes& b){VehicleAnchorModelView m(b);m.part(11,0);});
    mutate(p11+0x30,0xffffffff,[](const Bytes& b){VehicleAnchorModelView m(b);m.maximum_vertex_z(11,0);});
    mutate(p11+0x28,0x0000ffff,[](const Bytes& b){VehicleAnchorModelView m(b);m.maximum_vertex_z(11,0);});
    const auto vertex=0x148+std::size_t(get(raw,0x2c))+std::size_t(get(raw,p11+0x30))*16;
    require((get(raw,p11+0x28)&0xffff)!=0,"part11 needs vertices");
    mutate(vertex+8,0x7f800000,[](const Bytes& b){VehicleAnchorModelView m(b);m.maximum_vertex_z(11,0);});
    rejects([&]{model.part(0xffffffff,0);});rejects([&]{model.part(0,4);});
    // View borrows current caller bytes. No transform snapshot can become stale.
    auto mutable_raw=raw;VehicleAnchorModelView borrowed(mutable_raw);const auto tr=0x148+std::size_t(get(raw,0x44))+std::size_t(get(raw,p11+8))*80;
    put(mutable_raw,tr,0x43400000);put(mutable_raw,tr+12,0x7fc0beef);
    const auto observed=borrowed.part(11,0);require(observed.position.x==192&&std::bit_cast<std::uint32_t>(observed.position.w)==0x7fc0beef,"stale or truncated transform");
    // Missing models are not dereferenced for inactive physical slots.
    std::array<VehicleAnchorState,6> a{};std::array<VehiclePresentationState,6> p{};std::array<VehicleTrailInputs,6> t{};std::array<VehicleRaceState,6> r{};
    auto make=[&](unsigned i){return VehicleAnchorVehicle{r[i],p[i],t[i],a[i],nullptr};};
    std::array<VehicleAnchorVehicle,6> views{{make(0),make(1),make(2),make(3),make(4),make(5)}};
    unsigned current=5;VehicleAnchorWorld world{views,current};initialize_vehicle_extent_anchors(world);require(current==0,"current slot not restored");
    for(auto& trail:t)trail.pair_b_available={7,9};initialize_vehicle_pair_b_anchors(world);
    for(auto& trail:t)require(trail.pair_b_available==std::array<std::uint32_t,2>{0,0},"inactive pairB flags not cleared");
    r[0].active=1;rejects([&]{initialize_vehicle_extent_anchors(world);});
    std::cout<<"{\"passed\":true,\"reader_rejections\":18,\"borrowed_transform_live\":true,\"inactive_null_models\":true}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
