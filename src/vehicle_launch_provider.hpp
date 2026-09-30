#pragma once
#include "sgn/vehicle_model_effects.hpp"
namespace sgn::retail::detail {
struct LaunchProvider;
void append_launch_trail(VehicleLaunchBindings,VehicleVector,std::span<VehicleVector>,std::uint32_t&,std::uint32_t&,LaunchProvider);
struct LaunchProvider {
    const void* context;
    VehicleTrailModel (*read_basis)(const void*);
    std::uint32_t (*read_head)(const void*);
    Vec3 (*read_position)(const void*,std::uint32_t,unsigned);
    void (*update_coordinates)(const void*,unsigned);
    void (*observe)(const void*,std::uint32_t,std::uint32_t) noexcept;
    VehicleTrailModel basis_inputs() const{return read_basis(context);}
    std::uint32_t selected_head() const{return read_head(context);}
    Vec3 point(std::uint32_t head,unsigned index) const{return read_position(context,head,index);}
    void animate(unsigned index) const{update_coordinates(context,index);}
    void checkpoint(std::uint32_t address,std::uint32_t value) const noexcept{observe(context,address,value);}
    void append(VehicleLaunchBindings b,VehicleVector p,std::span<VehicleVector> ring,std::uint32_t& cursor,std::uint32_t& count) const{
        append_launch_trail(b,p,ring,cursor,count,*this);
    }
};
void run_launch(VehicleLaunchBindings,VehicleModelEffects&,const VehicleAnchorState&,LaunchProvider,contact::effects::ImpactParticles&,VehicleRandom&);
}
