#include "contact_solver_state.hpp"
#include "sgn/contact_cast.hpp"
#include "contact_sphere_cast_fixture.hpp"
#include "contact_payload_state.hpp"
#include "sgn/vehicle_score.hpp"
#include <fstream>
#include <iostream>
#include <memory>
#include <span>
#include <vector>

namespace {
using namespace sgn::retail;
using namespace sgn::retail::contact;
constexpr unsigned total_size=0x3c000,car_size=0x7800,controls_at=0x2d000,particles_at=0x30000,score_at=0x39000,params_at=0x3b000,work_at=0x3b400;
using Record=std::vector<std::uint8_t>;
std::uint32_t u(const Record& b,unsigned at){std::uint32_t value;std::memcpy(&value,b.data()+at,4);return value;}
void u(Record& b,unsigned at,std::uint32_t value){std::memcpy(b.data()+at,&value,4);}
float f(const Record& b,unsigned at){return std::bit_cast<float>(u(b,at));}
std::int32_t i(const Record& b,unsigned at){return std::bit_cast<std::int32_t>(u(b,at));}
bool permitted_opaque_word(unsigned at){
    if(at>=total_size||at%4!=0)return false;
    if(at==work_at+0x2c||at==work_at+0x5c)return true;
    if(at>=particles_at&&at<particles_at+0x8800){const auto lane=(at-particles_at)%0x440;return lane==0x14||lane==0x24;}
    if(at>=6*car_size)return false;
    const auto offset=at%car_size;
    switch(offset){case 0x1bb8:case 0x1bc8:case 0x1c98:case 0x1bd8:case 0x1c18:
        case 0x727c:case 0x728c:case 0x72ac:case 0x72c4:case 0x7304:case 0x7388:return true;default:break;}
    if(offset>=0x73b4&&offset<0x73b4+6*0x4c){const auto lane=(offset-0x73b4)%0x4c;return lane==28||lane==44||lane==60;}
    return offset>=0x2038&&offset<=0x2038+3*0x98&&(offset-0x2038)%0x98==0;
}
void verify_state_ownership(){
    VehicleParameters parameters{};VehicleState vehicle{};VehicleRaceState race{};solver::State contact{};
    std::array<Record,3> outputs{Record(car_size,0xa5),Record(car_size,0xa5),Record(car_size,0xa5)};
    sgn::retail::oracle::transfer(outputs[0],parameters,vehicle,false);
    sgn::retail::race_oracle::transfer(outputs[1],race,false);
    contact::oracle::transfer_contact(outputs[2],contact,false);
    for(unsigned at=0;at<car_size;++at){unsigned owners=0;for(const auto& data:outputs)owners+=data[at]!=0xa5;
        if(owners>1)throw std::runtime_error("overlapping physical/race/contact state at byte "+std::to_string(at));}
}
struct Audio final:VehicleScoreAudio {
    Record& record;explicit Audio(Record& b):record(b){}
    void play(std::uint32_t id,std::uint32_t volume)override{u(record,work_at+0xe0,u(record,work_at+0xe0)+1);u(record,work_at+0xe4,id);u(record,work_at+0xe8,volume);}
};
struct Inversion final:surface::InvertedContact {
    VehicleScoreState& score;const VehicleStuntContext& context;sgn::Vec3 gravity;
    Inversion(VehicleScoreState& s,const VehicleStuntContext& c,sgn::Vec3 g):score(s),context(c),gravity(g){}
    void apply(VehicleState& state,player::Controls& controls,player::VehicleId id)override{update_inversion_state(state,controls,id,score,context,gravity);}
};
void apply(Record& b,unsigned entry,const TrackGeometry& geometry,solver::PayloadAudit* audit,sphere_test::State* shared_query=nullptr){
    auto parameters=std::make_unique<std::array<VehicleParameters,6>>();auto states=std::make_unique<std::array<VehicleState,6>>();auto contacts=std::make_unique<std::array<solver::State,6>>();
    std::array<VehicleRaceState,6> races{};
    const auto transfer=[&](bool reading){for(unsigned j=0;j<6;++j){std::span bytes(b.data()+j*car_size,car_size);contact::oracle::transfer(bytes,(*parameters)[j],(*states)[j],races[j],(*contacts)[j],reading);}};
    transfer(true);
    std::array<solver::Vehicle,6> cars{{{(*parameters)[0],(*states)[0],races[0],(*contacts)[0],0},{(*parameters)[1],(*states)[1],races[1],(*contacts)[1],1},
        {(*parameters)[2],(*states)[2],races[2],(*contacts)[2],2},{(*parameters)[3],(*states)[3],races[3],(*contacts)[3],3},{(*parameters)[4],(*states)[4],races[4],(*contacts)[4],4},{(*parameters)[5],(*states)[5],races[5],(*contacts)[5],5}}};
    player::Controls controls;controls.current=u(b,work_at+0xf0);
    const auto controller_transfer=[&](bool read){for(unsigned j=0;j<4;++j){auto& c=controls.controllers[j];auto at=controls_at+j*0x74c;
        auto field=[&](std::int32_t& value,unsigned off){if(read)value=i(b,at+off);else u(b,at+off,std::bit_cast<std::uint32_t>(value));};
        auto vec=[&](CameraVector& v,unsigned off){if(read)v={f(b,at+off),f(b,at+off+4),f(b,at+off+8),f(b,at+off+12)};else{u(b,at+off,std::bit_cast<std::uint32_t>(v.x));u(b,at+off+4,std::bit_cast<std::uint32_t>(v.y));u(b,at+off+8,std::bit_cast<std::uint32_t>(v.z));u(b,at+off+12,std::bit_cast<std::uint32_t>(v.w));}};
        field(c.active,0);if(read)c.camera.mode=u(b,at+4);else u(b,at+4,c.camera.mode);
        vec(c.camera.position,8);vec(c.camera.velocity,0xc4);vec(c.camera.angular_velocity,0xd8);
        if(read){c.vehicle=u(b,at+0xac);c.respawn_transition.state=u(b,at+0x300);c.respawn_transition.phase=u(b,at+0x308);std::memcpy(c.notices.data(),b.data()+at+0x4ac,sizeof(c.notices));}
        else{u(b,at+0xac,c.vehicle);u(b,at+0x300,c.respawn_transition.state);u(b,at+0x308,c.respawn_transition.phase);std::memcpy(b.data()+at+0x4ac,c.notices.data(),sizeof(c.notices));}
    }};
    controller_transfer(true);
    auto particles=std::make_unique<effects::ImpactParticles>();static_assert(sizeof(effects::ImpactParticle)==0x440);std::memcpy(particles->particles.data(),b.data()+particles_at,0x8800);
    particles->emitted=u(b,work_at+0xc0);particles->cursor=u(b,work_at+0xc4);particles->rate_selector=u(b,work_at+0xc8);particles->throttle=i(b,work_at+0xcc);VehicleRandom random{u(b,work_at+0xd0)};
    VehicleScoreState score;static_assert(sizeof(VehicleScorePopup)==0x138);std::memcpy(score.popups.data(),b.data()+score_at,0x1380);
    score.sound_class=u(b,work_at+0xd4);score.sound_id=u(b,work_at+0xd8);score.sound_volume=u(b,work_at+0xdc);
    Audio audio(b);const auto* text=reinterpret_cast<const char*>(b.data()+params_at+0x40);unsigned length=0;while(length<64&&text[length])++length;if(length==64)throw std::runtime_error("solver notice terminator");
    VehicleStuntContext stunt{i(b,params_at),i(b,params_at+4),i(b,params_at+0x28),i(b,params_at+0x2c),u(b,params_at+0x30),u(b,params_at+0x34),{text,length},audio};
    Inversion inverted(score,stunt,{f(b,params_at+0x80),f(b,params_at+0x84),f(b,params_at+0x88)});
    solver::Parameters config;config.game_mode=i(b,params_at);config.race_mode=i(b,params_at+4);config.track_mode=i(b,params_at+8);config.gravity=f(b,params_at+12);
    config.energy={f(b,params_at+0x10),f(b,params_at+0x14),f(b,params_at+0x18),f(b,params_at+0x1c),f(b,params_at+0x20),f(b,params_at+0x24)};
    for(unsigned j=0;j<128;++j)config.restitution[j]=f(b,params_at+0x100+j*4);
    solver::Workspace work;
    const auto workspace_transfer=[&](bool read){
        auto field=[&](float& v,unsigned off){if(read)v=f(b,work_at+off);else u(b,work_at+off,std::bit_cast<std::uint32_t>(v));};
        field(work.inverse_probe_count,0x30);field(work.impact.velocity_delta.x,0x40);field(work.impact.velocity_delta.y,0x44);field(work.impact.velocity_delta.z,0x48);
        field(work.query_motion.x,0x10);field(work.query_motion.y,0x14);field(work.query_motion.z,0x18);
        for(unsigned j=0;j<16;++j)field(work.impact.rotation_delta[j],0x50+j*4);
        field(work.impact.normal_speed,0x90);field(work.response.alignment,0x9c);field(work.response.contact_load,0xa0);
        field(work.last_normal.x,0x20);field(work.last_normal.y,0x24);field(work.last_normal.z,0x28);
        if(read){work.last_normal.opaque=u(b,work_at+0x2c);work.impact.touched=u(b,work_at+0x98)!=0;work.response.body_touched=i(b,work_at+0xa4);if(u(b,work_at+0xa8)!=0xffffffff)work.selected_partner=u(b,work_at+0xa8);work.average_payload=u(b,work_at+0xb0);}
        else{u(b,work_at+0x2c,work.last_normal.opaque);u(b,work_at+0x98,work.impact.touched);u(b,work_at+0xa4,std::bit_cast<std::uint32_t>(work.response.body_touched));u(b,work_at+0xa8,work.selected_partner.value_or(0xffffffff));u(b,work_at+0xb0,work.average_payload);}
    };
    workspace_transfer(true);
    ContactCastState local_query;auto& query=shared_query?shared_query->query:local_query;
    if(shared_query)work.query_mass=shared_query->work.query_mass;
    solver::World world{cars,geometry,config,controls,*particles,random,inverted,work,query,audit,
        shared_query?&shared_query->audit:nullptr,shared_query?&shared_query->internals_audit:nullptr};
    const auto index=u(b,work_at);if(index>=6)throw std::runtime_error("solver fixture index");
    const Material material{b[work_at+8],b[work_at+9],b[work_at+10],b[work_at+11]};
    if(entry==0x4085e0)solver::initialize(cars[index].contact,u(b,work_at+4));
    else if(entry==0x4093e0)solver::impact(world,index,u(b,work_at+4),work.last_normal,material,work.query_motion);
    else if(entry==0x408fc0)surface::process_material(cars[index].state,cars[index].race,cars[index].contact.surface,controls,index,material,work.response.alignment,config.race_mode,inverted);
    else if(entry==0x4098a0)solver::collide_pair(world,index,u(b,work_at+4));
    else if(entry==0x409b60)u(b,work_at+0x34,solver::step(world,index));
    else throw std::runtime_error("solver entry");
    if(shared_query){shared_query->work=work;if(audit)shared_query->payload.normal=audit->normal;}
    transfer(false);controller_transfer(false);workspace_transfer(false);
    std::memcpy(b.data()+particles_at,particles->particles.data(),0x8800);u(b,work_at+0xc0,particles->emitted);u(b,work_at+0xc4,particles->cursor);u(b,work_at+0xc8,particles->rate_selector);u(b,work_at+0xcc,std::bit_cast<std::uint32_t>(particles->throttle));u(b,work_at+0xd0,random.state);
    std::memcpy(b.data()+score_at,score.popups.data(),0x1380);u(b,work_at+0xd4,score.sound_class);u(b,work_at+0xd8,score.sound_id);u(b,work_at+0xdc,score.sound_volume);u(b,work_at+0xf0,controls.current);
}
void read(std::ifstream& in,void* p,std::size_t n){if(!in.read(static_cast<char*>(p),static_cast<std::streamsize>(n)))throw std::runtime_error("short solver corpus");}
unsigned read_u(std::ifstream& in){unsigned v;read(in,&v,4);return v;}
}
int main(int argc,char** argv){
    try{
        if(argc!=3)throw std::runtime_error("expected corpus and track PMD");verify_state_ownership();const auto geometry=decode_track_collision(sgn::read_file(argv[2]));
        std::ifstream in(argv[1],std::ios::binary);char magic[8];read(in,magic,8);
        const bool sequence_schema=std::string(magic,8)=="SGNCF002";
        if(!sequence_schema&&std::string(magic,8)!="SGNCF001")throw std::runtime_error("solver corpus schema");
        auto count=read_u(in);if(count==0||count>10000)throw std::runtime_error("solver corpus count");unsigned errors=0;std::uint64_t exact=0;
        Record previous_actual,previous_expected;
        solver::PayloadAudit audit{};
        for(unsigned n=0;n<count;++n){const auto entry=read_u(in);const auto continuation=sequence_schema?read_u(in):0;
            Record actual(total_size),expected(total_size);read(in,actual.data(),actual.size());read(in,expected.data(),expected.size());
            if(continuation){if(continuation!=1||previous_actual.empty()||entry!=0x409b60)throw std::runtime_error("invalid solver continuation");
                const auto selected=u(actual,work_at);auto valid_input=previous_expected;u(valid_input,work_at,selected);
                if(actual!=valid_input)throw std::runtime_error("solver corpus continuation resets input");
                actual=previous_actual;u(actual,work_at,selected);}
            const auto excluded_count=read_u(in);if(excluded_count>256)throw std::runtime_error("solver excluded count");std::vector<unsigned> excluded;
            for(unsigned j=0;j<excluded_count;++j){const auto at=read_u(in);
                if(!permitted_opaque_word(at)||std::find(excluded.begin(),excluded.end(),at)!=excluded.end())throw std::runtime_error("invalid or duplicate solver opaque exclusion");
                excluded.push_back(at);}
            if(!continuation)audit={};
            Record without_audit=actual;
            apply(actual,entry,geometry,&audit);apply(without_audit,entry,geometry,nullptr);
            if(actual!=without_audit)throw std::runtime_error("solver audit changes native state");
            std::sort(excluded.begin(),excluded.end());const auto derived=contact::oracle::payload_mask(audit);
            if(excluded!=derived){
                std::cerr<<"case "<<n<<" entry "<<std::hex<<entry<<std::dec<<" source-mask "<<excluded.size()<<" native-mask "<<derived.size()<<'\n';
                for(auto at:excluded)if(!std::binary_search(derived.begin(),derived.end(),at))std::cerr<<"source-only "<<std::hex<<at<<std::dec<<'\n';
                for(auto at:derived)if(!std::binary_search(excluded.begin(),excluded.end(),at))std::cerr<<"native-only "<<std::hex<<at<<std::dec<<'\n';
                throw std::runtime_error("solver mask differs from executed native definedness");
            }
            previous_actual=actual;previous_expected=expected;
            for(unsigned at=0;at<total_size;at+=4){if(std::find(excluded.begin(),excluded.end(),at)!=excluded.end())continue;if(u(actual,at)==u(expected,at)){++exact;continue;}
                if(errors++<32)std::cerr<<"case "<<n<<" entry "<<std::hex<<entry<<" offset "<<at<<" expected "<<u(expected,at)<<" actual "<<u(actual,at)<<std::dec<<'\n';}
        }
        if(in.peek()!=std::char_traits<char>::eof())throw std::runtime_error("trailing solver corpus");
        std::cout<<"{\"cases\":"<<count<<",\"exact_words\":"<<exact<<",\"errors\":"<<errors<<",\"allowed_ulp\":0}\n";return errors?1:0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
