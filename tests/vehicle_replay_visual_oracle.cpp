#include "vehicle_integration_fixture.hpp"
#include "sgn/race_session_vehicle_world.hpp"
namespace {
sgn::retail::VehicleTickObservation invoke(sgn::retail::RaceSessionState& state,
    const sgn::retail::RaceSessionVehicleResources& resources,sgn::retail::VehicleTickAudit* audit){
    using namespace sgn::retail;
    auto& fixture=*static_cast<integration_test::Fixture*>(audit->call_context);
    fixture.compare_launch_position_provenance=true;
    VehicleWheelPositionWriteAudit wheel{&fixture,[](void* context,unsigned index,const float& value) noexcept{
        auto& f=*static_cast<integration_test::Fixture*>(context);
        constexpr std::array<unsigned,4> writers{0x41453b,0x4145d2,0x414667,0x414706};
        const auto slot=f.state->current_slot;
        if(index>=4||slot>=6||f.additional_retained_count==f.additional_retained.size()||
            &value!=&f.state->vehicles.presentation[slot].wheel_centers[index].w){f.trail_invalid=true;return;}
        f.additional_retained[f.additional_retained_count++]={whole_tick_test::car_base-whole_tick_test::global_base+
            slot*whole_tick_test::car_stride+0x2028+index*0x98,writers[index]};
    }};
    audit->wheel_positions=&wheel;
    sgn::retail::tick_session_replay_visuals(state,resources,audit);
    return {}; //418970 has no progression or air-dynamics call.
}
}
int main(int argc,char** argv){
    return sgn::retail::integration_test::run_corpus(argc,argv,"SGNRV001",invoke);
}
