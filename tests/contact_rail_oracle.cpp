#include "sgn/contact_rail.hpp"
#include "contact_solver_state.hpp"
#include "contact_rail_state.hpp"
#include "audio_oracle_state.hpp"
#include <memory>

namespace {
using RailRecord=std::vector<std::uint8_t>;
constexpr unsigned size=0x3c000,cs=0x7800,ca=0x2d000,p=0x2f000,bank=0x30000,audio_at=0x39000,projection_at=0x3a000;
unsigned word(const RailRecord& b,unsigned at){unsigned value;std::memcpy(&value,b.data()+at,4);return value;}
void word(RailRecord& b,unsigned at,unsigned value){std::memcpy(b.data()+at,&value,4);}
float number(const RailRecord& b,unsigned at){return std::bit_cast<float>(word(b,at));}
std::vector<unsigned> apply(RailRecord& b,unsigned entry,const SplineSet& splines,const std::vector<std::string>& labels){
    auto parameters=std::make_unique<std::array<VehicleParameters,6>>();auto physics=std::make_unique<std::array<VehicleState,6>>();
    auto contacts=std::make_unique<std::array<contact::solver::State,6>>();std::array<VehicleRaceState,6> races;std::array<VehicleRailState,6> rails;
    const auto cars=[&](bool loading){for(unsigned j=0;j<6;++j){auto bytes=std::span(b).subspan(j*cs,cs);
        contact::oracle::transfer(bytes,(*parameters)[j],(*physics)[j],races[j],(*contacts)[j],loading);contact::rail_oracle::transfer(bytes,rails[j],loading);}};
    cars(true);
    std::array<ResetVehicle,6> views{{{(*physics)[0],races[0],(*contacts)[0],0},{(*physics)[1],races[1],(*contacts)[1],1},
        {(*physics)[2],races[2],(*contacts)[2],2},{(*physics)[3],races[3],(*contacts)[3],3},{(*physics)[4],races[4],(*contacts)[4],4},{(*physics)[5],races[5],(*contacts)[5],5}}};
    contact::player::Controls controls;controls.current=word(b,p+4);
    const auto controllers=[&](bool loading){
        const auto field=[&](auto& value,unsigned at){static_assert(sizeof(value)==4);if(loading)std::memcpy(&value,b.data()+at,4);else std::memcpy(b.data()+at,&value,4);};
        const auto vector=[&](CameraVector& v,unsigned at){field(v.x,at);field(v.y,at+4);field(v.z,at+8);field(v.w,at+12);};
        for(unsigned j=0;j<4;++j){auto& c=controls.controllers[j];auto& s=c.camera;const auto at=ca+j*0x74c;
            field(c.active,at);field(c.vehicle,at+0xac);field(s.mode,at+4);vector(s.position,at+8);vector(s.side,at+0x7c);vector(s.velocity,at+0xc4);
            field(s.twist,at+0x78);field(s.lens,at+0xb0);field(s.near_clip,at+0xb4);field(s.far_factor,at+0xb8);field(s.half_width,at+0xbc);field(s.half_height,at+0xc0);
            field(s.collision_bypass_ticks,at+0x1a4);field(s.pending_lens,at+0x1e0);field(s.requested_lens,at+0x1e4);
            field(c.respawn_transition.state,at+0x300);field(c.respawn_transition.phase,at+0x308);
            if(loading){std::memcpy(c.notices.data(),b.data()+at+0x4ac,sizeof(c.notices));
                std::memcpy(s.projection.data(),b.data()+projection_at,64);s.inverse_depth_scale=number(b,projection_at+64);s.depth_offset=number(b,projection_at+68);
            }else std::memcpy(b.data()+at+0x4ac,c.notices.data(),sizeof(c.notices));
        }
    };controllers(true);
    VehicleResetParameters reset_parameters{};reset_parameters.stage=std::bit_cast<std::int32_t>(word(b,p+8));reset_parameters.mode=std::bit_cast<std::int32_t>(word(b,p+12));reset_parameters.tick=word(b,p+16);
    reset_parameters.unit_gravity={number(b,p+0x40),number(b,p+0x44),number(b,p+0x48),number(b,p+0x4c)};
    reset_parameters.vehicle_names={"CAR 0","CAR 1","CAR 2","CAR 3","CAR 4","CAR 5"};reset_parameters.special_end_text="Recovered";
    VehicleResetGlobals globals{word(b,p+20),word(b,p+24),word(b,p+28)};VehicleResetWorld reset{views,word(b,p),controls,globals,reset_parameters};
    RailParameters config{number(b,p+32),number(b,p+36),{number(b,p+0x50),number(b,p+0x54),number(b,p+0x58)},"Charging"};
    RailState state;if(word(b,p+0x30))state.spline=splines.find(2);if(word(b,p+0x34)!=0xffffffff)state.tube_glass_texture=word(b,p+0x34);
    contact::effects::ImpactParticles particles;static_assert(sizeof(contact::effects::ImpactParticle)==0x440);
    particles.emitted=word(b,p+0x60);particles.cursor=word(b,p+0x64);particles.rate_selector=word(b,p+0x68);particles.throttle=std::bit_cast<std::int32_t>(word(b,p+0x6c));
    std::memcpy(particles.particles.data(),b.data()+bank,0x8800);VehicleRandom random{word(b,p+0x70)};
    Record audio_record{};std::memcpy(audio_record.data(),b.data()+audio_at,sizeof(audio_record));
    AudioDriverState audio_state;LevelAudioState level;contact::player::Controls unused;
    transfer(audio_record,audio_state,level,unused,true);Device device(audio_record);AudioDriver driver{audio_state,device,level,controls,audio_record[8]};
    contact::solver::PayloadAudit payload;RailUpdateAudit audit;
    CameraMatrix shared_model_identity{}; //isolated fixture owner; old corpus does not compare61C468
    const DecodedTrackSplines track_splines{splines,labels}; //R18 decoded TrackSplineSource
    RailWorld world{reset,*parameters,rails,state,track_splines,config,particles,random,driver,shared_model_identity,&payload};
    if(entry==0x419310)update_rail(world,&audit);
    else if(entry==0x4191f0)update_rail_refuel(world);
    else if(entry==0x418bd0)update_rail_audio(world);
    else if(entry==0x419110)begin_rail_notice(world);
    else if(entry==0x4192f0)initialize_rail(state,splines,labels);
    else if(entry==0x418f70){const auto result=emit_rail_particles(particles,random,views[reset.current].physics,rails[reset.current]);if(result.anchor)payload.particle_velocity[*result.anchor]=true;}
    else throw std::runtime_error("unknown full rail entry");
    cars(false);controllers(false);word(b,p,reset.current);word(b,p+4,controls.current);
    word(b,p+20,globals.transition_active);word(b,p+24,globals.special_value);word(b,p+28,globals.special_timer);
    word(b,p+0x30,state.spline?1:0);word(b,p+0x34,state.tube_glass_texture.value_or(0xffffffff));
    word(b,p+0x60,particles.emitted);word(b,p+0x64,particles.cursor);word(b,p+0x68,particles.rate_selector);word(b,p+0x6c,std::bit_cast<unsigned>(particles.throttle));word(b,p+0x70,random.state);
    std::memcpy(b.data()+bank,particles.particles.data(),0x8800);
    transfer(audio_record,audio_state,level,unused,false);std::memcpy(b.data()+audio_at,audio_record.data(),sizeof(audio_record));
    if(audit.lens_controller){const auto& camera=controls.controllers[*audit.lens_controller].camera;std::memcpy(b.data()+projection_at,camera.projection.data(),64);word(b,projection_at+64,bits(camera.inverse_depth_scale));word(b,projection_at+68,bits(camera.depth_offset));}
    std::vector<unsigned> mask;for(unsigned j=0;j<32;++j)if(payload.particle_velocity[j])mask.push_back(bank+j*0x440+0x24);
    word(b,p+0x74,audit.lens_controller.value_or(0xffffffff));
    if(device.trace.size()>128)throw std::runtime_error("rail device trace overflow");
    word(b,p+0x78,static_cast<unsigned>(device.trace.size()));
    std::memcpy(b.data()+projection_at+0x100,device.trace.data(),device.trace.size()*sizeof(Event));
    return mask;
}
}
int main(int argc,char** argv)try{
    if(argc!=3)throw std::runtime_error("expected full rail corpus and track");
    auto bytes=sgn::read_file(argv[2]);const auto model=sgn::decode_pmd(bytes);SplineSet splines(std::move(bytes));
    std::ifstream file(argv[1],std::ios::binary);const auto read_bytes=[&](void* output,std::size_t n){file.read(static_cast<char*>(output),static_cast<std::streamsize>(n));if(!file)throw std::runtime_error("truncated full rail corpus");};
    const auto next=[&](){unsigned v;read_bytes(&v,4);return v;};char magic[8];read_bytes(magic,8);if(std::memcmp(magic,"SGNRA001",8))throw std::runtime_error("full rail schema");
    const auto count=next();if(!count||count>20000)throw std::runtime_error("full rail count");std::uint64_t exact=0,excluded=0;unsigned errors=0;
    for(unsigned n=0;n<count;++n){const auto entry=next();RailRecord actual(size),expected(size);read_bytes(actual.data(),size);read_bytes(expected.data(),size);
        const auto mask_count=next();if(mask_count>32)throw std::runtime_error("rail mask count");std::vector<unsigned> mask;for(unsigned j=0;j<mask_count;++j)mask.push_back(next());
        const auto before=actual;const auto native_mask=apply(actual,entry,splines,model.textures);if(mask!=native_mask)throw std::runtime_error("rail mask differs from executed native writes");
        for(const auto at:mask)if(word(before,at)!=word(actual,at))throw std::runtime_error("rail opaque retention failed");
        for(unsigned at=0;at<size;at+=4){if(std::find(mask.begin(),mask.end(),at)!=mask.end()){++excluded;continue;}
            if(word(actual,at)==word(expected,at)){++exact;continue;}
            if(errors++<16)std::cerr<<"rail case "<<n<<" entry "<<std::hex<<entry<<" offset "<<at<<" actual "<<word(actual,at)<<" expected "<<word(expected,at)<<std::dec<<'\n';
        }
    }
    if(file.peek()!=std::char_traits<char>::eof())throw std::runtime_error("trailing full rail corpus");
    std::cout<<"{\"cases\":"<<count<<",\"exact_words\":"<<exact<<",\"errors\":"<<errors<<",\"excluded_words\":"<<excluded<<",\"allowed_ulp\":0}\n";return errors?1:0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}
