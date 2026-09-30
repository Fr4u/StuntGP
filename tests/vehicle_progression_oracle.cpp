#include "vehicle_progression_state.hpp"
#include "vehicle_ai_automatic_state.hpp"
#include "contact_solver_state.hpp"
#include <algorithm>
#include <bit>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>

namespace {
using namespace sgn::retail;
using Record=std::vector<std::uint8_t>;
constexpr unsigned size=0x30000,cs=0x7800,ca=0x2d000,ctx=0x2f000;
std::uint32_t u(const Record& b,unsigned at){std::uint32_t x;std::memcpy(&x,b.data()+at,4);return x;}
void put(Record& b,unsigned at,std::uint32_t x){std::memcpy(b.data()+at,&x,4);}
float f(const Record& b,unsigned at){return std::bit_cast<float>(u(b,at));}
std::int32_t i(const Record& b,unsigned at){return std::bit_cast<std::int32_t>(u(b,at));}
std::string_view text(const Record& b,unsigned at,unsigned bound) {
    const char* first=reinterpret_cast<const char*>(b.data()+at);const auto end=static_cast<const char*>(std::memchr(first,0,bound));
    if(!end)throw std::runtime_error("progression text terminator");return {first,static_cast<std::size_t>(end-first)};
}
struct Audio final:VehicleStuntChainAudio {
    std::vector<std::array<std::uint32_t,7>> events;
    void play_chain(std::uint32_t id)override{events.push_back({id,0,0,255,0,1,0});}
};
std::vector<unsigned> apply(Record& b,unsigned entry,SplineSet& model) {
    const auto current=u(b,ctx),ticks=u(b,ctx+0x3c);
    if(current>=6 || u(b,ctx+0x38)>=4 || ticks==0 || ticks>1024)throw std::runtime_error("progression identity/duration");
    auto params=std::make_unique<std::array<VehicleParameters,6>>();
    auto physical=std::make_unique<std::array<VehicleState,6>>();
    auto contacts=std::make_unique<std::array<contact::solver::State,6>>();
    std::array<VehicleRaceState,6> races;std::array<VehicleProgressionState,6> states;std::array<AiAutomaticMemory,6> ai;
    auto transfer=[&](bool read){for(unsigned j=0;j<6;++j){auto bytes=std::span(b).subspan(j*cs,cs);
        contact::oracle::transfer(bytes,(*params)[j],(*physical)[j],races[j],(*contacts)[j],read);
        progression_oracle::transfer(bytes,states[j],read);automatic_oracle::transfer(bytes,ai[j],read);
        if(read)races[j].place=u(b,j*cs+0x24ac);else put(b,j*cs+0x24ac,races[j].place);
    }};transfer(true);
    contact::player::Controls controls;controls.current=u(b,ctx+0x38);
    auto controllers=[&](bool read){for(unsigned j=0;j<4;++j){auto& c=controls.controllers[j];const auto at=ca+j*0x74c;
        if(read){c.active=i(b,at);c.vehicle=u(b,at+0xac);std::memcpy(c.notices.data(),b.data()+at+0x4ac,sizeof(c.notices));}
        else {put(b,at,std::bit_cast<std::uint32_t>(c.active));put(b,at+0xac,c.vehicle);std::memcpy(b.data()+at+0x4ac,c.notices.data(),sizeof(c.notices));}
    }};controllers(true);
    for(const auto& c:controls.controllers){if(c.vehicle>=6)throw std::runtime_error("progression controller owner");
        for(const auto& n:c.notices)if(n.source_vehicle>=6)throw std::runtime_error("progression notice owner");}
    RaceClockState clock;clock.game_mode=u(b,ctx+4);clock.phase=u(b,ctx+8);clock.lap_count=u(b,ctx+12);
    ProgressionRecordState records{u(b,ctx+0x28),u(b,ctx+0x2c),u(b,ctx+0x30)};
    auto spline=model.find(1);if(!spline)throw std::runtime_error("progression missing grid spline");
    auto global_grid=spline;if(u(b,ctx+0xc0)==0)global_grid.reset();
    TimeTrialGhosts ghosts;const auto role=u(b,ctx+0x54);if(role>=6)throw std::runtime_error("progression replay identity");
    ghosts.roles[0].replay={role};ghosts.roles[0].lap_ticks=u(b,ctx+0x58);
    ReplayBank replays;const auto threshold=u(b,ctx+0x50);std::memcpy(replays.bytes().data()+ReplayBank::base({role})+0x1c,&threshold,4);
    const auto arena_before=std::vector<std::uint8_t>(replays.bytes().begin(),replays.bytes().end());
    std::array<std::string_view,6> names;for(unsigned j=0;j<6;++j)names[j]=text(b,ctx+0x100+j*64,64);
    const auto enabled=u(b,ctx+0x34);Audio audio;
    VehicleProgressionContext context{clock,records,global_grid,{f(b,ctx+0x18),f(b,ctx+0x1c),f(b,ctx+0x20)},
        controls,current,u(b,ctx+0x10),enabled,ghosts,replays,names,text(b,ctx+0x400,64),text(b,ctx+0x440,64),
        text(b,ctx+0x480,64),text(b,ctx+0x4c0,64),audio};
    auto& s=(*physical)[current];auto& race=races[current];auto& state=states[current];bool opaque=false;
    for(unsigned n=0;n<ticks;++n) {
        if(entry==0x41de70)tick_vehicle_race_timers(s,race,clock);
        else if(entry==0x41d9c0)accept_vehicle_progress(race,i(b,ctx+0x580));
        else if(entry==0x41ded0)opaque|=project_vehicle_progress(race,state,{s.position.x,s.position.y,s.position.z},*spline,*spline,context.unit_gravity).heading_opaque;
        else if(entry==0x41dce0)score_vehicle_lap(s,race,state,records);
        else if(entry==0x41dc30)record_vehicle_lap(race,state,context);
        else if(entry==0x41d3f0){const auto value=format_progression_time(u(b,ctx+0x580));std::memcpy(b.data()+ctx+0x740,value.c_str(),value.size()+1);}
        else if(entry==0x41d4b0)announce_vehicle_checkpoint(race,state,context);
        else if(entry==0x41e0f0)check_vehicle_wrong_way(s,race,state,context);
        else if(entry==0x41dd60)complete_time_trial_lap(s,race,state,ai[current],context);
        else if(entry==0x41d760)check_time_trial_checkpoints(race,state,context);
        else if(entry==0x41e210)opaque|=tick_time_trial_progression(s,race,state,ai[current],context).heading_opaque;
        else throw std::runtime_error("unknown progression entry");
    }
    if(!std::equal(arena_before.begin(),arena_before.end(),replays.bytes().begin()))throw std::runtime_error("progression mutated replay arena");
    transfer(false);controllers(false);put(b,ctx+0x38,controls.current);
    put(b,ctx+0x28,records.previous_lap_ticks);put(b,ctx+0x2c,records.lap_ticks);put(b,ctx+0x30,records.score);
    if(audio.events.size()>8)throw std::runtime_error("progression audio event capacity");put(b,ctx+0x600,static_cast<unsigned>(audio.events.size()));
    for(unsigned j=0;j<audio.events.size();++j)for(unsigned k=0;k<7;++k)put(b,ctx+0x604+j*28+k*4,audio.events[j][k]);
    return opaque?std::vector<unsigned>{current*cs+0x24cc}:std::vector<unsigned>{};
}
void read(std::ifstream& in,void* out,std::size_t n){if(!in.read(static_cast<char*>(out),static_cast<std::streamsize>(n)))throw std::runtime_error("short progression corpus");}
unsigned read_word(std::ifstream& in){unsigned n;read(in,&n,4);return n;}
}
int main(int argc,char** argv){try {
    if(argc!=3)throw std::runtime_error("expected progression corpus and authored model directory");
    std::ifstream in(argv[1],std::ios::binary);char magic[8];read(in,magic,8);
    if(std::string(magic,8)!="SGNPR001")throw std::runtime_error("progression corpus schema");
    const auto count=read_word(in);if(count==0||count>20000)throw std::runtime_error("progression corpus count");
    std::uint64_t exact=0,errors=0,ticks=0,excluded=0,retained=0;std::map<std::string,std::unique_ptr<SplineSet>> models;
    for(unsigned n=0;n<count;++n) {
        const auto entry=read_word(in);Record before(size),expected(size);read(in,before.data(),size);read(in,expected.data(),size);
        const auto mask_count=read_word(in);if(mask_count>1)throw std::runtime_error("progression opaque mask count");
        std::vector<unsigned> supplied(mask_count);for(auto& at:supplied)at=read_word(in);
        const std::string name(text(before,ctx+0x80,64));
        if(name.empty()||name.find_first_of("/\\:")!=std::string::npos||name.find("..")!=std::string::npos)throw std::runtime_error("progression model name");
        if(!models.contains(name)) {
            std::ifstream model(std::filesystem::path(argv[2])/name,std::ios::binary|std::ios::ate);const auto length=model.tellg();
            if(length<0||length>1024ll*1024*1024)throw std::runtime_error("progression model size");
            sgn::Bytes bytes(static_cast<std::size_t>(length));model.seekg(0);if(!model.read(reinterpret_cast<char*>(bytes.data()),length))throw std::runtime_error("progression model read");
            models.emplace(name,std::make_unique<SplineSet>(std::move(bytes)));
        }
        auto actual=before;const auto mask=apply(actual,entry,*models.at(name));if(mask!=supplied)throw std::runtime_error("progression execution mask differs");
        ticks+=u(before,ctx+0x3c);excluded+=mask.size();
        for(const auto at:mask){++retained;if(u(actual,at)!=u(before,at))throw std::runtime_error("progression opaque lane not retained");}
        for(unsigned at=0;at<size;at+=4){if(std::binary_search(mask.begin(),mask.end(),at))continue;
            if(u(actual,at)==u(expected,at)){++exact;continue;}
            if(errors++<24)std::cerr<<"case "<<n<<" entry "<<std::hex<<entry<<" offset "<<at<<" expected "<<u(expected,at)<<" actual "<<u(actual,at)<<std::dec<<'\n';
        }
    }
    if(in.peek()!=std::char_traits<char>::eof())throw std::runtime_error("trailing progression corpus");
    std::cout<<"{\"cases\":"<<count<<",\"ticks\":"<<ticks<<",\"exact_words\":"<<exact<<",\"errors\":"<<errors<<",\"allowed_ulp\":0,\"excluded_words\":"<<excluded<<",\"native_retained_words\":"<<retained<<"}\n";
    return errors?1:0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
