#include "sgn/race_overlay_textures.hpp"
#include "overlay_legacy_cpu_fixture.hpp"
#include <array>
#include <functional>
#include <iostream>
#include <map>
#include <type_traits>
using namespace sgn;using namespace sgn::retail;
extern unsigned overlay_renderer_tripwire_calls;
namespace {
unsigned checks=0,resets=0;
void require(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
bool same_image(const Image& a,const Image& b){return a.width==b.width&&a.height==b.height&&a.rgba==b.rgba;}
struct Device final:TextureReplacementDevice {
    std::map<unsigned,Image> destinations;
    std::map<unsigned,unsigned> bindings{{11,211},{14,214},{15,215}};
    mutable unsigned width_calls{};unsigned uploads{},width=256,observed{};
    RaceOverlayTextures* bank{};bool reject_calls{},fail_before{},fail_after{};
    std::function<void()> callback;
    std::uint16_t destination_width(std::uint32_t slot)const override {
        if(reject_calls)throw FormatError("reset unexpectedly queried device");
        ++width_calls;if(!bindings.contains(slot))throw FormatError("unbound fixture destination");
        return static_cast<std::uint16_t>(width);
    }
    void upload_to_destination(std::uint32_t slot,const Image& image)override {
        if(reject_calls)throw FormatError("reset unexpectedly uploaded");
        ++uploads;observed=bank->selected_overlay_texture();
        if(fail_before)throw FormatError("injected before device copy");
        destinations[slot]=image;
        if(callback)callback();
        if(fail_after)throw FormatError("injected after device copy");
    }
};
template<class F>void rejects(F&& call){bool thrown=false;try{call();}catch(const FormatError&){thrown=true;}require(thrown,"expected device failure missing");}
struct Snapshot {
    std::array<Image,14> images;
    std::array<const Image*,14> objects;
    std::array<const std::uint8_t*,14> pixels;
    std::array<std::optional<std::uint32_t>,255> completed;
    std::map<unsigned,Image> destinations;
    std::map<unsigned,unsigned> bindings;
    unsigned widths{},uploads{};
};
Snapshot snapshot(const RaceOverlayTextures& bank,const Device& device){
    Snapshot s{};
    for(unsigned i=0;i<14;++i){const auto& image=bank.source_image(i+16);s.images[i]=image;s.objects[i]=&image;s.pixels[i]=image.rgba.data();}
    for(unsigned i=0;i<255;++i)s.completed[i]=bank.completed_source(i);
    s.destinations=device.destinations;s.bindings=device.bindings;s.widths=device.width_calls;s.uploads=device.uploads;
    return s;
}
void retained(const Snapshot& before,const RaceOverlayTextures& bank,const Device& device){
    for(unsigned i=0;i<14;++i){const auto& image=bank.source_image(i+16);
        require(same_image(before.images[i],image)&&before.objects[i]==&image&&before.pixels[i]==image.rgba.data(),"reset changed source image bytes/storage");}
    for(unsigned i=0;i<255;++i)require(before.completed[i]==bank.completed_source(i),"reset changed completed-source history");
    require(before.bindings==device.bindings&&before.widths==device.width_calls&&before.uploads==device.uploads,"reset touched device bindings/calls");
    require(before.destinations.size()==device.destinations.size(),"reset changed destination count");
    for(const auto& [slot,image]:before.destinations)require(same_image(image,device.destinations.at(slot)),"reset changed actual CPU destination pixels");
}
void reset(RaceOverlayTextures& bank,Device& device){
    const auto before=snapshot(bank,device);device.reject_calls=true;
    bank.reset_source_selection_44600f();device.reject_calls=false;
    require(bank.selected_overlay_texture()==0,"44600F did not clear selected source word");retained(before,bank,device);++resets;
}
}
int main(int argc,char** argv)try {
    static_assert(!noexcept(std::declval<RaceOverlayTextures&>().reset_source_selection_44600f()));
    require(argc==2,"expected retail AssetStore root");const AssetStore assets(argv[1]);
    auto legacy_platform=std::make_shared<RaceSessionPlatform>(SourceColdConstructionOnly{},argv[1],0.0f);
    const auto legacy=overlay_legacy_cpu::run(assets,legacy_platform);
    auto platform=std::make_shared<RaceSessionPlatform>(SourceColdConstructionOnly{},argv[1],0.0f);
    auto other_platform=std::make_shared<RaceSessionPlatform>(SourceColdConstructionOnly{},argv[1],0.0f);
    Device device,other_device;RaceOverlayTextures bank(assets,device,platform),other(assets,other_device,other_platform);device.bank=&bank;other_device.bank=&other;
    other.replace_source_texture(15,25);const auto foreign=snapshot(other,other_device);
    reset(bank,device);reset(bank,device); // Cold zero and repeated zero.
    bank.replace_source_texture(15,16);bank.replace_source_texture(11,28);bank.replace_source_texture(14,29);
    require(device.observed==16&&bank.selected_overlay_texture()==16&&bank.completed_source(15)==16u,"baseline source publication");
    reset(bank,device);require(other.selected_overlay_texture()==25,"reset mutated foreign owner");retained(foreign,other,other_device);
    bank.replace_source_texture(15,17);require(device.observed==17&&bank.completed_source(15)==17u,"replacement after reset failed");
    device.width=255;const auto rejected=snapshot(bank,device);bank.replace_source_texture(15,26);
    require(bank.selected_overlay_texture()==17&&bank.completed_source(15)==17u&&device.uploads==rejected.uploads,"width guard changed publication");
    reset(bank,device);device.width=256;
    device.fail_before=true;rejects([&]{bank.replace_source_texture(15,18);});device.fail_before=false;
    require(bank.selected_overlay_texture()==18&&device.observed==18&&bank.completed_source(15)==17u&&same_image(device.destinations.at(15),bank.source_image(17)),"430D93 failure prefix before copy");
    reset(bank,device);
    device.fail_after=true;rejects([&]{bank.replace_source_texture(15,26);});device.fail_after=false;
    require(bank.selected_overlay_texture()==26&&device.observed==26&&bank.completed_source(15)==17u&&same_image(device.destinations.at(15),bank.source_image(26)),"430D93 failure prefix after copy");
    reset(bank,device);
    device.callback=[&]{reset(bank,device);};bank.replace_source_texture(15,19);device.callback={};
    require(device.observed==19&&bank.selected_overlay_texture()==0&&bank.completed_source(15)==19u,"post-callback continuation overwrote reset or lost completion");
    device.fail_after=true;device.callback=[&]{reset(bank,device);};rejects([&]{bank.replace_source_texture(15,20);});device.callback={};device.fail_after=false;
    require(device.observed==20&&bank.selected_overlay_texture()==0&&bank.completed_source(15)==19u&&same_image(device.destinations.at(15),bank.source_image(20)),"failed callback continuation lost source reset/history");
    bank.replace_source_texture(15,20);require(bank.selected_overlay_texture()==20&&bank.completed_source(15)==20u,"same-source retry after reset was suppressed");
    reset(bank,device);retained(foreign,other,other_device);require(other.selected_overlay_texture()==25,"late foreign selection changed");
    require(overlay_renderer_tripwire_calls==0,"native renderer tripwire reached");
    std::cout<<"OVERLAY_SCALAR_RESET_PASS legacy_contracts="<<legacy<<" resets="<<resets<<" checks="<<checks<<" images=14 renderer_calls="<<overlay_renderer_tripwire_calls<<" CPU_only=1\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
