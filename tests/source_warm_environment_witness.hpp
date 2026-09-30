#pragma once
#include "sgn/race_session.hpp"
#include "sgn/retail_scene_transition.hpp"
#include <stdexcept>
#include <cstring>
#include <cstddef>
#include <optional>
#include "source_scene_reset_witness.hpp"
#include <bit>
#include <iostream>

namespace warm_release_receiving {
class EnvironmentWitness {
    sgn::retail::RaceSessionState& state_;
    std::array<std::uint32_t,14> before_;
    std::uint32_t support_;
    bool active_;
    std::uint32_t stop_site_{};
    std::optional<source_scene_reset_receiving::SceneResetWitness> scene_reset_;
    std::uint32_t count_before_{},count_after_{};
    std::uint32_t initialized_before_{},gain_before_{},gain_target_{},gain_after_maintenance_{};
    std::array<std::uint8_t,sizeof(sgn::retail::AudioDriverState)> driver_before_;
    std::array<std::uint8_t,sizeof(sgn::retail::LevelAudioState)> level_before_;
    std::array<std::uint8_t,sizeof(sgn::retail::AudioStreamState)> stream_before_;
    std::array<std::uint8_t,sizeof(sgn::retail::AudioMusicState)> music_before_;
    static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
public:
    explicit EnvironmentWitness(sgn::retail::RaceSession& session,bool disable_spatial=false):state_(session.state()){
        if(disable_spatial){
            require(state_.audio_driver.initialized==1&&state_.audio_initialization.spatial_enabled==1,
                "disabled environment case requires actual spatial-audio startup first");
            state_.audio_initialization.spatial_enabled=0; //explicit receiving guard stimulus
        }
        active_=state_.audio_driver.initialized==1&&state_.audio_initialization.spatial_enabled==1;
        require(state_.audio_stream.playback_active==0||state_.audio_level.output_a==0,"environment receiving requires source music-stop guard");
        std::memcpy(stream_before_.data(),&state_.audio_stream,stream_before_.size());
        std::memcpy(music_before_.data(),&state_.audio_maintenance.music,music_before_.size());
        // These native receiving fixtures are idle immediately after startup.
        // The earlier462280 maintenance must not recycle or change their count.
        // Its460CE0 gain step is expected below from the actual input words.
        require(state_.audio_driver.duplicate_count==0&&state_.audio_driver.playing_count==0,
            "warm audio receiving requires idle startup samples with no duplicates");
        initialized_before_=state_.audio_driver.initialized;
        gain_before_=state_.audio_level.output_b;gain_target_=state_.audio_level.output_b_copy;
        gain_after_maintenance_=gain_before_;
        if(initialized_before_!=0&&gain_before_!=gain_target_){
            const auto signed_bits=[](std::uint32_t word){return std::bit_cast<std::int32_t>(word);};
            const bool rising=signed_bits(gain_before_)<signed_bits(gain_target_);
            const auto stepped=gain_before_+(rising?4u:0xfffffffcu);
            gain_after_maintenance_=(rising?signed_bits(stepped)>signed_bits(gain_target_):
                signed_bits(stepped)<signed_bits(gain_target_))?gain_target_:stepped;
        }
        count_before_=state_.audio_driver.playing_count;count_after_=count_before_;
        stop_site_=active_?0x460a9cu:0x445fa0u;
        if(!active_&&state_.audio_driver.initialized!=0){
            unsigned first=0;
            while(first<128&&state_.audio_driver.samples[first].buffer.value==0)++first;
            count_after_-=first;
            if(first<128)stop_site_=0x460d79u;
        }
        if(stop_site_==0x445fa0u){scene_reset_.emplace(state_,true,true);stop_site_=0x42e6e0u;}
        std::memcpy(driver_before_.data(),&state_.audio_driver,driver_before_.size());
        std::memcpy(level_before_.data(),&state_.audio_level,level_before_.size());
        // Distinct receiving values make every reached preset store observable.
        state_.audio_environment.properties.fill(0xd00dbeefu);state_.pause_flag=3;
        before_=state_.audio_environment.properties;support_=state_.audio_environment.supported_properties;
    }
    bool active()const{return active_;}
    // The scene witness exists only after the earlier460C90 prefix completed.
    bool sound_reset_completed()const{return scene_reset_.has_value();}
    std::optional<std::uint32_t> expected_selected_overlay()const{
        return scene_reset_?std::optional<std::uint32_t>{0}:std::nullopt;
    }
    std::uint32_t blocked_address()const{return stop_site_;}
    const char* retry_reason()const{return active_?"outer source frame duplicated, reentered or poisoned":
        "source outer requires the entered local non-demo race branch";}
    void finished(std::span<const sgn::retail::SceneOperation> missing)const{
        // Golden words from the existing original-instruction corpus env0;
        // proof ties them to actual460760 stores, not this test's implementation.
        constexpr std::array<std::uint32_t,14> outdoor{0xfffffc18,0xfffffce0,0,0x3fbeb852,0x3f2b851f,
            0xfffff71f,0x3be56042,0xfffff965,0x3c343958,0x10,0x40f00000,0x3f000000,0xc0a00000,0x2f};
        require(state_.audio_environment.properties==(active_?outdoor:before_)&&
            state_.audio_environment.supported_properties==support_,"warm environment preset/support prefix mismatch");
        require(state_.scene_mode==(active_?1u:2u)&&state_.options.selection_phase==2&&
            state_.normal_finish.menu_requested==(active_?1u:0u)&&state_.pause_flag==(active_?3u:0u),
            "active and guarded environment calls retained the wrong scene/pending/audio-pause words");
        require(std::memcmp(stream_before_.data(),&state_.audio_stream,stream_before_.size())==0&&
            std::memcmp(music_before_.data(),&state_.audio_maintenance.music,music_before_.size())==0,
            "inactive45F9D0 or earlier environment stop changed retained stream/music state");
        auto driver_expected=driver_before_;auto level_expected=level_before_;
        std::memcpy(level_expected.data()+offsetof(sgn::retail::LevelAudioState,output_b),&gain_after_maintenance_,sizeof(gain_after_maintenance_));
        std::memcpy(driver_expected.data()+offsetof(sgn::retail::AudioDriverState,playing_count),&count_after_,sizeof(count_after_));
        if(scene_reset_){
            const std::uint32_t full=255;
            std::memcpy(level_expected.data()+offsetof(sgn::retail::LevelAudioState,output_b),&full,sizeof(full));
            std::memcpy(level_expected.data()+offsetof(sgn::retail::LevelAudioState,output_b_copy),&full,sizeof(full));
        }
        require(std::memcmp(driver_expected.data(),&state_.audio_driver,driver_expected.size())==0&&
            std::memcmp(level_expected.data(),&state_.audio_level,level_expected.size())==0,
            "warm sample-stop prefix or ordered460C90 stores differ from source");
        std::cout<<"warm_audio_state initialized="<<initialized_before_<<" gain_before="<<gain_before_
            <<" gain_target="<<gain_target_<<" gain_after_maintenance="<<gain_after_maintenance_
            <<" gain_after="<<state_.audio_level.output_b<<" target_after="<<state_.audio_level.output_b_copy
            <<" count_before="<<count_before_<<" count_after="<<state_.audio_driver.playing_count
            <<" stop_site="<<stop_site_<<'\n';
        if(scene_reset_)(void)scene_reset_->verify();
        require(scene_reset_?(missing.size()==1&&missing[0]==sgn::retail::SceneOperation::load_scene_resources):missing.empty(),
            "warm boundary did not identify precisely the next unimplemented whole scene child");
    }
};
}
