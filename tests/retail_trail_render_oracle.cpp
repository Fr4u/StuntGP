#include "sgn/retail_trail_render.hpp"
#include "sgn/retail_time_trial.hpp"
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
namespace {
using namespace sgn::retail;using Record=std::array<std::uint8_t,0x3f800>;
unsigned word(const Record& b,unsigned at){unsigned v;std::memcpy(&v,b.data()+at,4);return v;}
void put(Record& b,unsigned at,unsigned value){std::memcpy(b.data()+at,&value,4);}
float number(const Record& b,unsigned at){return std::bit_cast<float>(word(b,at));}
struct State {
    contact::effects::ImpactParticles particles;contact::effects::ImpactParticleDrawList list;
    TrailProfileBank profiles=initial_trail_profile_bank();SourceTrailPolygon staging;VehicleMatrix basis;unsigned split{};ViewportState viewport;
    CameraState camera;CameraLifecycle lifecycle;CameraPresentationState presentation;ControllerOverlayTransition overlay;
    SourceSpritePacketStorage sprites;std::array<SourceScreenPolygon,256> screen;unsigned screen_count{};
    std::array<VehicleVector,64> ring;TrailRenderAudit audit;std::array<ScreenPolygonAudit,256> screen_audit;
    TrailRenderBindings bindings(){audit.screen=screen_audit;return {profiles,staging,sprites,{MutableScreenPolygonBaseReader::fixed_fixture(screen),screen_count},viewport,{camera,lifecycle,presentation,overlay},basis,split,&audit};}
};
void transfer(Record& b,State& s,bool read){
    const auto blob=[&](auto& v,unsigned at){if(read)std::memcpy(&v,b.data()+at,sizeof v);else std::memcpy(b.data()+at,&v,sizeof v);};
    const auto tag=[&](bool& v,unsigned at){if(read){const auto value=word(b,at);if(value>1)throw std::runtime_error("trail render invalid provenance");v=value!=0;}else put(b,at,v?1:0);};
    blob(s.particles.particles,0);blob(s.particles.emitted,0x8800);blob(s.particles.cursor,0x8804);blob(s.particles.rate_selector,0x8808);blob(s.particles.throttle,0x880c);
    blob(s.list.slots,0x8820);blob(s.list.count,0x88a0);
    // SGNTR001 captures five mutable profiles; extra profiles retain pinned literals.
    for(unsigned j=0;j<5;++j)blob(s.profiles.profiles[j],0x88b0+j*272);
    for(unsigned j=0;j<2;++j){s.profiles.shapes[j].identity=j+1;blob(s.profiles.shapes[j].points,0x8e00+j*64);}
    blob(s.staging,0x8e80);blob(s.basis,0x8f20);blob(s.split,0x8f60);blob(s.viewport.clip_screen,0x8f70);blob(s.viewport.clip_expanded,0x8f80);
    blob(s.camera.position,0x9000);blob(s.presentation.forward_axis,0x9010);blob(s.presentation.planes,0x9020);blob(s.lifecycle.clip_far,0x9060);blob(s.camera.camera_to_world_rotation,0x9080);
    auto& p=s.sprites;blob(p.header_bytes,0x9100);blob(p.index_count,0x9104);blob(p.position_count,0x9108);blob(p.attribute_count,0x910c);blob(p.polygons,0x9110);blob(p.vertices,0x9112);blob(p.corners,0x9114);
    blob(p.right,0x9120);blob(p.up,0x9130);blob(p.forward,0x9140);blob(p.headers,0x9200);blob(p.indices,0xa200);blob(p.positions,0xb200);blob(p.attributes,0x13200);
    blob(s.screen,0x1b200);blob(s.screen_count,0x2c200);blob(s.ring,0x2c230);tag(s.audit.staging_position_w,0x2c640);
    for(unsigned j=0;j<4;++j)tag(s.audit.packet.staging_w[j],0x2c644+j*4);
    for(unsigned j=0;j<2048;++j)tag(s.audit.packet.packet_w[j],0x2c660+j*4);
    for(unsigned j=0;j<256;++j)for(unsigned k=0;k<68;++k)tag(s.screen_audit[j].undefined[k],0x2e660+j*272+k*4);
}
bool masked(const Record& b,unsigned at){
    unsigned tag=0;
    if(at==0x8e8c)tag=0x2c640;
    for(unsigned j=0;j<4;++j)if(at==0x8ebc+j*28)tag=0x2c644+j*4;
    if(at>=0xb200&&at<0x13200&&(at-0xb200)%16==12)tag=0x2c660+((at-0xb200)/16)*4;
    if(at>=0x1b200&&at<0x2c200)tag=0x2e660+(at-0x1b200);
    return tag&&word(b,tag)!=0;
}
}
int main(int argc,char** argv)try{
    if(argc!=2)throw std::runtime_error("expected trail render corpus");std::ifstream file(argv[1],std::ios::binary);
    const auto read=[&](void* data,std::size_t size){if(!file.read(static_cast<char*>(data),static_cast<std::streamsize>(size)))throw std::runtime_error("short trail render corpus");};
    const auto next=[&](){unsigned v;read(&v,4);return v;};char magic[8];read(magic,8);if(std::string_view(magic,8)!="SGNTR001")throw std::runtime_error("trail render schema");
    const auto scenarios=next(),steps=next();if(!scenarios||scenarios>4096||!steps||steps>4)throw std::runtime_error("trail render corpus limit");std::uint64_t exact=0,excluded=0,sprites=0,screen=0;
    for(unsigned n=0;n<scenarios;++n){auto s=std::make_unique<State>();auto actual=std::make_unique<Record>(),previous=std::make_unique<Record>();
        for(unsigned step=0;step<steps;++step){auto input=std::make_unique<Record>(),expected=std::make_unique<Record>();read(input->data(),input->size());read(expected->data(),expected->size());
            if(step==0){*actual=*input;transfer(*actual,*s,true);for(unsigned at=0;at<actual->size();at+=4)if(masked(*actual,at))throw std::runtime_error("trail render initial provenance");}
            else {if(*input!=*previous)throw std::runtime_error("trail render broken source continuation");for(unsigned at=0;at<actual->size();at+=4)if(!masked(*actual,at)&&word(*actual,at)!=word(*input,at))throw std::runtime_error("trail render broken native continuation");}
            const auto old_sprite=s->sprites.header_bytes,old_screen=s->screen_count,mode=word(*actual,0x2c210);
            if(mode==0)append_impact_particle_trails(s->bindings(),s->particles,s->list);
            else if(mode==1){const auto profile=word(*actual,0x2c214);if(profile>=s->profiles.profiles.size())throw std::runtime_error("trail render profile index");append_retail_trail(s->bindings(),s->profiles.profiles[profile],s->ring,word(*actual,0x2c218),word(*actual,0x2c21c),number(*actual,0x2c220),number(*actual,0x2c224));}
            else throw std::runtime_error("trail render mode");
            sprites+=(s->sprites.header_bytes-old_sprite)/8;screen+=s->screen_count-old_screen;transfer(*actual,*s,false);
            for(unsigned at=0;at<actual->size();at+=4){
                if(masked(*actual,at)!=masked(*expected,at))throw std::runtime_error("trail render provenance mismatch scenario "+std::to_string(n)+" step "+std::to_string(step)+" offset "+std::to_string(at));
                if(masked(*actual,at)){++excluded;continue;}
                if(word(*actual,at)!=word(*expected,at))throw std::runtime_error("trail render mismatch scenario "+std::to_string(n)+" step "+std::to_string(step)+" offset "+std::to_string(at)+" actual "+std::to_string(word(*actual,at))+" expected "+std::to_string(word(*expected,at)));++exact;
            }
            *previous=*expected;
        }
    }
    if(file.peek()!=std::char_traits<char>::eof())throw std::runtime_error("trailing trail render corpus");
    std::cout<<"{\"cases\":"<<scenarios*steps<<",\"exact_words\":"<<exact<<",\"excluded_words\":"<<excluded<<",\"sprites\":"<<sprites<<",\"screen_polygons\":"<<screen<<",\"errors\":0,\"allowed_ulp\":0}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}
