#include "sgn/race_session_world_tick.hpp"
#include "sgn/race_overlay_textures.hpp"
#include <cmath>
#include <iostream>

namespace {
using namespace sgn;using namespace sgn::retail;
void require(bool value,const char* error){if(!value)throw FormatError(error);}
// Real decoded image replacement in CPU memory for this headless integration
// check. GPU upload has its own owner/gate and is not claimed by this test.
class ImageStorage final:public TextureReplacementDevice {
public:
    Image image;unsigned uploads{};
    explicit ImageStorage(const AssetStore& assets):image(assets.texture("Overlays\\Start0")){}
    std::uint16_t destination_width(std::uint32_t slot)const override{
        require(slot==15&&image.width==256,"unexpected overlay destination");return 256;
    }
    void upload_to_destination(std::uint32_t slot,const Image& source)override{
        require(slot==15,"unexpected image replacement slot");image=source;++uploads;
    }
};
struct Trace {
    std::array<unsigned,7> before{},after{};unsigned before_count{},after_count{},world_stages{};bool overflow{};
};
}
int main(int argc,char** argv)try{
    if(argc!=2)throw FormatError("expected retail root");
    unsigned session_count=0;
    for(const auto level:{"TRACK1","TRACK7","TRACK24"}){
        RaceSessionLaunch launch;launch.master_gain=0;launch.level_name=level;RaceSession session(argv[1],launch);auto& s=session.state();
        ImageStorage images(session.assets());RaceOverlayTextures textures(session.assets(),images,session.shared_platform());
        const auto tick=s.simulation_tick;const auto gravity=s.gravity.vector;
        Trace trace;
        VehicleWorldTickAudit world_audit{nullptr,&trace,[](void* p,unsigned,unsigned)noexcept{++static_cast<Trace*>(p)->world_stages;}};
        SessionWorldTickAudit audit{&world_audit,nullptr,nullptr,&trace,
            [](void* p,unsigned entry)noexcept{auto& t=*static_cast<Trace*>(p);if(t.before_count==t.before.size())t.overflow=true;else t.before[t.before_count++]=entry;},
            [](void* p,unsigned entry)noexcept{auto& t=*static_cast<Trace*>(p);if(t.after_count==t.after.size())t.overflow=true;else t.after[t.after_count++]=entry;}};
        // An explicit empty resource set is valid for the actual initial bank:
        // all recovered vehicle emitters publish animation_binding zero.
        const std::span<const contact::effects::EffectAnimationFrames> animations;
        tick_session_world(session,textures,animations,&audit);
        const std::array<unsigned,7> expected{0x405210,0x427140,0x45e1d0,0x415df0,0x443a80,0x418a90,0x401770};
        require(!trace.overflow&&trace.before_count==7&&trace.after_count==7&&trace.before==expected&&trace.after==expected,"world stage order or completion");
        require(trace.world_stages>1&&s.current_slot==0,"real vehicle world did not finish");
        require(s.simulation_tick==tick,"world group changed outer-owned simulation clock");
        require(s.clock.countdown_pending==3&&s.clock.game_mode==1&&s.clock.phase==2,"first actual prestart transition");
        require(gravity.x==s.gravity.vector.x&&gravity.y==s.gravity.vector.y&&gravity.z==s.gravity.vector.z&&s.gravity.factor==4,"world gravity owner changed");
        for(const auto& car:s.vehicles.physics)require(std::isfinite(car.position.x)&&std::isfinite(car.position.y)&&std::isfinite(car.position.z),"nonfinite world position");
        require(images.uploads==0,"first prestart group unexpectedly replaced overlay");
        ++session_count;std::cerr<<level<<" stages="<<trace.before_count<<" vehicle_stages="<<trace.world_stages<<" clock="<<tick<<'\n';
    }
    std::cout<<"{\"passed\":true,\"actual_asset_sessions\":"<<session_count<<",\"world_groups\":3,\"source_parity\":false,\"gpu_upload_verified\":false,\"playable_acceptance\":false}\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
