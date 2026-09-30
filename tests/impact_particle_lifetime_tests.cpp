#include "sgn/impact_particle_lifetime.hpp"
#include <bit>
#include <cstring>
#include <iostream>

namespace {
using namespace sgn;using namespace sgn::retail;using namespace contact;using namespace effects;
void require(bool ok){if(!ok)throw std::runtime_error("impact lifetime contract");}
struct State {
    ImpactParticles bank;ImpactParticleDrawList draw;solver::Workspace work;ContactCastState query;
    VehicleRandom random;TrackGeometry track;VehicleTrailProjection projection;
    ImpactTrailProfiles profiles=initial_impact_trail_profiles();solver::PayloadAudit payload;ImpactParticleLifetimeAudit audit;
    void run(bool diagnostic=true){update_impact_particles({bank,draw,work,query,random,track,{},projection,profiles,diagnostic?&payload:nullptr,diagnostic?&audit:nullptr});}
};
}
int main()try{
    unsigned checks=0;State s;
    const std::array<unsigned,9> capacity{7,16,5,9,3,12,12,48,3};
    for(unsigned j=0;j<9;++j){require(s.profiles[j].capacity==capacity[j]&&s.profiles[j].project_to_screen==((j>=2&&j<=4)||j==8));++checks;}
    s.bank.throttle=1;s.draw.count=9;s.draw.slots.fill(27);s.bank.particles[0].lifetime=1;
    s.run();require(s.bank.throttle==0&&s.draw.count==0&&s.draw.slots[0]==27&&s.bank.particles[0].lifetime==1);++checks;
    s.bank.emitted=1;s.bank.rate_selector=79;s.bank.throttle=std::bit_cast<std::int32_t>(0x80000000u);
    auto& p=s.bank.particles[0];p.position={1,2,3,0x12345678};p.velocity={4,5,6,0xabcdef01};p.decay=.5f;p.field_430=0xffffffff;p.field_434=0;
    s.payload.particle_position[0]=true;s.payload.particle_velocity[0]=true;s.work.query_mass=321;
    const auto seed=s.random.state;s.run();
    require(p.lifetime==0&&s.bank.emitted==0&&s.draw.count==1&&s.draw.slots[0]==0&&s.bank.throttle==0x7fffffff);++checks;
    require(p.position.x==5&&p.position.y==7&&p.position.z==9&&p.position.opaque==0x12345678);++checks;
    require(p.velocity.x==2&&p.velocity.y==2.5f&&p.velocity.z==3&&p.velocity.opaque==0xabcdef01);++checks;
    require(p.field_430==0&&p.field_434==1&&p.reserved[3]==0x12345678&&p.reserved[7*4+3]==0x12345678);++checks;
    require(s.audit.trail_position[0][0]&&s.audit.trail_position[0][7]&&!s.audit.trail_position[0][1]);++checks;
    require(s.random.state==seed&&s.work.query_mass==321);++checks;
    s.run();require(s.draw.count==0&&s.draw.slots[0]==0);++checks;
    const auto reject=[&](State candidate){const auto before=candidate.bank;const auto draw=candidate.draw;bool rejected=false;
        try{candidate.run();}catch(const FormatError&){rejected=true;}
        require(rejected&&std::memcmp(&before,&candidate.bank,sizeof before)==0&&draw.count==candidate.draw.count&&draw.slots==candidate.draw.slots);++checks;
    };
    s.bank.emitted=1;p.lifetime=3;
    for(const int type:{-1,9}){auto invalid=s;invalid.bank.particles[0].type=type;reject(invalid);}
    for(const unsigned count:{0u,64u}){auto invalid=s;invalid.profiles[0].capacity=count;reject(invalid);}
    p.type=2;p.position={2,4,7,0xaabbccdd};p.velocity={};p.initial_lifetime=0;s.projection.world_view=vehicle_identity();
    s.projection.projection_x=1;s.projection.projection_y=1;s.projection.scale_x=1;s.projection.scale_y=1;s.projection.clip_near=0;
    s.run();const auto at=p.field_430*4;require(std::bit_cast<float>(p.reserved[at])==.25f&&std::bit_cast<float>(p.reserved[at+1])==.5f&&std::bit_cast<float>(p.reserved[at+2])==4&&p.reserved[at+3]==0xaabbccdd);++checks;
    require(std::bit_cast<float>(p.reserved[5*4])==2&&std::bit_cast<float>(p.reserved[5*4+2])==7);++checks;
    p.initial_lifetime=-1;p.alpha=0;s.random.state=123;VehicleRandom expected=s.random;
    (void)expected.signed_value(3);(void)expected.signed_value(2);(void)expected.signed_value(3);s.run();require(s.random.state==expected.state);++checks;
    State plain=s;plain.run(false);s.run(true);require(std::memcmp(&plain.bank,&s.bank,sizeof s.bank)==0&&plain.draw.slots==s.draw.slots&&plain.random.state==s.random.state);++checks;
    std::cout<<"{\"checks\":"<<checks<<",\"errors\":0}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}
