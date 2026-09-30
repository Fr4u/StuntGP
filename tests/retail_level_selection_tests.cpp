#include "sgn/retail_level_selection.hpp"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
using namespace sgn;
using namespace sgn::retail;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
std::uint32_t get(const Bytes& b,std::size_t at){std::uint32_t x;std::memcpy(&x,b.data()+at,4);return x;}
void put(Bytes& b,std::size_t at,std::uint32_t x){std::memcpy(b.data()+at,&x,4);}
void save(const std::filesystem::path& path,const Bytes& b){std::ofstream f(path,std::ios::binary);check(bool(f.write(reinterpret_cast<const char*>(b.data()),b.size())),"fixture save failed");}
struct Clear:VehicleStorageClear {
    std::array<VehicleState,6> physical;
    unsigned calls{};
    void clear_vehicle_storage() override {for(auto& p:physical)p={};++calls;}
};
}
int main(int argc,char**argv){try {
    if(argc!=3)throw std::runtime_error("usage: retail_level_selection_tests asset_root fixture_root");
    const std::filesystem::path original=argv[1],root=argv[2];
    const auto replay=read_file(original/"wads/replays.wad");const auto table=get(replay,8);
    auto node=get(replay,table+4+name_hash("replays\\track1.rpl")*4);std::size_t entry=0;
    while(node){const auto at=table+node;const std::string name(reinterpret_cast<const char*>(replay.data()+at+12));
        if(canonical(name)=="replays\\track1.rpl"){entry=at;break;}node=get(replay,at);}
    check(entry!=0,"fixture replay missing");const auto offset=get(replay,entry+4);
    const auto config=read_file(original/"wads/config.wad");
    unsigned rejected=0;
    for(unsigned mode=0;mode<3;++mode){
        const auto path=root/ std::to_string(mode);std::filesystem::create_directories(path/"wads");
        auto altered=replay;
        if(mode==0)put(altered,offset+0x10,0xdeadbeef);
        if(mode==1)put(altered,entry+8,0x40);
        if(mode==2){check(table-offset>ReplayBank::stride,"fixture oversized space");put(altered,entry+8,ReplayBank::stride+1);}
        save(path/"wads/config.wad",config);save(path/"wads/replays.wad",altered);
        AssetStore assets(path);std::array<SelectionRecord,6> selection{};std::array<LevelCarSlot,6> cars{};
        ReplayBank bank;std::fill(bank.bytes().begin(),bank.bytes().end(),std::uint8_t{0xa5});const Bytes before(bank.bytes().begin(),bank.bytes().end());
        TimeTrialGhosts ghosts;TimeTrialSelectionState state;VehicleRandom random;LevelAssetBindings bindings;Clear clear;
        LevelSelectionContext context{selection,cars,bank,ghosts,state,random,assets,bindings,clear,"TRACK1",0x800000};
        try{load_track_replay(context);}catch(const FormatError&){++rejected;}
        check(std::equal(bank.bytes().begin(),bank.bytes().end(),before.begin()),"rejected replay changed bank");
    }
    check(rejected==3,"invalid import accepted");
    AssetStore assets(original);std::array<SelectionRecord,6> selection{};std::array<LevelCarSlot,6> cars{};
    ReplayBank bank;std::fill(bank.bytes().begin(),bank.bytes().end(),std::uint8_t{0xa5});TimeTrialGhosts ghosts;TimeTrialSelectionState state;
    VehicleRandom random;LevelAssetBindings bindings;Clear clear;
    LevelSelectionContext context{selection,cars,bank,ghosts,state,random,assets,bindings,clear,"TRACK1",0x800000};
    load_track_replay(context);check(bank.cursor({0})==get(replay,offset+0x10)-0x4d5cc0,"cursor import representation");
    const auto size=get(replay,entry+8);for(std::size_t i=0;i<size;++i)if(i<0x10||i>=0x14)check(bank.bytes()[i]==replay[offset+i],"import changed non-pointer byte");
    check(bank.bytes()[size]==0xa5,"short import cleared remainder");
    const Bytes before(bank.bytes().begin(),bank.bytes().end());context.level_name="MISSING";load_track_replay(context);
    check(!state.track.replay,"missing replay kept identity");check(std::equal(bank.bytes().begin(),bank.bytes().end(),before.begin()),"missing replay changed bank");
    std::cout<<"{\"passed\":true,\"rejected_imports\":3,\"bank_unchanged_on_rejection\":true,\"cursor_conversion\":true,\"remainder_preserved\":true}\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
