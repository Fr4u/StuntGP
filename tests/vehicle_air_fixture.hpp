#pragma once
#include "world_template_fixture.hpp"
#include "sgn/vehicle_air_tick.hpp"
#include "contact_takeoff_state.hpp"
#include "vehicle_oracle_state.hpp"
#include <algorithm>
#include <type_traits>

// Shared test-only fixture for the full air/dynamics/replay compositions.
// Runtime state uses typed owners; these offsets are not a runtime ABI.
namespace sgn::retail::air_oracle {
inline constexpr unsigned size=0x10800,vehicle_at=0x5000,bank_at=0xd000;
using Record=std::array<std::uint8_t,size>;
inline std::uint32_t u(const Record& b,unsigned at){std::uint32_t x;std::memcpy(&x,b.data()+at,4);return x;}
inline void put(Record& b,unsigned at,std::uint32_t x){std::memcpy(b.data()+at,&x,4);}
inline float f(const Record& b,unsigned at){return std::bit_cast<float>(u(b,at));}
inline std::int32_t i(const Record& b,unsigned at){return std::bit_cast<std::int32_t>(u(b,at));}
static_assert(sizeof(VehicleScorePopup)==0x138 && std::is_trivially_copyable_v<VehicleScorePopup>);
struct Audio final:VehicleScoreAudio,VehicleStuntChainAudio {
    Record& record;
    explicit Audio(Record& b):record(b){}
    void request(std::uint32_t id,std::uint32_t volume,std::uint32_t pan,std::uint32_t force) {
        const auto count=u(record,0x14c);if(count>=8)throw std::runtime_error("air audio request capacity");
        const std::array<std::uint32_t,6> values={id,0,volume,pan,force,0};
        for(unsigned j=0;j<6;++j)put(record,0x1700+count*24+j*4,values[j]);
        put(record,0x14c,count+1);put(record,0x150,id);put(record,0x154,volume);
    }
    void play(std::uint32_t id,std::uint32_t volume) override {request(id,volume,255,0);}
    void play_chain(std::uint32_t id) override {request(id,255,0,1);}
};
struct Fixture {
    Record& record;
    VehicleParameters parameters{};
    VehicleState physics{};
    VehicleAirTickState air{};
    VehicleScoreState score{};
    contact::player::Controls controls{};
    VehicleRandom random{};
    contact::effects::EffectParticleBank particles{};
    Audio audio;
    VehicleStuntContext stunt;
    std::array<std::uint32_t,33> points{};
    std::array<std::string,93> messages;
    VehicleStuntRewards rewards{};
    explicit Fixture(Record& b):record(b),audio(b),stunt{i(b,0),i(b,4),i(b,8),i(b,12),u(b,0x24),u(b,0x28),{},audio} {
        if(u(b,0x34)>=6 || u(b,0x38)>=4)throw std::runtime_error("air vehicle or controller identity");
        auto vehicle=std::span(b).subspan(vehicle_at,0x7800);oracle::transfer(vehicle,parameters,physics,true);
        air.completed_airtime=u(b,vehicle_at+0x7200);
        std::memcpy(score.popups.data(),b.data()+0x200,sizeof(score.popups));
        score.sound_class=u(b,0x140);score.sound_id=u(b,0x144);score.sound_volume=u(b,0x148);
        controls.current=u(b,0x38);
        for(unsigned j=0;j<4;++j) {
            auto& c=controls.controllers[j];const auto at=0x2000+j*0x74c;
            c.active=i(b,at);c.vehicle=u(b,at+0xac);std::memcpy(c.notices.data(),b.data()+at+0x4ac,sizeof(c.notices));
            if(c.vehicle>=6)throw std::runtime_error("air controller vehicle identity");
            for(const auto& notice:c.notices)if(notice.source_vehicle>=6)throw std::runtime_error("air notice vehicle identity");
        }
        for(const auto& popup:score.popups)if(popup.vehicle>=6)throw std::runtime_error("air popup vehicle identity");
        const auto* text=reinterpret_cast<const char*>(b.data()+0x40);unsigned length=0;
        while(length<64 && text[length])++length;if(length==64)throw std::runtime_error("air notice unterminated");
        stunt.inversion_text={text,length};
        for(unsigned j=0;j<33;++j)points[j]=u(b,0x1600+j*4);
        rewards.points=points;rewards.normal_energy=f(b,0x190);rewards.upgraded_energy=f(b,0x194);
        for(unsigned j=0;j<messages.size();++j){messages[j]="STUNT "+std::to_string(j);rewards.messages[j]=messages[j];}
        random.state=u(b,0x12c);particles.active_signal=u(b,0x10400);
        contact::takeoff_oracle::transfer_bank(std::span(b).subspan(bank_at,0x3400),particles,true);
    }
    VehicleAirTickContext context(){return {controls,u(record,0x34),score,stunt,rewards,random,audio,particles,
        {f(record,0x120),f(record,0x124),f(record,0x128)},sgn::retail::fixture::cold_world_template};}
    void save() {
        auto& b=record;auto vehicle=std::span(b).subspan(vehicle_at,0x7800);oracle::transfer(vehicle,parameters,physics,false);
        put(b,vehicle_at+0x7200,air.completed_airtime);put(b,0x100,std::bit_cast<std::uint32_t>(physics.score));
        put(b,0x10c,physics.stunt.inversion_latch);put(b,0x110,physics.stunt.inversion_timer);
        put(b,0x134,std::bit_cast<std::uint32_t>(physics.stunt.maximum_height));put(b,0x13c,physics.stunt.height_level);
        put(b,0x12c,random.state);std::memcpy(b.data()+0x200,score.popups.data(),sizeof(score.popups));
        put(b,0x140,score.sound_class);put(b,0x144,score.sound_id);put(b,0x148,score.sound_volume);put(b,0x38,controls.current);
        for(unsigned j=0;j<4;++j)std::memcpy(b.data()+0x2000+j*0x74c+0x4ac,controls.controllers[j].notices.data(),sizeof(controls.controllers[j].notices));
        put(b,0x10400,particles.active_signal);
        contact::takeoff_oracle::transfer_bank(std::span(b).subspan(bank_at,0x3400),particles,false);
    }
    std::vector<unsigned> excluded(const VehicleAirTickObservation& observed) const {
        auto result=contact::takeoff_oracle::excluded(particles,bank_at);
        if(observed.orthogonalized)result.push_back(vehicle_at+0x1bd8);
        if(observed.leveled_incremental)for(unsigned at:{0x1c18u,0x1c28u,0x1c38u})result.push_back(vehicle_at+at);
        std::sort(result.begin(),result.end());return result;
    }
};
}
