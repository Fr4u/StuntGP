#include "sgn/impact_particle_lifetime.hpp"
#include <bit>
#include <cstring>
#include <fstream>
#include <iostream>

namespace {
using namespace sgn;using namespace sgn::retail;using namespace contact;
using Record=std::array<std::uint8_t,0x8c00>;
unsigned word(const Record& b,unsigned at){unsigned value;std::memcpy(&value,b.data()+at,4);return value;}
void put(Record& b,unsigned at,unsigned value){std::memcpy(b.data()+at,&value,4);}
struct State {
    effects::ImpactParticles particles;effects::ImpactParticleDrawList draw;
    solver::Workspace work;ContactCastState query;VehicleRandom random;
    Vec3 gravity;VehicleTrailProjection projection;effects::ImpactTrailProfiles profiles=effects::initial_impact_trail_profiles();effects::ImpactParticleLifetimeAudit audit;unsigned profile_count=5;
};
void transfer(Record& b,State& s,bool read){
    const auto u=[&](unsigned& value,unsigned at){if(read)value=word(b,at);else put(b,at,value);};
    const auto f=[&](float& value,unsigned at){if(read)value=std::bit_cast<float>(word(b,at));else put(b,at,std::bit_cast<unsigned>(value));};
    const auto xyz=[&](auto& value,unsigned at){f(value.x,at);f(value.y,at+4);f(value.z,at+8);};
    const auto v=[&](Vector4& value,unsigned at){xyz(value,at);u(value.opaque,at+12);};
    static_assert(sizeof(effects::ImpactParticle)==0x440);
    if(read)std::memcpy(s.particles.particles.data(),b.data(),0x8800);else std::memcpy(b.data(),s.particles.particles.data(),0x8800);
    u(s.particles.emitted,0x8800);u(s.particles.cursor,0x8804);auto throttle=std::bit_cast<unsigned>(s.particles.throttle);u(throttle,0x8808);if(read)s.particles.throttle=std::bit_cast<std::int32_t>(throttle);
    u(s.particles.rate_selector,0x880c);u(s.random.state,0x8810);f(s.work.query_mass,0x8814);xyz(s.gravity,0x8818);
    const auto at=[](unsigned address){return 0x8840+address-0x4980c4;};auto& q=s.query;
    v(q.start,at(0x4980c4));v(q.end,at(0x4980d4));xyz(s.work.query_motion,at(0x4980e8));u(q.motion_payload,at(0x4980f4));
    v(q.step,at(0x498118));v(q.direction,at(0x498128));f(q.step_length,at(0x498148));f(q.travel_limit,at(0x49814c));
    f(q.remaining,at(0x498150));f(q.radius,at(0x498154));f(q.radius_squared,at(0x498158));
    if(read){const auto id=word(b,at(0x498164));q.polygon=id==0xffffffffu?std::nullopt:std::optional{id};}else put(b,at(0x498164),q.polygon.value_or(0xffffffffu));
    v(s.work.last_normal,at(0x498198));v(q.center,at(0x4981a8));v(q.surface,at(0x4981b8));u(q.hit_flag,at(0x4981e4));u(q.polygon_list,at(0x49823c));u(q.mode,0x8a70);
    auto& p=s.projection;for(unsigned j=0;j<16;++j)f(p.world_view[j],0x8a00+j*4);
    f(p.projection_x,0x8a40);f(p.projection_y,0x8a44);f(p.center_x,0x8a48);f(p.center_y,0x8a4c);f(p.scale_x,0x8a50);f(p.scale_y,0x8a54);f(p.clip_near,0x8a58);
    for(unsigned j=0;j<s.profile_count;++j){u(s.profiles[j].capacity,0x8a80+j*8);if(read)s.profiles[j].project_to_screen=word(b,0x8a84+j*8)!=0;else put(b,0x8a84+j*8,s.profiles[j].project_to_screen?1:0);}
    for(unsigned j=0;j<32;++j)u(s.draw.slots[j],0x8b00+j*4);u(s.draw.count,0x8b80);
    if(!read)put(b,0x8b84,s.audit.query.center?1:0);
}
}
int main(int argc,char** argv)try{
    if(argc!=3)throw std::runtime_error("expected impact lifetime corpus and mesh directory");std::ifstream file(argv[1],std::ios::binary);
    const auto read=[&](void* data,std::size_t size){if(!file.read(static_cast<char*>(data),static_cast<std::streamsize>(size)))throw std::runtime_error("short impact lifetime corpus");};
    const auto next=[&](){unsigned value;read(&value,4);return value;};char magic[8];read(magic,8);const std::string_view schema(magic,8);
    if(schema!="SGNIL001"&&schema!="SGNIL002")throw std::runtime_error("impact lifetime schema");
    const auto groups=next();if(!groups||groups>30)throw std::runtime_error("impact lifetime track limit");std::uint64_t exact=0,excluded=0;unsigned cases=0;
    for(unsigned g=0;g<groups;++g){const auto length=next();if(!length||length>64)throw std::runtime_error("impact lifetime name limit");std::string name(length,'\0');read(name.data(),length);
        if(name.find_first_of("/\\:")!=name.npos||name.find("..")!=name.npos)throw std::runtime_error("impact lifetime unsafe name");
        const auto track=decode_track_collision(read_file(std::filesystem::path(argv[2])/name));const auto scenarios=next(),steps=next();if(!scenarios||scenarios>1024||!steps||steps>64)throw std::runtime_error("impact lifetime scenario limit");
        for(unsigned n=0;n<scenarios;++n){State state;state.profile_count=schema=="SGNIL002"?9u:5u;Record actual{},previous{};
            for(unsigned step=0;step<steps;++step){Record input{},expected{};read(input.data(),input.size());read(expected.data(),expected.size());
                if(step==0){if(word(input,0x8b84))throw std::runtime_error("impact lifetime initial provenance");actual=input;transfer(actual,state,true);}
                else {if(input!=previous)throw std::runtime_error("impact lifetime broken source continuation");for(unsigned at=0;at<actual.size();at+=4)if(!(state.audit.query.center&&at==0x8930)&&word(actual,at)!=word(input,at))throw std::runtime_error("impact lifetime broken native continuation");}
                effects::update_impact_particles({state.particles,state.draw,state.work,state.query,state.random,track,state.gravity,state.projection,state.profiles,nullptr,&state.audit});transfer(actual,state,false);
                const auto mask=word(expected,0x8b84);if(mask>1||mask!=word(actual,0x8b84))throw std::runtime_error("impact lifetime mask mismatch");
                for(unsigned at=0;at<actual.size();at+=4){if(mask&&at==0x8930){++excluded;continue;}if(word(actual,at)!=word(expected,at))throw std::runtime_error("impact lifetime mismatch "+name+" scenario "+std::to_string(n)+" step "+std::to_string(step)+" offset "+std::to_string(at)+" actual "+std::to_string(word(actual,at))+" expected "+std::to_string(word(expected,at)));++exact;}
                previous=expected;++cases;
            }
        }
    }
    if(file.peek()!=std::char_traits<char>::eof())throw std::runtime_error("trailing impact lifetime corpus");
    std::cout<<"{\"tracks\":"<<groups<<",\"cases\":"<<cases<<",\"exact_words\":"<<exact<<",\"excluded_words\":"<<excluded<<",\"errors\":0,\"allowed_ulp\":0}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}
