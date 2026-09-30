#include "replay_fixture_cursor_codec.hpp"
#include "sgn/retail_replay.hpp"
#include "vehicle_oracle_state.hpp"
#include <bit>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>

namespace {
using namespace sgn::retail;
using Record=std::array<std::uint8_t,0x7c00>;
std::uint32_t u(const Record& b,unsigned at){std::uint32_t x;std::memcpy(&x,b.data()+at,4);return x;}
void put(Record& b,unsigned at,std::uint32_t x){std::memcpy(b.data()+at,&x,4);}
float f(const Record& b,unsigned at){return std::bit_cast<float>(u(b,at));}
std::optional<ReplayHandle> handle(std::uint32_t x) {
    if(x==0xffffffff)return std::nullopt;
    if(x>=6)throw std::runtime_error("bad replay identity in corpus");return ReplayHandle{x};
}
std::uint32_t identity(std::optional<ReplayHandle> h){return h?h->slot:0xffffffff;}
void initialize(ReplayBank& bank,const Record& b) {
    auto bytes=bank.bytes();std::fill(bytes.begin(),bytes.end(),static_cast<std::uint8_t>(u(b,0x94)));
    for(unsigned j=0;j<6;++j)std::memcpy(bytes.data()+j*ReplayBank::stride,b.data()+0x7900+j*0x80,0x80);
}
void apply(Record& b,ReplayBank& bank,unsigned entry) {
    ::sgn::retail::fixture::legacy_replay_cursor_words(bank,true);
    std::array<std::uint8_t,0x7800> raw{};std::memcpy(raw.data(),b.data()+0x100,raw.size());
    VehicleParameters p;VehicleState s;oracle::transfer(raw,p,s,true);
    std::array<VehicleRaceState,6> race{};race[0].recording=handle(u(b,0x100+0x7268));
    for(unsigned j=1;j<6;++j)race[j].recording=handle(u(b,0x20+(j-1)*4));
    ReplayWorkspace workspace{handle(u(b,0x40)),u(b,0x44),u(b,0x48)};
    ReplayObservation observed;
    for(unsigned j=0;j<4;++j){observed.wheel_angles[j]=f(b,0x50+j*4);observed.contact_loads[j]=f(b,0x60+j*4);}
    observed.surface_type=std::bit_cast<std::int32_t>(u(b,0x70));
    observed.effect_current=f(b,0x74);observed.effect_target=f(b,0x78);
    observed.effect_active=u(b,0x7c);observed.effect_pending=u(b,0x80);
    const ReplayHandle selected{u(b,0)};
    if(entry==0x4199a0)reset_replays(bank,race);
    else if(entry==0x41ab40)begin_recording(bank,selected,race[0]);
    else if(entry==0x41ab00)begin_playback(bank,selected,race[0],s);
    else if(entry==0x41aac0)stop_replay(bank,selected);
    else if(entry==0x41ac30) {
        const auto ticks=u(b,4);if(ticks==0 || ticks>10000)throw std::runtime_error("replay corpus tick count");
        for(unsigned tick=0;tick<ticks;++tick)record_vehicle(bank,race[0],s,observed,f(b,0x4c),workspace);
    }
    else throw std::runtime_error("unknown replay entry");
    oracle::transfer(raw,p,s,false);std::memcpy(b.data()+0x100,raw.data(),raw.size());
    put(b,0x100+0x7268,identity(race[0].recording));
    for(unsigned j=1;j<6;++j)put(b,0x20+(j-1)*4,identity(race[j].recording));
    put(b,0x40,identity(workspace.recording));put(b,0x44,workspace.frame_offset);put(b,0x48,workspace.previous_frame_offset);
    ::sgn::retail::fixture::legacy_replay_cursor_words(bank,false);
    for(unsigned j=0;j<6;++j)std::memcpy(b.data()+0x7900+j*0x80,bank.bytes().data()+j*ReplayBank::stride,0x80);
}
void read(std::ifstream& in,void* out,std::size_t n){if(!in.read(static_cast<char*>(out),static_cast<std::streamsize>(n)))throw std::runtime_error("short replay corpus");}
std::uint32_t word(std::ifstream& in){std::uint32_t x;read(in,&x,4);return x;}
}
int main(int argc,char** argv) {
    try {
        if(argc!=2)throw std::runtime_error("expected replay corpus");std::ifstream in(argv[1],std::ios::binary);
        char magic[8];read(in,magic,8);if(std::string(magic,8)!="SGNRP001")throw std::runtime_error("replay corpus schema");
        const auto count=word(in);if(count==0 || count>20000)throw std::runtime_error("replay corpus count");
        std::uint64_t errors=0,exact=0;
        for(unsigned n=0;n<count;++n) {
            const auto entry=word(in);Record actual{},expected{};
            read(in,actual.data(),actual.size());read(in,expected.data(),expected.size());
            ReplayBank bank,reference;initialize(bank,actual);initialize(reference,actual);
            const auto changes=word(in);if(changes>ReplayBank::size)throw std::runtime_error("replay delta count");
            std::uint32_t previous=0;
            for(unsigned j=0;j<changes;++j) {
                const auto at=word(in);std::uint8_t value;read(in,&value,1);
                if(at>=ReplayBank::size || (j && at<=previous))throw std::runtime_error("bad replay delta order");
                reference.bytes()[at]=value;previous=at;
            }
            apply(actual,bank,entry);
            for(unsigned at=0;at<actual.size();++at) {
                ++exact;if(actual[at]!=expected[at]) {
                    if(errors++<24)std::cerr<<"entry "<<std::hex<<entry<<" case "<<std::dec<<n<<" record offset "<<std::hex<<at<<" actual "<<unsigned(actual[at])<<" expected "<<unsigned(expected[at])<<'\n';
                }
            }
            for(unsigned at=0;at<ReplayBank::size;++at) {
                ++exact;if(bank.bytes()[at]!=reference.bytes()[at]) {
                    if(errors++<24)std::cerr<<"entry "<<std::hex<<entry<<" case "<<std::dec<<n<<" bank offset "<<std::hex<<at<<" actual "<<unsigned(bank.bytes()[at])<<" expected "<<unsigned(reference.bytes()[at])<<'\n';
                }
            }
        }
        if(in.peek()!=std::char_traits<char>::eof())throw std::runtime_error("trailing replay corpus bytes");
        std::cout<<"{\"cases\":"<<count<<",\"exact_bytes\":"<<exact<<",\"errors\":"<<errors<<",\"excluded_bytes\":0}\n";
        return errors?1:0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}
}
