#pragma once
#include "sgn/race_session.hpp"
#include <bit>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace source_scene_reset_receiving {
using Bytes=std::vector<unsigned char>;
template<class T> Bytes bytes(const T& value){
    static_assert(std::is_trivially_copyable_v<T>);
    const auto* first=reinterpret_cast<const unsigned char*>(&value);return {first,first+sizeof(T)};
}
// Authored expected transfers from the pinned instructions. This fixture does
// not call any production reset/refill/gravity helper to obtain expected data.
class SceneResetWitness {
    struct Region {const void* actual;Bytes expected;const char* label;};
    std::vector<Region> regions_;
    template<class T,class F> void changed(const T& value,const char* label,F transform){
        static_assert(std::is_trivially_copyable_v<T>);
        auto copy=std::make_unique<T>();std::memcpy(copy.get(),&value,sizeof(T));
        transform(*copy);regions_.push_back({&value,bytes(*copy),label});
    }
    template<class T> void retained(const T& value,const char* label){regions_.push_back({&value,bytes(value),label});}
public:
    explicit SceneResetWitness(const sgn::retail::RaceSessionState& state,bool reset_scalars=false,bool reset_rail=false){
        changed(state.frontend_launch,"60DF90 and adjacent frontend words",[](auto& x){x.catalog_count=0x005096c0u;});
        changed(state.vehicles.race,"six active flags and retained race payload",[](auto& x){for(auto& car:x)car.active=0;});
        changed(state.vehicles.models,"six model/shadow identities and two extra presentation slots",[](auto& x){
            for(unsigned i=0;i<6;++i){x[i].model.reset();x[i].shadow.reset();}
        });
        changed(state.vehicles.physics,"fuel/energy and retained physical fields",[&](auto& x){
            for(unsigned i=0;i<6;++i){x[i].fuel=state.vehicles.parameters[i].fuel_capacity;x[i].energy=x[i].energy_capacity;}
        });
        changed(state.vehicles.contact,"impact count and retained contact payload",[](auto& x){for(auto& car:x)car.impact_count.value=0;});
        changed(state.gravity,"gravity factor/rounded outputs and retained unit W",[](auto& x){
            x.factor=4.0f;x.inverse_factor=0.25f;
            x.magnitude=std::bit_cast<float>(0x3f8b851fu);x.inverse_magnitude=std::bit_cast<float>(0x3f6adcc5u);
            x.vector={0,std::bit_cast<float>(0xbf8b851fu),0};x.unit.x=0;x.unit.y=-1;x.unit.z=0;
        });
        changed(state.cars,"exact600-byte car bank",[](auto& x){x={};});
        changed(state.score_popups,"16 popups and retained sound cache",[](auto& x){x.popups={};});
        changed(state.effect_particles,"64 active-only writes and retained payload/provenance",[](auto& x){
            for(auto& particle:x.particles)particle.active=2;x.active_signal=0;
        });
        retained(state.vehicles.parameters,"vehicle parameters");
        retained(state.vehicles.presentation,"all eight presentation payloads");
        retained(state.vehicles.wheels,"all eight wheel animation records");
        retained(state.selection,"selection immediately outside600-byte car clear");
        retained(state.ghosts.reference_completed,"ghost word immediately following600-byte car clear");
        changed(state.finish,"6 cleared order rows; retained10 rows and best totals",[=](auto& x){
            if(reset_scalars){for(unsigned i=0;i<6;++i)x.order[i].reset();x.count=0;}
        });
        changed(state.impact_particles,"0x8800 particles/counters reset; retained rate selector",[=](auto& x){
            if(reset_scalars){x.emitted=0;x.cursor=0;x.throttle=0;x.particles={};}
        });
        changed(state.impact_draw_list,"sourceNULL slots and cursor, distinct from index/count0",[=](auto& x){
            if(reset_scalars){x.slots.fill(0xffffffffu);x.count=0xffffffffu;}
        });
        retained(state.selected_hud,"distinct61B740 selected HUD word");
        retained(state.ui.selected_material,"distinct61C5E4 material selection");
        changed(state.outer.loop_exit_word_612948,"612948 source loop word",[=](auto& x){if(reset_scalars)x=0;});
        changed(state.outer.source_word_612974,"612974 first reviewed-block word",[=](auto& x){if(reset_scalars)x=0;});
        changed(state.outer.source_word_612978,"612978 second reviewed-block word",[=](auto& x){if(reset_scalars)x=0;});
        changed(state.outer.source_word_612980,"612980 opaque source word",[=](auto& x){if(reset_scalars)x=0;});
        changed(state.outer.render_counter,"612984 existing render counter",[=](auto& x){if(reset_scalars)x=0;});
        changed(state.outer.source_word_612988,"612988 opaque source word",[=](auto& x){if(reset_scalars)x=0;});
        changed(state.outer.source_word_61298c,"61298C source formatter input",[=](auto& x){if(reset_scalars)x=0;});
        changed(state.outer.source_word_612994,"612994 source word",[=](auto& x){if(reset_scalars)x=0;});
        changed(state.outer.source_word_612998,"612998 source word",[=](auto& x){if(reset_scalars)x=0;});
        changed(state.outer.source_word_6129a0,"6129A0 source word",[=](auto& x){if(reset_scalars)x=0;});
        changed(state.outer.source_word_612be4,"612BE4 source word",[=](auto& x){if(reset_scalars)x=0;});
        changed(state.outer.source_word_61b62c,"61B62C source word",[=](auto& x){if(reset_scalars)x=0;});
        changed(state.demo_ticks,"612990 demo ticks",[=](auto& x){if(reset_scalars)x=0;});
        changed(state.hud_block,"61299C HUD block",[=](auto& x){if(reset_scalars)x=0;});
        changed(state.normal_finish.field_612bf0,"612BF0 finish word",[=](auto& x){if(reset_scalars)x=0;});
        changed(state.grid,"571328 borrowed spline identity",[=](auto& x){if(reset_scalars)x.start_spline.reset();});
        changed(state.automatic,"49A164 only, retained route views and payload",[=](auto& x){if(reset_scalars)x.initialized=0;});
        changed(state.ui.loading_tile_base,"61B628 loading tiles",[=](auto& x){if(reset_scalars)x=0;});
        changed(state.rail,"4192D0 rail owner or retained prefix boundary",[=](auto& x){
            if(reset_rail){x.tube_glass_texture.reset();x.spline.reset();x.source_word_4d5ca0=0;}
        });
        changed(state.frame_pause,"61294C frame pause",[=](auto& x){if(reset_scalars)x=0;});
        changed(state.ui.second_pause,"612958 second pause",[=](auto& x){if(reset_scalars)x=0;});
        changed(state.normal_finish.confirm_pressed,"612968 confirmation word",[=](auto& x){if(reset_scalars)x=0;});
    }
    std::size_t verify()const{
        for(const auto& region:regions_)if(std::memcmp(region.actual,region.expected.data(),region.expected.size())!=0)
            throw std::runtime_error(std::string("source445FA0 prefix mismatch: ")+region.label);
        return regions_.size();
    }
    Bytes observed_bytes()const{
        Bytes result;
        for(const auto& region:regions_){const auto* first=static_cast<const unsigned char*>(region.actual);
            result.insert(result.end(),first,first+region.expected.size());}
        return result;
    }
};
}
