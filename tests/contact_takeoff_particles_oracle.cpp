#include "world_template_fixture.hpp"
#include "contact_takeoff_state.hpp"
#include "sgn/vehicle.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>

namespace {
using namespace sgn::retail;
using namespace sgn::retail::contact;
constexpr unsigned size=0x3500,bank_at=0x100;
using Record=std::vector<std::uint8_t>;
unsigned word(const Record& b,unsigned at){unsigned value;std::memcpy(&value,b.data()+at,4);return value;}
void word(Record& b,unsigned at,unsigned value){std::memcpy(b.data()+at,&value,4);}
float number(const Record& b,unsigned at){return std::bit_cast<float>(word(b,at));}
void load(Record& b,VehicleState& vehicle,VehicleRandom& random,effects::EffectParticleBank& bank){
    vehicle.position={number(b,0),number(b,4),number(b,8),0};
    vehicle.velocity={number(b,12),number(b,16),number(b,20)};
    vehicle.up={number(b,24),number(b,28),number(b,32)};
    random.state=word(b,36);bank.active_signal=word(b,40);
    takeoff_oracle::transfer_bank(std::span(b).subspan(bank_at),bank,true);
}
void save(Record& b,VehicleRandom& random,effects::EffectParticleBank& bank){
    word(b,36,random.state);word(b,40,bank.active_signal);
    takeoff_oracle::transfer_bank(std::span(b).subspan(bank_at),bank,false);
}
}
int main(int argc,char** argv){try{
    if(argc!=2)throw std::runtime_error("expected particle corpus");
    std::ifstream file(argv[1],std::ios::binary);if(!file)throw std::runtime_error("cannot open particle corpus");
    const auto read=[&](void* output,std::size_t count){file.read(static_cast<char*>(output),static_cast<std::streamsize>(count));if(!file)throw std::runtime_error("truncated particle corpus");};
    const auto next=[&](){unsigned value;read(&value,4);return value;};
    char magic[8];read(magic,8);if(std::memcmp(magic,"SGNTP001",8))throw std::runtime_error("particle corpus magic");
    const auto cases=next();if(cases==0||cases>10000)throw std::runtime_error("particle case count");
    VehicleState vehicle{};VehicleRandom random{};effects::EffectParticleBank bank{};
    Record previous_expected,actual;std::uint64_t words=0;
    for(unsigned trial=0;trial<cases;++trial){
        const auto continuation=next();if(continuation>1||(trial==0&&continuation))throw std::runtime_error("particle continuation flag");
        Record input(size),expected(size);read(input.data(),size);read(expected.data(),size);
        const auto mask_count=next();if(mask_count>64*7)throw std::runtime_error("particle mask size");
        std::vector<unsigned> supplied(mask_count);for(auto& offset:supplied)offset=next();
        if(continuation){if(input!=previous_expected)throw std::runtime_error("particle continuation input changed");}
        else {bank={};load(input,vehicle,random,bank);actual=input;}
        effects::emit_takeoff_particles(vehicle,random,bank,sgn::retail::fixture::cold_world_template);save(actual,random,bank);
        const auto mask=takeoff_oracle::excluded(bank,bank_at);
        if(mask!=supplied)throw std::runtime_error("particle dynamic mask differs at case "+std::to_string(trial));
        for(unsigned at=0;at<size;at+=4){if(std::binary_search(mask.begin(),mask.end(),at))continue;++words;
            if(word(actual,at)!=word(expected,at))throw std::runtime_error("particle mismatch case "+std::to_string(trial)+" offset "+std::to_string(at)+" actual "+std::to_string(word(actual,at))+" expected "+std::to_string(word(expected,at)));}
        previous_expected=std::move(expected);
    }
    if(file.peek()!=std::char_traits<char>::eof())throw std::runtime_error("trailing particle corpus bytes");
    std::cout<<"{\"cases\":"<<cases<<",\"exact_words\":"<<words<<",\"errors\":0,\"allowed_ulp\":0}\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
