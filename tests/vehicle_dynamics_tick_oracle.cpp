#include "vehicle_air_fixture.hpp"
#include "sgn/vehicle_dynamics_tick.hpp"
#include "retail_race_state.hpp"
#include <fstream>
#include <iostream>

namespace {
using namespace sgn::retail;
constexpr unsigned cars_at=air_oracle::size,car_size=0x7800,context_at=cars_at+6*car_size,record_size=context_at+0x100;
using Record=std::vector<std::uint8_t>;
unsigned word(const Record& b,unsigned at){unsigned value;std::memcpy(&value,b.data()+at,4);return value;}
float number(const Record& b,unsigned at){return std::bit_cast<float>(word(b,at));}
void put(Record& b,unsigned at,unsigned value){std::memcpy(b.data()+at,&value,4);}
VehicleVector vector(const Record& b,unsigned at){return {number(b,at),number(b,at+4),number(b,at+8),number(b,at+12)};}
std::vector<unsigned> apply(Record& b,bool source_modifier) {
    if(!source_modifier && word(b,air_oracle::vehicle_at+0x2640)!=0)throw std::runtime_error("stock dynamics corpus requires optional drag flag zero");
    if(source_modifier && (!std::isfinite(number(b,context_at+0x40)) || !std::isfinite(number(b,air_oracle::vehicle_at+0x2660))))
        throw std::runtime_error("full dynamics modifier inputs must be finite");
    air_oracle::Record common;std::copy_n(b.begin(),common.size(),common.begin());air_oracle::Fixture fixture(common);
    const auto current=air_oracle::u(common,0x34);const auto at=cars_at+current*car_size;
    if(!std::equal(common.begin()+air_oracle::vehicle_at,common.begin()+air_oracle::vehicle_at+car_size,b.begin()+at))
        throw std::runtime_error("dynamics duplicate current record differs");
    std::array<VehicleParameters,6> parameters;std::array<VehicleState,6> other;std::array<VehicleRaceState,6> races;
    for(unsigned j=0;j<6;++j){auto bytes=std::span(b).subspan(cars_at+j*car_size,car_size);oracle::transfer(bytes,parameters[j],other[j],true);race_oracle::transfer(bytes,races[j],true);}
    std::array<VehicleWakeVehicle,6> world={{{current==0?fixture.physics:other[0],races[0]},{current==1?fixture.physics:other[1],races[1]},
        {current==2?fixture.physics:other[2],races[2]},{current==3?fixture.physics:other[3],races[3]},
        {current==4?fixture.physics:other[4],races[4]},{current==5?fixture.physics:other[5],races[5]}}};
    VehicleSupport support;support.body_position=vector(b,at+0x7270);
    for(unsigned j=0;j<4;++j){support.local[j]=vector(b,at+0x73b4+j*0x4c);support.current[j]=vector(b,at+0x73c4+j*0x4c);}
    VehiclePositionHistory history;history.ticks=word(b,at+0x1fcc);
    for(unsigned j=0;j<8;++j){history.samples[j].position=vector(b,at+0x1f2c+j*20);history.samples[j].radius=number(b,at+0x1f3c+j*20);}
    const auto controller_vehicle=fixture.controls.controllers[fixture.controls.current].vehicle;
    const auto player=cars_at+controller_vehicle*car_size;
    const EngineContext engine{fixture.stunt.game_mode,word(b,player+0x767c)==1 || word(b,player+0x7688)<61,number(b,at+0x2a28),number(b,at+0x2a2c)};
    const VehicleDynamicsContext dynamics{engine,number(b,at+0x2a08),number(b,at+0x2a44),
        {number(b,at+0x2a38),number(b,at+0x2a3c),number(b,at+0x2a40)},0,fixture.stunt.tick};
    VehicleDynamicsWorkspace workspace;for(unsigned j=0;j<16;++j)workspace.steering[j]=number(b,context_at+j*4);
    const auto air=fixture.context();const VehicleDynamicsTickContext context{dynamics,workspace,world,current,air,
        source_modifier?number(b,context_at+0x40):0.0f};
    const auto observed=vehicle_dynamics_tick({fixture.parameters,fixture.physics,support,fixture.air,history},context);
    fixture.save();
    for(unsigned j=0;j<8;++j){const auto& sample=history.samples[j];const std::array<float,5> fields={sample.position.x,sample.position.y,sample.position.z,sample.position.w,sample.radius};
        std::memcpy(common.data()+air_oracle::vehicle_at+0x1f2c+j*20,fields.data(),20);}
    air_oracle::put(common,air_oracle::vehicle_at+0x1fcc,history.ticks);
    std::copy(common.begin(),common.end(),b.begin());
    for(unsigned j=0;j<6;++j){auto bytes=std::span(b).subspan(cars_at+j*car_size,car_size);
        if(j==current)std::copy_n(common.begin()+air_oracle::vehicle_at,car_size,bytes.begin());
        else {oracle::transfer(bytes,parameters[j],other[j],false);race_oracle::transfer(bytes,races[j],false);}}
    for(unsigned j=0;j<16;++j)put(b,context_at+j*4,std::bit_cast<unsigned>(workspace.steering[j]));
    auto mask=fixture.excluded(observed);const auto original_count=mask.size();
    for(std::size_t j=0;j<original_count;++j)if(mask[j]>=air_oracle::vehicle_at && mask[j]<air_oracle::vehicle_at+car_size)
        mask.push_back(at+mask[j]-air_oracle::vehicle_at);
    std::sort(mask.begin(),mask.end());return mask;
}
}
int main(int argc,char** argv){try {
    if(argc!=2)throw std::runtime_error("expected full dynamics corpus");std::ifstream in(argv[1],std::ios::binary);
    const auto read=[&](void* output,std::size_t bytes){if(!in.read(static_cast<char*>(output),static_cast<std::streamsize>(bytes)))throw std::runtime_error("short full dynamics corpus");};
    const auto next=[&](){unsigned value;read(&value,4);return value;};
    char magic[8];read(magic,8);const auto schema=std::string(magic,8);
    if(schema!="SGNDT001" && schema!="SGNDT002")throw std::runtime_error("full dynamics schema");
    const bool source_modifier=schema=="SGNDT002";
    const auto count=next();if(count==0 || count>20000)throw std::runtime_error("full dynamics count");
    std::uint64_t errors=0,exact=0,excluded=0,retained=0;
    for(unsigned n=0;n<count;++n){
        if(next()!=0x40fc90)throw std::runtime_error("unknown full dynamics entry");
        Record before(record_size),expected(record_size);read(before.data(),before.size());read(expected.data(),expected.size());
        const auto mask_count=next();if(mask_count>456)throw std::runtime_error("full dynamics mask count");
        std::vector<unsigned> supplied(mask_count);for(auto& at:supplied)at=next();
        auto actual=before;const auto mask=apply(actual,source_modifier);if(mask!=supplied)throw std::runtime_error("full dynamics mask differs case "+std::to_string(n));
        excluded+=mask.size();const auto current=word(before,0x34),current_at=cars_at+current*car_size;
        for(unsigned at:mask){auto previous=at;
            if(at==air_oracle::vehicle_at+0x1bd8)previous=air_oracle::vehicle_at+0x1cc8;
            if(at==current_at+0x1bd8)previous=current_at+0x1cc8;
            ++retained;if(word(actual,at)!=word(before,previous))throw std::runtime_error("full dynamics native opaque retention case "+std::to_string(n)+" offset "+std::to_string(at));}
        for(unsigned at=0;at<record_size;at+=4){if(std::binary_search(mask.begin(),mask.end(),at))continue;
            if(word(actual,at)==word(expected,at)){++exact;continue;}
            if(errors++<24)std::cerr<<"case "<<n<<" offset "<<std::hex<<at<<" expected "<<word(expected,at)<<" actual "<<word(actual,at)<<std::dec<<'\n';}
    }
    if(in.peek()!=std::char_traits<char>::eof())throw std::runtime_error("trailing full dynamics corpus");
    std::cout<<"{\"cases\":"<<count<<",\"exact_words\":"<<exact<<",\"errors\":"<<errors<<",\"allowed_ulp\":0,\"excluded_words\":"<<excluded<<",\"native_retained_words\":"<<retained<<"}\n";return errors?1:0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
