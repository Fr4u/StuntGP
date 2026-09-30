#include "sgn/vehicle_score.hpp"
#include "sgn/vehicle_stunt_awards.hpp"
#include "sgn/vehicle_stunt_tables.hpp"
#include "vehicle_oracle_state.hpp"
#include <bit>
#include <cstring>
#include <fstream>
#include <iostream>
#include <type_traits>
namespace {
using namespace sgn::retail;
using Record=std::array<std::uint8_t,0xd000>;
static_assert(sizeof(VehicleScorePopup)==0x138 && std::is_trivially_copyable_v<VehicleScorePopup>);
std::uint32_t u(const Record& b,unsigned at){std::uint32_t x;std::memcpy(&x,b.data()+at,4);return x;}
void put(Record& b,unsigned at,std::uint32_t x){std::memcpy(b.data()+at,&x,4);}
float f(const Record& b,unsigned at){return std::bit_cast<float>(u(b,at));}
std::int32_t i(const Record& b,unsigned at){return std::bit_cast<std::int32_t>(u(b,at));}
struct Audio final:VehicleScoreAudio,VehicleStuntChainAudio {
    Record& record;
    explicit Audio(Record& b):record(b){}
    void request(std::uint32_t id,std::uint32_t volume,std::uint32_t pan,std::uint32_t force) {
        const auto count=u(record,0x14c);if(count>=8)throw std::runtime_error("too many stunt audio requests");
        const std::array<std::uint32_t,6> values={id,0,volume,pan,force,0};
        for(unsigned j=0;j<6;++j)put(record,0x1700+count*24+j*4,values[j]);
        put(record,0x14c,count+1);put(record,0x150,id);put(record,0x154,volume);
    }
    void play(std::uint32_t id,std::uint32_t volume) override {
        request(id,volume,255,0);
    }
    void play_chain(std::uint32_t id) override {request(id,255,0,1);}
};
void apply(Record& b,unsigned entry) {
    std::array<std::uint8_t,0x7800> vehicle_record{};std::memcpy(vehicle_record.data(),b.data()+0x5000,vehicle_record.size());
    VehicleParameters parameters;VehicleState s;oracle::transfer(vehicle_record,parameters,s,true);
    VehicleScoreState score;std::memcpy(score.popups.data(),b.data()+0x200,sizeof(score.popups));
    score.sound_class=u(b,0x140);score.sound_id=u(b,0x144);score.sound_volume=u(b,0x148);
    contact::player::Controls controls;controls.current=u(b,0x38);
    for(unsigned j=0;j<4;++j) {
        auto& c=controls.controllers[j];const auto at=0x2000+j*0x74c;
        c.active=i(b,at);c.vehicle=u(b,at+0xac);std::memcpy(c.notices.data(),b.data()+at+0x4ac,sizeof(c.notices));
    }
    const auto* text=reinterpret_cast<const char*>(b.data()+0x40);unsigned length=0;
    while(length<64 && text[length])++length;if(length==64)throw std::runtime_error("score notice unterminated");
    Audio audio(b);const VehicleStuntContext context{i(b,0),i(b,4),i(b,8),i(b,12),u(b,0x24),u(b,0x28),{text,length},audio};
    std::array<std::uint32_t,33> point_values{};for(unsigned j=0;j<33;++j)point_values[j]=u(b,0x1600+j*4);
    VehicleStuntRewards rewards;rewards.points=point_values;rewards.normal_energy=f(b,0x190);rewards.upgraded_energy=f(b,0x194);
    std::array<std::string,93> messages;
    for(unsigned j=0;j<messages.size();++j){messages[j]="STUNT "+std::to_string(j);rewards.messages[j]=messages[j];}
    VehicleRandom random{u(b,0x12c)};
    if(entry==0x401000)clear_score_popups(score);
    else if(entry==0x401220)award_stunt_score(s,controls,u(b,0x34),score,context,u(b,0x2c),u(b,0x30),u(b,0x28));
    else if(entry==0x411340)update_inversion_state(s,controls,u(b,0x34),score,context,{f(b,0x120),f(b,0x124),f(b,0x128)});
    else if(entry==0x4115e0)award_stunt_height(s,controls,u(b,0x34),score,context,point_values,f(b,0x18));
    else if(entry==0x411680)update_stunt_height(s,controls,u(b,0x34),score,context,point_values);
    else if(entry==0x42f1f0)put(b,0x198,std::bit_cast<std::uint32_t>(vehicle_random_integer(random,i(b,0x1c))));
    else if(entry==0x411da0)put(b,0x198,stunt_chain_points(u(b,0x1c),u(b,0x20)));
    else if(entry==0x4190b0)reward_stunt_energy(s,u(b,0x1c));
    else if(entry==0x411de0)award_stunt_pattern(s,controls,u(b,0x34),score,context,rewards,random,audio,stunt_tables::patterns.at(u(b,0x1c)),u(b,0x20));
    else if(entry==0x411f10)recognize_stunt(s,controls,u(b,0x34),score,context,rewards,random,audio);
    else if(entry==0x412040)complete_stunt_landing(s,controls,u(b,0x34),score,context,rewards,random,audio);
    else if(entry==0x4120f0)update_stunt_landing(s,controls,u(b,0x34),score,context,rewards,random,audio);
    else throw std::runtime_error("unknown score entry");
    put(b,0x100,std::bit_cast<std::uint32_t>(s.score));put(b,0x10c,s.stunt.inversion_latch);put(b,0x110,s.stunt.inversion_timer);
    put(b,0x134,std::bit_cast<std::uint32_t>(s.stunt.maximum_height));put(b,0x13c,s.stunt.height_level);
    put(b,0x12c,random.state);oracle::transfer(vehicle_record,parameters,s,false);
    std::memcpy(b.data()+0x5000,vehicle_record.data(),vehicle_record.size());
    std::memcpy(b.data()+0x200,score.popups.data(),sizeof(score.popups));
    put(b,0x140,score.sound_class);put(b,0x144,score.sound_id);put(b,0x148,score.sound_volume);put(b,0x38,controls.current);
    for(unsigned j=0;j<4;++j)std::memcpy(b.data()+0x2000+j*0x74c+0x4ac,controls.controllers[j].notices.data(),sizeof(controls.controllers[j].notices));
}
void read(std::ifstream& in,void* out,std::size_t bytes) {
    if(!in.read(static_cast<char*>(out),static_cast<std::streamsize>(bytes)))throw std::runtime_error("short score corpus");
}
unsigned word(std::ifstream& in){unsigned value;read(in,&value,4);return value;}
}
int main(int argc,char** argv) {
    try {
        if(argc!=2)throw std::runtime_error("expected score corpus");std::ifstream in(argv[1],std::ios::binary);
        char magic[8];read(in,magic,8);if(std::string(magic,8)!="SGNSC002")throw std::runtime_error("score corpus schema");
        const auto count=word(in);if(count==0 || count>20000)throw std::runtime_error("score corpus count");
        unsigned errors=0;std::uint64_t exact=0;
        for(unsigned n=0;n<count;++n) {
            const auto entry=word(in);Record actual{},expected{};read(in,actual.data(),actual.size());read(in,expected.data(),expected.size());
            apply(actual,entry);
            for(unsigned at=0;at<actual.size();at+=4) {
                if(u(actual,at)==u(expected,at)){++exact;continue;}
                if(errors++<24)std::cerr<<"case "<<n<<" entry "<<std::hex<<entry<<" offset "<<at<<" expected "<<u(expected,at)<<" actual "<<u(actual,at)<<std::dec<<'\n';
            }
        }
        if(in.peek()!=std::char_traits<char>::eof())throw std::runtime_error("trailing score corpus");
        std::cout<<"{\"cases\":"<<count<<",\"exact_words\":"<<exact<<",\"errors\":"<<errors<<",\"allowed_ulp\":0,\"excluded_words\":0}\n";
        return errors?1:0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
