#include "capture.hpp"
#include "external_audio.hpp"
#include <iostream>

namespace p=pause_menu_test;
int main(int argc,char** argv){
    try{
        if(argc!=7)throw sgn::FormatError("usage: pause_menu_native inputs.bin group source-initial external-inputs asset-root fresh-output");
        const auto groups=p::input::read(argv[1]);const unsigned index=static_cast<unsigned>(std::stoul(argv[2]));
        p::input::require(index<groups.size(),"pause native group index");const auto& group=groups[index];
        const std::filesystem::path seed=argv[3],external=argv[4],asset_root=argv[5],output=argv[6];
        p::input::require(!std::filesystem::exists(output),"fresh native attempt required");std::filesystem::create_directories(output);
        const sgn::AssetStore assets(asset_root);p::r::ModelResources models(assets,asset_root);
        p::os::Record device_record{};const auto audio_seed=sgn::read_file(seed/"audio.bin");
        p::input::require(audio_seed.size()==sizeof(device_record),"pause OS record extent");std::memcpy(device_record.data(),audio_seed.data(),audio_seed.size());
        p::os::Device platform(device_record);p::ExternalAudio receiver(platform);p::audio_boundary::RecordingDevice audio(receiver);
        p::ExternalStream streaming(platform);
        // This large fixture is heap-owned; its referenced members keep stable
        // addresses throughout all retained menu invocations.
        auto owned=std::make_unique<p::NativeFixture>(assets,models,seed,asset_root,audio,platform,streaming,streaming,platform);auto& fixture=*owned;
        p::import_descriptor_seed(fixture,seed,external);p::StateCapture capture(seed,external);
        p::InputLocations locations(fixture.menu);
        for(const auto& [address,owner]:fixture.extra_fields())if(address!=0x4835e0)locations.representation_word(address,owner);
        locations.music_word(0x62df08,fixture.maintenance.music.buffer.value);
        for(unsigned i=0;i<4;++i){locations.word(0x62d598+i*0x1dc,fixture.frontend.input_bindings[i].vehicle);
            locations.word(0x62d59c+i*0x1dc,fixture.frontend.input_bindings[i].kind);}
        capture.save(fixture,output/"initial");unsigned calls=0,events=0;
        for(unsigned i=0;i<group.steps.size();++i){const auto& step=group.steps[i];
            receiver.responses=p::read_audio_responses(external/("audio-"+std::to_string(i)+".bin"));receiver.cursor=0;
            auto init=p::read_initialization_responses(external/("initialization-"+std::to_string(i)+".bin"));platform.expected=init;platform.cursor=0;
            locations.apply(step.patches);p::InputEffects effects(locations,step);std::ofstream events_out(output/("events-"+std::to_string(i)+".jsonl"));
            fixture.draw_device.events.clear();audio.events.clear();
            fixture.draw_device.capture=[&]{return p::core(fixture.menu);};audio.capture=fixture.draw_device.capture;
            fixture.draw_device.on_call=[&](std::size_t n){p::json_draw(events_out,fixture.draw_device.events[n]);effects.boundary(p::input::Boundary::draw);++events;};
            audio.on_call=[&](std::size_t n){const auto& e=audio.events[n];
                if(e.operation==8||e.operation==3)return; // actual initialization OS stream is separately checked
                p::json_audio(events_out,e);effects.boundary(p::input::Boundary::audio);++events;};
            fixture.texture_device.on_upload=[&](std::size_t){effects.boundary(p::input::Boundary::upload);};
            // Different-owner reset.current is an invocation-only argument.
            // Both variants publish to the sole persistent fixture.reset.current.
            std::optional<p::r::VehicleResetWorld> invocation_reset;
            if(group.separate_current)invocation_reset.emplace(fixture.reset);
            auto& reset=invocation_reset?*invocation_reset:fixture.reset;
            p::BorrowedRestart restart(fixture.initialization(reset),fixture.reset_parameters,fixture.cars,fixture.selection,
                fixture.chosen,fixture.control,fixture.scene,fixture.music_track,fixture.reset.current);
            auto transition=fixture.transition();auto results=fixture.results();
            auto world=p::bind_world(fixture.menu,fixture.frontend,fixture.lifecycle(),fixture.initialization(fixture.reset),
                fixture.render_b,fixture.game_type,fixture.detail,fixture.streaming,restart,transition,results,fixture.game_text,
                fixture.textures,fixture.ui.quad,fixture.ui.clip,fixture.ui,fixture.drawing,fixture.draw_device);
            bool guarded=false;
            try{p::invoke(world,step);}catch(const sgn::FormatError& error){
                if(!group.native_guard_only)throw;
                std::ifstream diagnostic(external/("guard-"+std::to_string(i)+".txt"));std::string required;std::getline(diagnostic,required);
                p::input::require(bool(diagnostic)&&!required.empty()&&required.size()<=256&&required==error.what(),"unexpected pause host-guard diagnostic");
                guarded=true;std::ofstream reason(output/("guard-"+std::to_string(i)+".txt"));reason<<error.what()<<'\n';}
            p::input::require(guarded==group.native_guard_only,"pause guard-domain result");
            if(!guarded)effects.require_all_applied();
            p::input::require(receiver.cursor==receiver.responses.size(),"missing pause external sound requests");
            p::input::require(platform.cursor==platform.expected.size(),"missing pause external initialization requests");
            events_out.close();capture.save(fixture,output/("after-"+std::to_string(i)));++calls;
            std::ofstream core_out(output/("core-"+std::to_string(i)+".json"));p::json_words(core_out,p::core(fixture.menu));core_out<<'\n';
            fixture.draw_device.capture={};fixture.draw_device.on_call={};audio.capture={};audio.on_call={};fixture.texture_device.on_upload={};
        }
        std::cout<<"{\"native_execution_completed\":true,\"qualification_pass\":false,\"group\":"<<index<<",\"calls\":"<<calls<<",\"events\":"<<events<<"}\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 2;}
}
