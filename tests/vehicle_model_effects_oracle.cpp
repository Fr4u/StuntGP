#include "sgn/vehicle_model_effects.hpp"
#include "vehicle_oracle_state.hpp"
#include <bit>
#include <fstream>
#include <iostream>
#include <set>
namespace {
using namespace sgn;using namespace sgn::retail;
using Record=std::array<std::uint8_t,0x16000>;
std::uint32_t word(const Record& r,std::size_t at){std::uint32_t x;std::memcpy(&x,r.data()+at,4);return x;}
void put(Record& r,std::size_t at,std::uint32_t x){std::memcpy(r.data()+at,&x,4);}
template<class T>void read(std::ifstream& f,T& value){if(!f.read(reinterpret_cast<char*>(&value),sizeof(value)))throw std::runtime_error("truncated model effects corpus");}
struct Model final:VehicleModelEffectResource {
    unsigned lod{},count{};std::array<std::uint32_t,4> lod_map{};
    std::array<ModelTransform,96> transforms{};std::array<std::uint16_t,96> flags{};
    std::array<std::array<std::uint32_t,4>,96> parts{};
    std::array<std::array<std::uint8_t,8>,128> attrs{};std::array<std::array<std::uint32_t,4>,256> coords{};
    std::uint32_t part_count() const override{return count;}
    unsigned selected_lod() const override{return lod;}
    std::size_t at(unsigned l,unsigned i) const{if(l>=4||i>=24||lod_map[l]>=4)throw std::runtime_error("model source part outside corpus");return lod_map[l]*24+i;}
    VehicleModelEffectPart part(unsigned l,unsigned i) override{const auto n=at(l,i);return {flags[n],transforms.at(parts[n][0])};}
    std::span<std::array<std::uint8_t,8>> attributes(unsigned l,unsigned i) override{const auto v=parts[at(l,i)][1];return std::span(attrs).subspan(v&0xffff,v>>16);}
    std::span<std::array<std::uint32_t,4>> coordinates(unsigned l,unsigned i) override{const auto v=parts[at(l,i)][2];return std::span(coords).subspan(v&0xffff,v>>16);}
};
struct State {
    VehicleParameters parameters{};VehicleState physics{};VehicleRaceState race{};VehicleTrailInputs inputs{};VehicleTrailStorage trail_storage{};
    VehicleAnchorState anchors{};VehicleModelEffects effects{};VehicleMatrix basis{};VehicleTrailModel trail_model{};VehicleTrailProjection projection{};
    contact::effects::ImpactParticles particles{};VehicleRandom random{};Model model;
    std::array<float,128> lights{};std::array<std::uint32_t,128> ambience{};
};
void transfer(Record& r,State& s,bool loading){
    oracle::transfer(r,s.parameters,s.physics,loading);
    auto u=[&](std::uint32_t& x,std::size_t at){if(loading)x=word(r,at);else put(r,at,x);};
    auto f=[&](float& x,std::size_t at){if(loading)x=std::bit_cast<float>(word(r,at));else put(r,at,std::bit_cast<std::uint32_t>(x));};
    auto v=[&](VehicleVector& x,std::size_t at){f(x.x,at);f(x.y,at+4);f(x.z,at+8);f(x.w,at+12);};
    auto blob=[&](auto& object,std::size_t at){if(loading)std::memcpy(&object,r.data()+at,sizeof(object));else std::memcpy(r.data()+at,&object,sizeof(object));};
    u(s.race.spline_index,0x24a4);u(s.inputs.field_28a0,0x28a0);
    u(s.effects.phase,0x225c);u(s.effects.enabled,0x226c);u(s.effects.cached_brake,0x28a4);u(s.effects.route_envelope,0x7684);
    for(unsigned i=0;i<2;++i){const auto at=0x328c+i*0x140;
        u(s.anchors.effect_available[i],at);v(s.anchors.effect_position[i],at+4);v(s.anchors.effect_base_scale[i],at+20);
        v(s.effects.current_scale[i],at+36);blob(s.effects.trails[i],at+52);u(s.effects.cursor[i],at+308);}
    blob(s.basis,0x7800);v(s.trail_model.base_forward,0x7840);v(s.trail_model.base_up,0x7850);f(s.trail_model.part23_position_z,0x7860);
    blob(s.projection.world_view,0x7880);f(s.projection.projection_x,0x78c0);f(s.projection.projection_y,0x78c4);
    f(s.projection.center_x,0x78c8);f(s.projection.center_y,0x78cc);f(s.projection.scale_x,0x78d0);f(s.projection.scale_y,0x78d4);f(s.projection.clip_near,0x78d8);
    blob(s.lights,0x7a00);blob(s.ambience,0x7c00);u(s.model.lod,0x7e08);u(s.model.count,0x7e10);u(s.random.state,0x7e14);blob(s.model.lod_map,0x7e20);
    static_assert(sizeof(contact::effects::ImpactParticle)==0x440);blob(s.particles.particles,0x8000);
    u(s.particles.emitted,0x10800);u(s.particles.cursor,0x10804);u(s.particles.rate_selector,0x10808);blob(s.particles.throttle,0x1080c);
    blob(s.model.transforms,0x11000);blob(s.model.flags,0x13000);blob(s.model.parts,0x13400);blob(s.model.attrs,0x14000);blob(s.model.coords,0x14800);
}
std::set<std::size_t> policy(const Record& r,unsigned op){
    std::set<std::size_t> out;
    if(op!=3&&op!=4&&op!=5)return out;
    if(word(r,0x328c)!=1)return out;
    const float turbo=std::bit_cast<float>(word(r,0x2254));
    if((turbo==0||std::isnan(turbo))&&word(r,0x2260)==0)return out;
    if(word(r,0x7e18)>=90)for(unsigned i=0;i<2;++i)if(word(r,0x328c+i*0x140)==1){
        out.insert(0x780c);out.insert(0x781c);
        const auto at=0x32c0+i*0x140;out.insert(at+7*16+12);out.insert(at+((word(r,0x33c0+i*0x140)+1u)%7u)*16+12);
    }
    const auto timer=word(r,0x2268);
    if(timer&&((timer-1u)&1u)==0){auto cursor=word(r,0x10804);if(cursor>=32)cursor=0;out.insert(0x8000+cursor*0x440+20);
        if(word(r,0x33cc)==1){cursor=cursor+1>=32?0:cursor+1;out.insert(0x8000+cursor*0x440+20);}}
    return out;
}
void validate(const Record& r){
    if(word(r,0x7e08)>=4||word(r,0x7e10)>24||word(r,0x7e0c)>=16)throw std::runtime_error("model selector outside corpus");
    for(unsigned i=0;i<4;++i)if(word(r,0x7e20+i*4)>=4)throw std::runtime_error("model LOD map outside corpus");
    for(unsigned i=0;i<96;++i){const auto at=0x13400+i*16;const auto a=word(r,at+4),c=word(r,at+8);
        if(word(r,at)>=96||(a&0xffff)+(a>>16)>128||(c&0xffff)+(c>>16)>256)throw std::runtime_error("model part span outside corpus");}
}
}
int main(int argc,char** argv)try{
    if(argc!=2)throw std::runtime_error("expected model effects corpus");
    std::ifstream file(argv[1],std::ios::binary);std::array<char,8> magic{};read(file,magic);
    if(std::string_view(magic.data(),8)!="SGNMEF01")throw std::runtime_error("bad model effects magic");
    std::uint32_t count{};read(file,count);if(count==0||count>10000)throw std::runtime_error("invalid model effects count");
    State s;Record previous{};unsigned previous_op=99;std::size_t words{},masked{},mismatches{},continuous{};
    for(unsigned n=0;n<count;++n){unsigned op{};read(file,op);if(op>5)throw std::runtime_error("invalid model effects operation");
        Record r{},expected{};read(file,r);read(file,expected);validate(r);const auto input=r;const auto mask=policy(r,op);const auto step=word(r,0x7e0c);
        if(op!=5&&step!=0)throw std::runtime_error("unexpected model effects step");
        if(op==5&&step!=0){if(previous_op!=5||word(previous,0x7e0c)+1!=step)throw std::runtime_error("broken model effects step order");
            put(previous,0x7e0c,step);if(previous!=r)throw std::runtime_error("broken continuous native model effects state");++continuous;
        }else transfer(r,s,true);
        const auto settings=initial_vehicle_trail_settings();
        if(op==0||op>=4)update_vehicle_brake_lights(s.physics,s.inputs,s.effects,s.model);
        if(op==1||op>=4)update_vehicle_route_lights(s.physics,s.race,s.inputs,s.effects,s.model,{s.lights,s.ambience,word(r,0x7e00),word(r,0x7e04)});
        if(op==2||op>=4)update_vehicle_brake_effects(s.physics,s.model);
        if(op==3||op>=4){const auto& first=s.model.part(0,0).transform;const auto& last=s.model.part(0,23).transform;
            const VehicleTrailModel current_model{{first.forward.x,first.forward.y,first.forward.z,std::bit_cast<float>(first.forward_w)},
                {first.up.x,first.up.y,first.up.z,std::bit_cast<float>(first.up_w)},last.position.z};
            const VehicleTrailBindings launch{s.physics,s.parameters,s.trail_storage,s.basis,s.inputs,current_model,s.projection,settings,word(r,0x7e18)};
            update_vehicle_launch_effect(launch,s.effects,s.anchors,s.model,s.particles,s.random);
        }
        transfer(r,s,false);previous=r;previous_op=op;
        for(std::size_t at=0;at<r.size();at+=4){if(mask.contains(at)){++masked;if(word(r,at)!=word(input,at))throw std::runtime_error("native model effects W retention failed");continue;}
            ++words;if(word(r,at)!=word(expected,at)){if(mismatches<25)std::cerr<<"case="<<n<<" op="<<op<<" offset="<<std::hex<<at<<" actual="<<word(r,at)<<" expected="<<word(expected,at)<<std::dec<<'\n';++mismatches;}}
    }
    if(file.peek()!=std::char_traits<char>::eof())throw std::runtime_error("trailing model effects corpus");
    std::cout<<"cases="<<count<<" words="<<words<<" undefined_W="<<masked<<" continuous_native_steps="<<continuous<<" mismatches="<<mismatches<<" tolerance_ulp=0\n";return mismatches?1:0;
}catch(const std::exception& e){std::cerr<<"model effects oracle: "<<e.what()<<'\n';return 2;}
