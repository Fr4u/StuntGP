#include "world_template_fixture.hpp"
#include "sgn/retail_vehicle_storage.hpp"
#include "sgn/vehicle_dynamics_shared.hpp"
#include "contact_solver_state.hpp"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <memory>
#include <type_traits>

namespace {
using namespace sgn;using namespace sgn::retail;
void require(bool condition,const char* reason){if(!condition)throw std::runtime_error(reason);}
// These explicitly selected owners contain only scalar numeric members/arrays;
// optional identities are seeded and checked through their typed API below.
template<class T>void poison(T& value){static_assert(std::is_trivially_copyable_v<T>);std::memset(&value,0x3f,sizeof(value));}
template<class T>void zero(const T& value){static_assert(std::is_trivially_copyable_v<T>);const T empty{};require(std::memcmp(&value,&empty,sizeof(T))==0,"represented scalar owner not cleared");}
void seed(VehicleStorage& s){
    for(unsigned j=0;j<6;++j){std::array<std::uint8_t,0x7800> bytes;bytes.fill(0x3f);
        contact::oracle::transfer(bytes,s.parameters[j],s.physics[j],s.race[j],s.contact[j],true);
        s.race[j].recording=ReplayHandle{j};s.race[j].place=99;
        poison(s.drivers[j]);poison(s.ai[j]);poison(s.ai_stunts[j]);poison(s.history[j]);poison(s.load[j]);poison(s.air[j]);poison(s.trails[j]);poison(s.trail_inputs[j]);
        poison(s.anchors[j]);poison(s.strips[j]);poison(s.audio[j]);
        poison(s.camera_settings[j]);poison(s.rail[j]);poison(s.restart[j]);
        poison(s.segments[j]);poison(s.initialization_extras[j]);
        poison(s.tick[j]);poison(s.model_effects[j]);poison(s.particles[j]);
        poison(s.route_assist[j]);poison(s.shadows[j]);poison(s.progression[j]);poison(s.startup[j]);
        for(auto& ring:s.skid_rings[j].items){ring.last_corner=508;poison(ring.segments);}
    }
    for(unsigned j=0;j<8;++j){poison(s.configuration_identity[j]);poison(s.presentation[j]);poison(s.wheels[j]);auto& m=s.models[j];m.model=j;m.shadow=j+8;
        poison(m.position);poison(m.scale);poison(m.target);poison(m.up);poison(m.template_tail);m.field_2708=99;poison(m.body_rotation);}
}
void cleared(const VehicleStorage& s){
    for(unsigned j=0;j<6;++j){std::array<std::uint8_t,0x7800> bytes{};auto p=s.parameters[j];auto v=s.physics[j];auto r=s.race[j];auto c=s.contact[j];
        contact::oracle::transfer(bytes,p,v,r,c,false);require(std::all_of(bytes.begin(),bytes.end(),[](auto b){return b==0;}),"source-mapped numeric words not cleared");
        require(!r.recording&&r.place==0&&!c.surface.object_mode,"optional per-car ownership retained");
        zero(s.parameters[j]);zero(s.physics[j]);zero(s.drivers[j]);zero(s.ai[j]);zero(s.ai_stunts[j]);zero(s.history[j]);zero(s.load[j]);zero(s.air[j]);zero(s.trails[j]);
        zero(s.trail_inputs[j]);zero(s.anchors[j]);zero(s.strips[j]);zero(s.audio[j]);zero(s.presentation[j]);zero(s.wheels[j]);
        zero(s.camera_settings[j]);zero(s.rail[j]);zero(s.restart[j]);zero(s.configuration_identity[j]);
        zero(s.segments[j]);zero(s.initialization_extras[j]);
        zero(s.tick[j]);zero(s.model_effects[j]);zero(s.particles[j]);
        zero(s.route_assist[j]);zero(s.shadows[j]);zero(s.progression[j]);zero(s.startup[j]);
        for(const auto& ring:s.skid_rings[j].items){require(!ring.last_corner,"strip corner binding retained");zero(ring.segments);}
        const auto& m=s.models[j];require(!m.model&&!m.shadow&&m.field_2708==0,"model identities retained");zero(m.position);zero(m.scale);zero(m.target);zero(m.up);zero(m.template_tail);zero(m.body_rotation);
    }
    for(unsigned j=6;j<8;++j){VehiclePresentationState p;poison(p);require(std::memcmp(&s.presentation[j],&p,sizeof(p))==0,"nonphysical presentation slot cleared by six-car clear");
        VehicleConfigurationIdentity identity;poison(identity);require(s.configuration_identity[j].header==identity.header,"extra configuration identity cleared by six-car clear");
        require(s.models[j].model==j&&s.models[j].shadow==j+8&&s.models[j].field_2708==99,"nonphysical model slot cleared");
        std::array<VehicleWheelAnimation,4> wheels;poison(wheels);require(std::memcmp(s.wheels[j].data(),wheels.data(),sizeof(wheels))==0,"nonphysical wheel slot cleared");}
}
}
int main(int argc,char** argv){try{
    if(argc!=2)throw std::runtime_error("expected retail data root");
    auto storage=std::make_unique<VehicleStorage>();AssetStore assets(argv[1]);seed(*storage);
    VehicleScoreState score;score.sound_id=91;score.popups[0].active=1;
    contact::player::Controls controls;controls.current=3;controls.controllers[3].vehicle=5;
    VehicleDynamicsWorkspace workspace;workspace.steering.fill(7);unsigned current=4;
    ReplayBank bank;std::fill(bank.bytes().begin(),bank.bytes().end(),std::uint8_t{0x5a});const Bytes before_bank(bank.bytes().begin(),bank.bytes().end());
    std::array<SelectionRecord,6> selection{};std::array<LevelCarSlot,6> cars{};TimeTrialGhosts ghosts;TimeTrialSelectionState tt;VehicleRandom rng{77};LevelAssetBindings bindings;
    LevelSelectionContext context{selection,cars,bank,ghosts,tt,rng,assets,bindings,*storage,"TRACK1",0x8000000};
    // Execute the actual qualified caller with no selected records. Its only
    // vehicle operation is the mandatory six-stride clear of live owners.
    prepare_level_cars(context);cleared(*storage);
    require(std::equal(before_bank.begin(),before_bank.end(),bank.bytes().begin()),"vehicle clear changed global replay bank");
    require(score.sound_id==91&&score.popups[0].active==1&&controls.current==3&&controls.controllers[3].vehicle==5&&current==4&&workspace.steering[0]==7,"vehicle clear changed global state");
    auto reset=storage->reset_views();auto contact=storage->contact_views();auto load=storage->load_views();auto automatic=storage->automatic_views();auto wake=storage->wake_views();
    for(unsigned j=0;j<6;++j){require(&reset[j].physics==&storage->physics[j]&&&reset[j].race==&storage->race[j]&&&reset[j].contact==&storage->contact[j]&&reset[j].id==j,"reset view owns another car");
        require(&contact[j].state==&storage->physics[j]&&&contact[j].contact==&storage->contact[j]&&contact[j].id==j,"contact view owns another car");
        require(&load[j].parameters==&storage->parameters[j]&&&load[j].history==&storage->history[j]&&&load[j].load==&storage->load[j],"loader copied state");
        require(&automatic[j].physical==&storage->physics[j]&&&automatic[j].ai==&storage->ai[j]&&&wake[j].physics==&storage->physics[j],"AI/wake copied state");}
    initialize_cold_numeric_storage(storage->cold_numeric(current),0.2725f);require(current==0&&storage->race[0].active==1&&storage->physics[0].energy==1000,"cold numeric initializer disconnected from storage");
    initialize_cold_presentation(storage->cold_presentation(),17,18,sgn::retail::fixture::cold_world_template);require(storage->models[0].model==17&&storage->models[0].shadow==18&&!storage->models[7].model,"cold presentation did not include eight slots");
    seed(*storage);storage->contact[2].surface.object_mode.reset();storage->physics[2].energy=73;storage->ai_stunts[2].grade=2;
    storage->camera_settings[2].distance=4321;storage->rail[2].progress=456;storage->restart[2].previous_input.turbo=23;storage->configuration_identity[2].header[0]='V';
    storage->initialization_extras[2].field_28b0=0xc2700000u;
    storage->tick[2].low_energy_notice=17;storage->model_effects[2].cached_brake=19;storage->particles[2].field_761c=23;
    storage->model_effects[2].glow_phases={0xfffffff0u,8191u};
    storage->clone_vehicle(2,5);
    require(storage->physics[5].energy==73&&storage->ai_stunts[5].grade==2&&storage->race[5].recording==ReplayHandle{2}&&storage->models[5].model==2&&storage->models[5].shadow==10&&!storage->contact[5].surface.object_mode,"clone changed resource/replay identity or shared physical fields");
    require(storage->camera_settings[5].distance==4321&&storage->rail[5].progress==456&&storage->restart[5].previous_input.turbo==23&&storage->configuration_identity[5].header[0]=='V',"clone omitted camera/rail/restart/configuration owners");
    require(storage->initialization_extras[5].field_28b0==0xc2700000u&&std::memcmp(&storage->segments[2],&storage->segments[5],sizeof(VehicleSegments))==0,"clone omitted segment or cold extra owner");
    require(storage->tick[5].low_energy_notice==17,"clone omitted vehicle tick owner");
    require(storage->model_effects[5].cached_brake==19&&storage->particles[5].field_761c==23,"clone omitted model effect or particle owner");
    require(storage->model_effects[5].glow_phases==storage->model_effects[2].glow_phases,"clone omitted glow phase owner");
    ++storage->model_effects[5].glow_phases[0];require(storage->model_effects[2].glow_phases[0]==0xfffffff0u,"cloned glow phases alias source vehicle");
    for(unsigned j=0;j<4;++j){const auto& source_ring=storage->skid_rings[2].items[j];const auto& target_ring=storage->skid_rings[5].items[j];require(target_ring.last_corner==508u&&std::memcmp(source_ring.segments.data(),target_ring.segments.data(),sizeof(source_ring.segments))==0,"clone omitted strip ring or changed world corner identity");}
    require(storage->route_assist[5].blend==storage->route_assist[2].blend,"clone omitted route assist owner");
    require(std::memcmp(&storage->shadows[5],&storage->shadows[2],sizeof(VehicleShadowState))==0,"clone omitted shadow owner");
    require(std::memcmp(&storage->progression[5],&storage->progression[2],sizeof(VehicleProgressionState))==0,"clone omitted progression owner");
    require(std::memcmp(&storage->startup[5],&storage->startup[2],sizeof(VehicleRaceStartupState))==0,"clone omitted race startup owner");
    std::array<std::uint8_t,0x7800> source{},target{};contact::oracle::transfer(source,storage->parameters[2],storage->physics[2],storage->race[2],storage->contact[2],false);
    contact::oracle::transfer(target,storage->parameters[5],storage->physics[5],storage->race[5],storage->contact[5],false);require(source==target,"clone differs in source-mapped numeric words");
    storage->physics[5].energy=13;require(storage->physics[2].energy==73,"clone aliases mutable physics");
    storage->contact[2].surface.object_mode=1;bool rejected=false;try{storage->clone_vehicle(2,5);}catch(const std::domain_error&){rejected=true;}
    require(rejected&&storage->physics[5].energy==13,"unsupported external object clone mutated target");
    storage->physics[0].position={1,2,3,4};storage->physics[0].stunt.launch_height=321;storage->race[0].active=1;
    storage->race[0].progress=765;storage->race[0].saved_camera_mode=3;storage->camera_settings[0]={};
    storage->camera_settings[0].angles.w=17;initialize_vehicle_camera_settings(storage->camera_settings[0]);
    auto snapshot=storage->camera_snapshot(0);
    require(snapshot.position.x==1&&snapshot.contact_height==321&&snapshot.progress==765&&snapshot.camera_mode==3&&snapshot.angles.y==180&&snapshot.angles.w==17&&snapshot.distance==1000,"camera snapshot lost source aliases or retained padding");
    snapshot.position.x=999;snapshot.progress=999;snapshot.active=0;snapshot.contact_height=999;
    snapshot.distance=678;snapshot.camera_mode=4;snapshot.orbit_offset={11,12,13,14};storage->store_camera_settings(0,snapshot);
    require(storage->physics[0].position.x==1&&storage->physics[0].stunt.launch_height==321&&storage->race[0].active==1&&storage->race[0].progress==765,"camera writeback replaced live physical or race observations");
    require(storage->camera_settings[0].distance==678&&storage->camera_settings[0].orbit_offset.w==14&&storage->race[0].saved_camera_mode==4,"camera writeback did not publish persistent settings");
    storage->physics[0].position.x=31;require(storage->camera_snapshot(0).position.x==31,"camera snapshot retained stale physics");
    std::cout<<"physical_slots=6 presentation_slots=8 represented_owner_groups=32 borrowed_view_families=5 selection_clear_calls=1 clone_checks=13 errors=0\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
