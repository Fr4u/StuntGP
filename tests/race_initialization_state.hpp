#pragma once
#include "sgn/retail_race_initialization.hpp"
#include "retail_camera_state.hpp"
#include <cstring>

namespace sgn::retail::race_initialization_test {
constexpr std::size_t controller_stride=0x74c;
using Controllers=std::array<std::uint8_t,4*controller_stride>;
using Globals=std::array<std::uint32_t,0x300/4>;
inline void controllers(Controllers& bytes,RaceHudView view,bool reading) {
    const auto field=[&](auto& value,std::size_t at) {
        if(reading)std::memcpy(&value,bytes.data()+at,sizeof(value));
        else std::memcpy(bytes.data()+at,&value,sizeof(value));
    };
    for(unsigned i=0;i<4;++i) {
        const auto base=i*controller_stride,hud=base+0x210;
        auto& controller=view.controls.controllers[i];auto& countdown=view.countdown[i];
        camera_test::Bytes c(camera_test::camera_size),v(camera_test::vehicle_size);CameraVehicle ignored;
        std::copy_n(bytes.begin()+base,0x1e8,c.begin());
        camera_test::transfer(c,v,controller.camera,ignored,reading);
        if(!reading)std::copy_n(c.begin(),0x1e8,bytes.begin()+base);
        field(controller.active,base);field(controller.vehicle,base+0xac);
        field(countdown.active,hud);field(countdown.ticks,hud+4);
        for(unsigned k=0;k<18;++k) {
            auto element=hud_element(view,i,k);const auto at=hud+0x10+k*0x24;
            field(element.layout.x,at);field(element.layout.y,at+4);field(element.active,at+8);
            field(element.value,at+12);field(element.phase,at+16);field(element.layout.word_14,at+20);
            field(element.layout.word_18,at+24);field(element.layout.word_1c,at+28);field(element.layout.word_20,at+32);
        }
        static_assert(sizeof(contact::player::Notice)==0x54);
        if(reading)std::memcpy(controller.notices.data(),bytes.data()+base+0x4ac,0x2a0);
        else std::memcpy(bytes.data()+base+0x4ac,controller.notices.data(),0x2a0);
    }
}
inline void globals(Globals& data,RaceInitializationWorld world,bool reading) {
    const auto field=[&](auto& value,std::size_t at) {
        if(reading)std::memcpy(&value,reinterpret_cast<std::uint8_t*>(data.data())+at,sizeof(value));
        else std::memcpy(reinterpret_cast<std::uint8_t*>(data.data())+at,&value,sizeof(value));
    };
    field(world.reset.current,0);field(world.load_selection.template_vehicle,4);field(world.load_selection.configuration_id,8);
    field(world.hud.controls.current,0xc);field(world.hud.selected_hud,0x10);field(world.selected_camera_preset,0x14);
    field(world.clock.game_mode,0x18);field(world.clock.phase,0x1c);field(world.clock.race_mode,0x20);field(world.clock.course_length,0x24);
    field(world.automatic.initialized,0x28);field(world.state.field_5898c8,0x2c);field(world.state.field_60f2c0,0x30);field(world.state.start_delay_ticks,0x34);
    field(world.music.selected,0x38);field(world.hud.margins.x,0x3c);field(world.hud.margins.y,0x40);field(world.counters.field_60d71c,0x44);
    field(world.counters.fields_485650,0x48);field(world.state.fields_62dcf0,0x80);field(world.state.fields_62d480,0x180);
    field(world.frame_projection.matrix,0x280);field(world.frame_projection.inverse_depth_scale,0x2c0);field(world.frame_projection.depth_offset,0x2c4);
}
}
