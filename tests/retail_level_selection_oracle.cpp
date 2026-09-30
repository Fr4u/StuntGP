#include "replay_fixture_cursor_codec.hpp"
#include "sgn/retail_level_selection.hpp"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
using namespace sgn;
using namespace sgn::retail;
namespace {
constexpr std::size_t cars_at=6*sizeof(SelectionRecord),bank_at=cars_at+6*sizeof(LevelCarSlot),
    globals_at=bank_at+ReplayBank::size,total_size=globals_at+0xa0;
std::uint32_t get(std::span<const std::uint8_t> b,std::size_t at){std::uint32_t v;std::memcpy(&v,b.data()+at,4);return v;}
void put(Bytes& b,std::size_t at,std::uint32_t v){std::memcpy(b.data()+at,&v,4);}
struct Reader {
    std::ifstream f;
    explicit Reader(const char* path):f(path,std::ios::binary){if(!f)throw std::runtime_error("cannot open corpus");}
    Bytes bytes(std::size_t n){if(n>2*total_size)throw std::runtime_error("excessive field");Bytes b(n);if(!f.read(reinterpret_cast<char*>(b.data()),n))throw std::runtime_error("truncated corpus");return b;}
    std::uint32_t word(){return get(bytes(4),0);}
    std::string text(){auto n=word();if(!n||n>200)throw std::runtime_error("invalid name length");auto b=bytes(n);return {b.begin(),b.end()};}
};
std::optional<unsigned> identity(std::uint32_t value){if(value==0xffffffffu)return {};if(value>=6)throw std::runtime_error("invalid identity");return value;}
unsigned required_identity(std::uint32_t value){auto result=identity(value);if(!result)throw std::runtime_error("missing required identity");return *result;}
std::optional<ReplayHandle> handle(std::uint32_t value){auto v=identity(value);return v?std::optional{ReplayHandle{*v}}:std::nullopt;}
GhostSelection ghost(std::span<const std::uint8_t>b,std::size_t at){return {get(b,at),identity(get(b,at+4)),handle(get(b,at+8))};}
void ghost(Bytes& b,std::size_t at,const GhostSelection& g){put(b,at,g.header_value);put(b,at+4,g.vehicle.value_or(0xffffffffu));put(b,at+8,g.replay?g.replay->slot:0xffffffffu);}
struct Clear:VehicleStorageClear {
    unsigned count{};
    std::function<void()> check_order;
    std::array<VehicleState,6> physical; // explicit fixture binding; not assembled-world qualification
    void clear_vehicle_storage() override {check_order();for(auto& v:physical)v={};++count;}
};
Bytes execute(Bytes b,unsigned op,std::uint32_t memory,const std::string& name,const AssetStore& assets,LevelAssetBindings& bindings){
    std::array<SelectionRecord,6> records;std::array<LevelCarSlot,6> cars;
    std::memcpy(records.data(),b.data(),cars_at);std::memcpy(cars.data(),b.data()+cars_at,bank_at-cars_at);
    ReplayBank bank;std::copy_n(b.begin()+bank_at,ReplayBank::size,bank.bytes().begin());
    ::sgn::retail::fixture::legacy_replay_cursor_words(bank,true);
    const auto g=std::span<const std::uint8_t>(b).subspan(globals_at);
    TimeTrialGhosts ghosts;TimeTrialSelectionState state;
    for(unsigned i=0;i<4;++i){const auto p=i*16;ghosts.roles[i]={get(g,p),required_identity(get(g,p+4)),ReplayHandle{required_identity(get(g,p+8))},get(g,p+12)};state.availability[i]=get(g,0x40+i*4);}
    state.split_selection=get(g,0x50);state.previous_completed=get(g,0x54);
    state.track=ghost(g,0x58);state.selected=ghost(g,0x64);state.result_a=ghost(g,0x70);state.result_b=ghost(g,0x7c);
    ghosts.reference_completed=get(g,0x88);LevelOverrideState level;level.split=get(g,0x8c);VehicleRandom rng{get(g,0x90)};
    auto clear=std::make_unique<Clear>();for(auto& v:clear->physical)v.speed=123;
    const auto prior_bindings=bindings.requests;
    clear->check_order=[&]{
        if(std::memcmp(records.data(),b.data(),cars_at)||std::memcmp(cars.data(),b.data()+cars_at,bank_at-cars_at))
            throw std::runtime_error("vehicle clear moved after selection/car writes");
        if(bindings.requests.size()!=prior_bindings.size())throw std::runtime_error("vehicle clear moved after resource writes");
        for(unsigned i=0;i<prior_bindings.size();++i)if(bindings.requests[i].name!=prior_bindings[i].name||bindings.requests[i].type!=prior_bindings[i].type)
            throw std::runtime_error("vehicle clear moved after prior resource changes");
    };
    LevelSelectionContext c{records,cars,bank,ghosts,state,rng,assets,bindings,*clear,name,memory};
    switch(op){case 0:reset_time_trial_ghosts(ghosts,state);break;case 1:load_track_replay(c);break;
        case 2:prepare_time_trial_selection(c);break;case 3:prepare_time_trial_level(c,level);break;
        case 4:prepare_level_cars(c);break;default:throw std::runtime_error("invalid operation");}
    if(op==4)for(const auto& v:clear->physical)if(v.speed!=0)throw std::runtime_error("fixture clear not executed");
    std::memcpy(b.data(),records.data(),cars_at);std::memcpy(b.data()+cars_at,cars.data(),bank_at-cars_at);
    ::sgn::retail::fixture::legacy_replay_cursor_words(bank,false);
    std::copy(bank.bytes().begin(),bank.bytes().end(),b.begin()+bank_at);
    for(unsigned i=0;i<4;++i){const auto p=globals_at+i*16;const auto& r=ghosts.roles[i];put(b,p,r.active);put(b,p+4,r.vehicle);put(b,p+8,r.replay.slot);put(b,p+12,r.lap_ticks);put(b,globals_at+0x40+i*4,state.availability[i]);}
    put(b,globals_at+0x50,state.split_selection);put(b,globals_at+0x54,state.previous_completed);
    ghost(b,globals_at+0x58,state.track);ghost(b,globals_at+0x64,state.selected);ghost(b,globals_at+0x70,state.result_a);ghost(b,globals_at+0x7c,state.result_b);
    put(b,globals_at+0x88,ghosts.reference_completed);put(b,globals_at+0x8c,level.split);put(b,globals_at+0x90,rng.state);put(b,globals_at+0x94,clear->count);
    return b;
}
}
int main(int argc,char** argv){try {
    if(argc!=3)throw std::runtime_error("usage: retail_level_selection_oracle corpus asset_root");
    Reader input(argv[1]);auto magic=input.bytes(8);if(std::string(magic.begin(),magic.end())!="SGNSEL01")throw std::runtime_error("invalid corpus magic");
    auto count=input.word();if(!count||count>10000)throw std::runtime_error("invalid case count");
    AssetStore assets(argv[2]);std::uint64_t bytes=0,requests=0,clears=0;
    for(unsigned i=0;i<count;++i){const auto op=input.word(),memory=input.word();const auto name=input.text();
        auto before=input.bytes(total_size),expected=input.bytes(total_size);LevelAssetBindings bindings;
        const auto initial=input.word();if(initial>20)throw std::runtime_error("invalid prior bindings");
        for(unsigned j=0;j<initial;++j){auto type=input.word();bindings.requests.push_back({input.text(),type});}
        const auto wanted_count=input.word();if(wanted_count>32)throw std::runtime_error("invalid output bindings");
        std::vector<LevelAssetRequest>wanted;
        for(unsigned j=0;j<wanted_count;++j){auto type=input.word();wanted.push_back({input.text(),type});}
        auto actual=execute(std::move(before),op,memory,name,assets,bindings);
        for(std::size_t at=0;at<actual.size();++at){++bytes;if(actual[at]!=expected[at]){std::cerr<<"case "<<i<<" op "<<op<<" byte 0x"<<std::hex<<at<<" actual "<<unsigned(actual[at])<<" expected "<<unsigned(expected[at])<<'\n';return 1;}}
        if(bindings.requests.size()!=wanted.size())throw std::runtime_error("binding count differs");
        for(unsigned j=0;j<wanted.size();++j){++requests;if(bindings.requests[j].name!=wanted[j].name||bindings.requests[j].type!=wanted[j].type){std::cerr<<"case "<<i<<" binding "<<j<<" differs\n";return 1;}}
        clears+=get(actual,globals_at+0x94);
    }
    if(input.f.peek()!=std::char_traits<char>::eof())throw std::runtime_error("trailing corpus bytes");
    std::cout<<"{\"passed\":true,\"cases\":"<<count<<",\"bytes\":"<<bytes<<",\"requests\":"<<requests<<",\"clear_calls\":"<<clears<<",\"mismatches\":0}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}}
