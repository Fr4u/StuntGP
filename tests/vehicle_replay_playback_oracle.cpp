#include "replay_fixture_cursor_codec.hpp"
#include "vehicle_air_fixture.hpp"
#include "contact_solver_state.hpp"
#include "sgn/retail_replay_playback.hpp"
#include <fstream>
#include <iostream>

namespace {
using namespace sgn::retail;
constexpr unsigned size=0x11000,ctx=0x10800,headers=0x10900,frames=0x10c00;
using Record=std::array<std::uint8_t,size>;
unsigned u(const Record& b,unsigned at){unsigned value;std::memcpy(&value,b.data()+at,4);return value;}
void put(Record& b,unsigned at,unsigned value){std::memcpy(b.data()+at,&value,4);}
float f(const Record& b,unsigned at){return std::bit_cast<float>(u(b,at));}
std::optional<ReplayHandle> handle(unsigned value){if(value==0xffffffff)return {};if(value>=6)throw std::runtime_error("playback fixture handle");return ReplayHandle{value};}
unsigned identity(std::optional<ReplayHandle> value){return value?value->slot:0xffffffff;}
void initialize(ReplayBank& bank,const Record& b){
    const auto selected=handle(u(b,ctx+4));if(!selected || u(b,ctx+0x18)>255 || u(b,ctx+0x1c)>1)throw std::runtime_error("playback fixture initializer");
    auto bytes=bank.bytes();std::fill(bytes.begin(),bytes.end(),static_cast<std::uint8_t>(u(b,ctx+0x18)));
    for(unsigned j=0;j<6;++j)std::memcpy(bytes.data()+j*ReplayBank::stride,b.data()+headers+j*0x80,0x80);
    const auto cursor=u(b,headers+selected->slot*0x80u+0x10u),base=ReplayBank::base(*selected);
    if(cursor<base+ReplayBank::header_bytes || cursor>base+ReplayBank::stride-88 || (cursor-base-ReplayBank::header_bytes)%44)
        throw std::runtime_error("playback fixture two-frame domain");
    std::memcpy(bytes.data()+cursor,b.data()+frames,88);
    if(cursor+88+0x24<bytes.size())bytes[cursor+88+0x24]=static_cast<std::uint8_t>(u(b,ctx+0x1c));
}
std::vector<unsigned> apply(Record& b,ReplayBank& bank,unsigned entry){
    ::sgn::retail::fixture::legacy_replay_cursor_words(bank,true);
    const auto ticks=u(b,ctx);if(ticks==0 || ticks>1024)throw std::runtime_error("playback fixture ticks");
    air_oracle::Record common;std::copy_n(b.begin(),common.size(),common.begin());air_oracle::Fixture fixture(common);
    auto raw=std::span(common).subspan(air_oracle::vehicle_at,0x7800);
    contact::solver::State contact;contact::oracle::transfer_contact(raw,contact,true);
    std::array<VehicleWheelAnimation,4> wheels;
    for(unsigned j=0;j<4;++j){auto& wheel=wheels[j];const auto at=air_oracle::vehicle_at+0x2070+j*0x98;
        wheel.previous_angle=air_oracle::f(common,at);wheel.current_angle=air_oracle::f(common,at+4);
        for(unsigned k=0;k<16;++k)wheel.rotation[k]=air_oracle::f(common,air_oracle::vehicle_at+0x274c+j*64+k*4);}
    VehicleRaceState current,owner;current.recording=handle(air_oracle::u(common,air_oracle::vehicle_at+0x7268));owner.recording=handle(u(b,ctx+4));
    ReplayWorkspace workspace{handle(u(b,ctx+8)),u(b,ctx+0xc),u(b,ctx+0x10)};
    VehicleDynamicsWorkspace dynamics;for(unsigned j=0;j<16;++j)dynamics.steering[j]=f(b,ctx+0x40+j*4);
    const auto air=fixture.context();const ReplayPlaybackContext context{f(b,ctx+0x14),dynamics,air};
    ReplayPlaybackVehicle vehicle{fixture.parameters,fixture.physics,contact,wheels,fixture.air};
    VehicleAirTickObservation combined;bool decoded=false;
    for(unsigned tick=0;tick<ticks;++tick){ReplayPlaybackObservation observed;
        if(entry==0x419ec0)observed.air=replay_air_dynamics(vehicle,context);
        else if(entry==0x41ab70)observed=play_replay_frame(bank,owner,workspace,vehicle,context);
        else if(entry==0x41af50)observed=update_replay_vehicle(bank,current,workspace,vehicle,context);
        else throw std::runtime_error("unknown playback entry");
        decoded|=observed.decoded;combined.leveled_incremental|=observed.air.leveled_incremental;
        combined.orthogonalized|=observed.air.orthogonalized;combined.emitted_takeoff|=observed.air.emitted_takeoff;
    }
    contact::oracle::transfer_contact(raw,contact,false);fixture.save();
    for(unsigned j=0;j<4;++j){const auto& wheel=wheels[j];const auto at=air_oracle::vehicle_at+0x2070+j*0x98;
        air_oracle::put(common,at,std::bit_cast<unsigned>(wheel.previous_angle));air_oracle::put(common,at+4,std::bit_cast<unsigned>(wheel.current_angle));
        for(unsigned k=0;k<16;++k)air_oracle::put(common,air_oracle::vehicle_at+0x274c+j*64+k*4,std::bit_cast<unsigned>(wheel.rotation[k]));}
    std::copy(common.begin(),common.end(),b.begin());put(b,ctx+8,identity(workspace.recording));put(b,ctx+0xc,workspace.frame_offset);put(b,ctx+0x10,workspace.previous_frame_offset);
    ::sgn::retail::fixture::legacy_replay_cursor_words(bank,false);
    for(unsigned j=0;j<6;++j)std::memcpy(b.data()+headers+j*0x80,bank.bytes().data()+j*ReplayBank::stride,0x80);
    auto mask=fixture.excluded(combined);if(decoded)for(unsigned at:{0x1bb8u,0x1bc8u,0x1cc8u,0x1cd8u,0x1ce8u})mask.push_back(air_oracle::vehicle_at+at);
    std::sort(mask.begin(),mask.end());mask.erase(std::unique(mask.begin(),mask.end()),mask.end());return mask;
}
}
int main(int argc,char** argv){try {
    if(argc!=2)throw std::runtime_error("expected playback corpus");std::ifstream in(argv[1],std::ios::binary);
    const auto read=[&](void* out,std::size_t bytes){if(!in.read(static_cast<char*>(out),static_cast<std::streamsize>(bytes)))throw std::runtime_error("short playback corpus");};
    const auto next=[&](){unsigned value;read(&value,4);return value;};
    char magic[8];read(magic,8);if(std::string(magic,8)!="SGNPB001")throw std::runtime_error("playback schema");
    const auto count=next();if(count==0 || count>20000)throw std::runtime_error("playback count");
    std::uint64_t errors=0,exact=0,excluded=0,retained=0,ticks=0;
    for(unsigned n=0;n<count;++n){const auto entry=next();Record before{},expected{};read(before.data(),before.size());read(expected.data(),expected.size());
        const auto mask_count=next();if(mask_count>457)throw std::runtime_error("playback mask count");
        std::vector<unsigned> supplied(mask_count);for(auto& at:supplied)at=next();
        ReplayBank bank,reference;initialize(bank,before);initialize(reference,before);
        const auto changes=next();if(changes>ReplayBank::size)throw std::runtime_error("playback delta count");
        unsigned previous=0;for(unsigned j=0;j<changes;++j){const auto at=next();std::uint8_t value;read(&value,1);
            if(at>=ReplayBank::size || (j && at<=previous))throw std::runtime_error("playback delta order");reference.bytes()[at]=value;previous=at;}
        auto actual=before;const auto mask=apply(actual,bank,entry);if(mask!=supplied)throw std::runtime_error("playback dynamic mask case "+std::to_string(n));
        ticks+=u(before,ctx);excluded+=mask.size();for(unsigned at:mask){++retained;if(u(actual,at)!=u(before,at))throw std::runtime_error("playback native retention case "+std::to_string(n)+" offset "+std::to_string(at));}
        for(unsigned at=0;at<size;at+=4){if(std::binary_search(mask.begin(),mask.end(),at))continue;
            if(u(actual,at)==u(expected,at)){++exact;continue;}
            if(errors++<24)std::cerr<<"case "<<n<<" entry "<<std::hex<<entry<<" offset "<<at<<" expected "<<u(expected,at)<<" actual "<<u(actual,at)<<std::dec<<'\n';}
        for(unsigned at=0;at<ReplayBank::size;at+=4){unsigned actual_word,expected_word;std::memcpy(&actual_word,bank.bytes().data()+at,4);std::memcpy(&expected_word,reference.bytes().data()+at,4);
            if(actual_word==expected_word){++exact;continue;}
            if(errors++<24)std::cerr<<"case "<<n<<" bank "<<std::hex<<at<<" expected "<<expected_word<<" actual "<<actual_word<<std::dec<<'\n';}
    }
    if(in.peek()!=std::char_traits<char>::eof())throw std::runtime_error("trailing playback corpus");
    std::cout<<"{\"cases\":"<<count<<",\"ticks\":"<<ticks<<",\"exact_words\":"<<exact<<",\"errors\":"<<errors<<",\"allowed_ulp\":0,\"excluded_words\":"<<excluded<<",\"native_retained_words\":"<<retained<<"}\n";return errors?1:0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
