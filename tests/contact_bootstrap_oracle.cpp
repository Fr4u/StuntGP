#include "sgn/contact_bootstrap.hpp"
#include "contact_solver_state.hpp"
#include <fstream>
#include <iostream>
#include <vector>

namespace {
using namespace sgn::retail;
using namespace sgn::retail::contact;
constexpr unsigned size=0xf800,camera_at=0xf000;
using Record=std::vector<std::uint8_t>;
unsigned word(const Record& b,unsigned at){unsigned value;std::memcpy(&value,b.data()+at,4);return value;}
float number(const Record& b,unsigned at){return std::bit_cast<float>(word(b,at));}
CameraVector vector(const Record& b,unsigned at){return {number(b,at),number(b,at+4),number(b,at+8),number(b,at+12)};}
void apply(Record& record,bool enable_audit){
    VehicleParameters p{};VehicleState v{};solver::State contact{};
    sgn::retail::oracle::transfer(record,p,v,true);contact::oracle::transfer_contact(record,contact,true);
    solver::VehiclePayloadAudit audit{};
    audit.position=true;audit.previous_position=true;audit.velocity=true;
    audit.body_position=true;audit.body_previous_position=true;audit.body_velocity=true;
    audit.orientation_first=true;audit.incremental_first=true;
    audit.body_orientation_first=true;audit.body_incremental_first=true;audit.snapshot_first=true;
    for(auto& probe:audit.probes)probe={true,true,true};
    solver::place_template_from_camera(v,contact,vector(record,camera_at+8),vector(record,camera_at+0x9c),enable_audit?&audit:nullptr);
    if(enable_audit){
        // Compare named metadata, not struct padding.
        if(audit.orientation_first||audit.incremental_first||audit.body_orientation_first||audit.body_incremental_first||
           !audit.position||!audit.previous_position||!audit.velocity||!audit.body_position||!audit.body_previous_position||!audit.body_velocity||!audit.snapshot_first)
            throw std::runtime_error("bootstrap matrix definedness transition");
        for(const auto& probe:audit.probes)if(!probe.current||!probe.previous||!probe.velocity)throw std::runtime_error("bootstrap changed opaque probe provenance");
    }
    sgn::retail::oracle::transfer(record,p,v,false);contact::oracle::transfer_contact(record,contact,false);
}
}
int main(int argc,char** argv){try{
    if(argc!=2)throw std::runtime_error("expected bootstrap corpus");
    std::ifstream file(argv[1],std::ios::binary);if(!file)throw std::runtime_error("cannot open bootstrap corpus");
    const auto read=[&](void* output,std::size_t count){file.read(static_cast<char*>(output),static_cast<std::streamsize>(count));if(!file)throw std::runtime_error("truncated bootstrap corpus");};
    const auto next=[&](){unsigned value;read(&value,4);return value;};
    char magic[8];read(magic,8);if(std::memcmp(magic,"SGNBT001",8))throw std::runtime_error("bootstrap corpus magic");
    const auto cases=next();if(cases==0||cases>10000)throw std::runtime_error("bootstrap case count");
    std::uint64_t words=0;
    for(unsigned trial=0;trial<cases;++trial){
        Record actual(size),expected(size);read(actual.data(),size);read(expected.data(),size);
        auto without_audit=actual;apply(actual,true);apply(without_audit,false);
        if(actual!=without_audit)throw std::runtime_error("bootstrap audit changed numeric state");
        for(unsigned at=0;at<size;at+=4){++words;if(word(actual,at)!=word(expected,at))
            throw std::runtime_error("bootstrap mismatch case "+std::to_string(trial)+" offset "+std::to_string(at)+" actual "+std::to_string(word(actual,at))+" expected "+std::to_string(word(expected,at)));}
    }
    if(file.peek()!=std::char_traits<char>::eof())throw std::runtime_error("trailing bootstrap corpus bytes");
    std::cout<<"{\"cases\":"<<cases<<",\"exact_words\":"<<words<<",\"errors\":0,\"allowed_ulp\":0,\"excluded_words\":0}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
