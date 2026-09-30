#pragma once
#include "sgn/retail_audio_driver.hpp"
#include <bit>
#include <functional>
#include <vector>

namespace pause_menu_test {
namespace audio_boundary {
namespace r=sgn::retail;
struct Event {
    std::uint32_t operation;
    std::uint64_t object;
    std::vector<std::uint32_t> arguments;
    std::vector<std::uint32_t> state_before;
    std::uint32_t result;
    std::uint64_t output;
};

// One actual AudioDevice dispatch per request. The receiver may be the existing
// scripted external OS fixture or the real device; no source game function is
// bypassed. Per-call effects execute at the OS return boundary, not through the
// AudioDriver's read-only observer. This permits the input-latch discriminators.
class RecordingDevice final:public r::AudioDevice {
    r::AudioDevice& receiver_;
    Event begin(std::uint32_t operation,std::uint64_t object,std::initializer_list<std::uint32_t> args){
        Event event{operation,object,{args.begin(),args.end()},{},0,0};
        if(capture)event.state_before=capture();return event;
    }
    std::uint32_t finish(Event event,std::uint32_t result,std::uint64_t output=0){
        event.result=result;event.output=output;const auto index=events.size();events.push_back(std::move(event));
        if(on_call)on_call(index);return result;
    }
    static std::uint32_t bits(std::int32_t value){return std::bit_cast<std::uint32_t>(value);}
    static std::uint32_t bits(float value){return std::bit_cast<std::uint32_t>(value);}
    static r::AudioResult hr(std::uint32_t value){return std::bit_cast<r::AudioResult>(value);}
public:
    std::vector<Event> events;
    std::function<std::vector<std::uint32_t>()> capture;
    std::function<void(std::size_t)> on_call;
    explicit RecordingDevice(r::AudioDevice& receiver):receiver_(receiver){}
    r::AudioResult status(r::AudioBufferId id,std::uint32_t& flags)override{
        auto event=begin(0,id.value,{});const auto result=receiver_.status(id,flags);return hr(finish(std::move(event),bits(result),flags));
    }
    r::AudioResult duplicate(r::AudioBufferId id,r::AudioBufferId& output)override{
        auto event=begin(1,id.value,{});const auto result=receiver_.duplicate(id,output);return hr(finish(std::move(event),bits(result),output.value));
    }
    r::AudioResult query_spatial(r::AudioBufferId id,r::AudioSpatialId& output)override{
        auto event=begin(2,id.value,{});const auto result=receiver_.query_spatial(id,output);return hr(finish(std::move(event),bits(result),output.value));
    }
    std::uint32_t release(r::AudioBufferId id)override{
        auto event=begin(3,id.value,{});return finish(std::move(event),receiver_.release(id));
    }
    r::AudioResult frequency(r::AudioBufferId id,std::uint32_t value)override{
        auto event=begin(4,id.value,{value});return hr(finish(std::move(event),bits(receiver_.frequency(id,value))));
    }
    r::AudioResult volume(r::AudioBufferId id,std::int32_t value)override{
        auto event=begin(5,id.value,{bits(value)});return hr(finish(std::move(event),bits(receiver_.volume(id,value))));
    }
    r::AudioResult pan(r::AudioBufferId id,std::int32_t value)override{
        auto event=begin(6,id.value,{bits(value)});return hr(finish(std::move(event),bits(receiver_.pan(id,value))));
    }
    r::AudioResult play(r::AudioBufferId id,std::uint32_t first,std::uint32_t second,std::uint32_t flags)override{
        auto event=begin(7,id.value,{first,second,flags});return hr(finish(std::move(event),bits(receiver_.play(id,first,second,flags))));
    }
    r::AudioResult stop(r::AudioBufferId id)override{
        auto event=begin(8,id.value,{});return hr(finish(std::move(event),bits(receiver_.stop(id))));
    }
    r::AudioResult spatial_parameters(r::AudioSpatialId id,const r::AudioSpatialParameters& parameters,std::uint32_t apply)override{
        const auto words=std::bit_cast<std::array<std::uint32_t,16>>(parameters);auto event=begin(9,id.value,{});
        event.arguments.assign(words.begin(),words.end());event.arguments.push_back(apply);
        return hr(finish(std::move(event),bits(receiver_.spatial_parameters(id,parameters,apply))));
    }
    r::AudioResult spatial_mode(r::AudioSpatialId id,std::uint32_t mode,std::uint32_t apply)override{
        auto event=begin(10,id.value,{mode,apply});return hr(finish(std::move(event),bits(receiver_.spatial_mode(id,mode,apply))));
    }
    r::AudioResult spatial_position(r::AudioSpatialId id,sgn::Vec3 value,std::uint32_t apply)override{
        auto event=begin(11,id.value,{bits(value.x),bits(value.y),bits(value.z),apply});
        return hr(finish(std::move(event),bits(receiver_.spatial_position(id,value,apply))));
    }
    r::AudioResult spatial_velocity(r::AudioSpatialId id,sgn::Vec3 value,std::uint32_t apply)override{
        auto event=begin(12,id.value,{bits(value.x),bits(value.y),bits(value.z),apply});
        return hr(finish(std::move(event),bits(receiver_.spatial_velocity(id,value,apply))));
    }
    std::uint32_t release_spatial(r::AudioSpatialId id)override{
        auto event=begin(13,id.value,{});return finish(std::move(event),receiver_.release_spatial(id));
    }
    r::AudioResult listener_position(r::AudioListenerId id,sgn::Vec3 value,std::uint32_t apply)override{
        auto event=begin(14,id.value,{bits(value.x),bits(value.y),bits(value.z),apply});
        return hr(finish(std::move(event),bits(receiver_.listener_position(id,value,apply))));
    }
    r::AudioResult listener_velocity(r::AudioListenerId id,sgn::Vec3 value,std::uint32_t apply)override{
        auto event=begin(15,id.value,{bits(value.x),bits(value.y),bits(value.z),apply});
        return hr(finish(std::move(event),bits(receiver_.listener_velocity(id,value,apply))));
    }
    r::AudioResult listener_orientation(r::AudioListenerId id,sgn::Vec3 front,sgn::Vec3 up,std::uint32_t apply)override{
        auto event=begin(16,id.value,{bits(front.x),bits(front.y),bits(front.z),bits(up.x),bits(up.y),bits(up.z),apply});
        return hr(finish(std::move(event),bits(receiver_.listener_orientation(id,front,up,apply))));
    }
};
}
}
