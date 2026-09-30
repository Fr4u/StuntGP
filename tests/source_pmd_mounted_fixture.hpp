#pragma once
#include "sgn/retail_camera_frame_session.hpp"
#include "sgn/retail_camera_mounted.hpp"
#include "sgn/model_resources.hpp"
#include "retail_camera_lifecycle_state.hpp"
#include <memory>
#include <iostream>
#include <functional>
namespace sgn::mounted_test {
using namespace retail;using namespace retail::camera_test;
inline void check(bool ok,const char* why){if(!ok)throw FormatError(why);}
inline void observe(VehicleStorage& storage,unsigned slot,const CameraVehicle& v){
    auto& p=storage.physics[slot];auto& r=storage.race[slot];
    r.active=static_cast<std::int32_t>(v.active);r.progress=v.progress;p.stunt.launch_height=v.contact_height;
    p.position=std::bit_cast<VehicleVector>(v.position);p.velocity={v.velocity.x,v.velocity.y,v.velocity.z};p.velocity_w=v.velocity.w;
    p.basis_x=std::bit_cast<VehicleVector>(v.right);p.up={v.up.x,v.up.y,v.up.z};p.basis_y_w=v.up.w;
    p.basis_z=std::bit_cast<VehicleVector>(v.forward);p.orientation=v.orientation;p.speed=v.speed;
    storage.parameters[slot].height=v.height;p.contact_class=static_cast<std::int32_t>(v.contact_class);p.contact_counter=static_cast<std::int32_t>(v.contact_counter);
    p.auxiliary_displacement=std::bit_cast<VehicleVector>(v.shift);p.previous_auxiliary_displacement=std::bit_cast<VehicleVector>(v.previous_shift);
    storage.store_camera_settings(slot,v);
}
inline Bytes camera_bytes(CameraState c,CameraLifecycle life={}){
    CameraVehicle unused;Bytes out(camera_size,0xa5),v(vehicle_size,0xa5);transfer(out,v,c,unused,false);lifecycle(out,life,false);return out;
}
inline Bytes vehicle_bytes(CameraVehicle v){CameraState unused;Bytes c(camera_size,0xa5),out(vehicle_size,0xa5);transfer(c,out,unused,v,false);return out;}
inline Bytes pmd_bytes(SourceAllocationBank& bank,SourceAllocationRef ref){auto b=bank.bytes(ref);return {b.begin(),b.end()};}
inline CameraMountedModelAxes axes(SourcePmdModelView m){auto t=m.part(m.source_lod_index(0),0).transform().value();return {std::bit_cast<CameraVector>(t.up),std::bit_cast<CameraVector>(t.forward)};}
inline CameraVehicle vehicle(){CameraVehicle v;v.active=1;v.position={11,22,33,44};v.right={1,0,0,55};v.up={0,1,0,66};v.forward={0,0,1,77};
    initialize_model_matrix_identity(v.orientation);v.orbit_offset={3,4,5,std::bit_cast<float>(0xffc12345u)};v.target_offset={6,7,8,9};v.angles={10,11,12,13};
    v.shift={1,2,3,4};v.previous_shift={4,5,6,7};v.distance=123;v.approach_scale=2;v.speed=40;v.camera_mode=12;return v;}
inline CameraState camera(){CameraState c; c.mode=6;c.position={1,2,3,std::bit_cast<float>(0x7fc10001u)};c.target={10,20,30,std::bit_cast<float>(0xffc20002u)};
    c.up={40,50,60,std::bit_cast<float>(0x7fc30003u)};c.previous_target={70,80,90,100};c.twist=99;c.collision_bypass_ticks=42;
    c.velocity={2,3,4,5};c.drift={1,2,3,4};c.angular_velocity={1,2,3,4};c.angular_acceleration={2,3,4,5};c.control_angles={70,80,90};
    c.lens=2;c.near_clip=1;c.far_factor=.001f;c.half_width=640;c.half_height=360;return c;}
struct Fixture {
    AssetStore assets;SourceAllocationBank bank;ModelResources models;std::size_t handle;
    Bytes track_bytes;SplineSet splines;contact::TrackGeometry track;std::unique_ptr<RaceSessionState> state;
    explicit Fixture(const std::filesystem::path& root):assets(root),models(assets,root),handle(0),track_bytes(read_file(root/"meshdata/track1.PMD")),splines(track_bytes),track(contact::decode_track_collision(track_bytes)),state(std::make_unique<RaceSessionState>()){
        bank.initialize_42eab0();handle=*models.resolve(models.load_source(bank,"car1"));
        auto pose=models.source_at(handle).part(0,0).transform();auto authored=pose.value();authored.up={.25f,.5f,.75f,9};pose.set(authored);
        state->gravity.unit={0,-1,0,0};state->clock.course_length=10000;state->ranking.count=6;state->camera_bank.count=1;
        for(unsigned i=0;i<6;++i){observe(state->vehicles,i,vehicle());state->vehicles.models[i].model=handle;state->ranking.slots[i]=i;state->vehicles.race[i].place=i;}
        for(unsigned i=0;i<4;++i){auto& c=state->controls.controllers[i];c.camera=camera();c.vehicle=i;c.active=i==0?1u:0u;}
        state->camera_bank.records[0]={{1,2,3,4},{10,20,30,40},{0,1,0,99},3,1,7,0};state->shared_model_identity.fill(17);
    }
    SourcePmdModelView model(){return models.source_at(handle);}
    CameraFrameWorld world(){return camera_frame_world(*state,splines,track,&models);}
};
#ifndef SGN_MOUNTED_FROZEN
struct Hook {
    std::function<void(unsigned)> fn;std::vector<unsigned> sites;
    CameraMountedAudit audit(){return {this,[](void* p,unsigned site){auto& h=*static_cast<Hook*>(p);h.sites.push_back(site);h.fn(site);}};}
};
inline void call_frame(CameraFrameWorld& world,CameraMountedAudit* audit=nullptr){camera_frame_before_world(world,audit);}
#endif
inline void expected_prefix(CameraState& c,CameraVehicle& v,const CameraMountedModelAxes& a,bool second){
    c.previous_target=c.target;v.orbit_offset.x=0;v.orbit_offset.y=100;v.orbit_offset.z=0;
    // Contract fixtures have identity basis/orientation, position11,22,33. These
    // expected stores are independent of the production mounted arithmetic.
    c.position.x=11;c.position.y=122;c.position.z=33;
    if(second){c.up.x=-a.up.x;c.up.y=a.up.y;c.up.z=a.up.z;}
}
}
