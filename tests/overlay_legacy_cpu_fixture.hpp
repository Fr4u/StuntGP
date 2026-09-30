#pragma once
#include "sgn/race_overlay_textures.hpp"
namespace overlay_legacy_cpu {
using namespace sgn;using namespace sgn::retail;
void require(bool v,const char* m){if(!v)throw std::runtime_error(m);}
struct Device final:TextureReplacementDevice{
    std::uint16_t width{256};unsigned uploads{};std::uint32_t observed_selection{};RaceOverlayTextures* bank{};bool fail{};
    std::uint16_t destination_width(std::uint32_t) const override{return width;}
    void upload_to_destination(std::uint32_t,const Image&) override{++uploads;observed_selection=bank->selected_overlay_texture();if(fail)throw std::runtime_error("injected texture device failure");}
};
template<class F>void rejects(F f){bool failed=false;try{f();}catch(const std::exception&){failed=true;}require(failed,"invalid texture operation accepted");}

inline unsigned run(const AssetStore& assets,const std::shared_ptr<RaceSessionPlatform>& platform){unsigned contracts=0;
    Device probe;RaceOverlayTextures bank(assets,probe,platform);probe.bank=&bank;require(bank.selected_overlay_texture()==0,"selection not initialized by source reset");++contracts;
    for(unsigned id=16;id<=25;++id){bank.replace_source_texture(15,id);require(probe.observed_selection==id&&bank.source_image(id).rgba.size()==262144,"selection not published before upload");++contracts;}
    const auto uploads=probe.uploads;bank.replace_source_texture(15,20);require(probe.uploads==uploads+1,"texture callee invented duplicate suppression");++contracts;
    probe.width=255;const auto count=probe.uploads;bank.replace_source_texture(15,16);require(probe.uploads==count&&bank.selected_overlay_texture()==20,"width mismatch changed image or selection");++contracts;
    probe.width=256;bank.replace_source_texture(14,17);require(bank.selected_overlay_texture()==20,"non15destination changed selection");++contracts;
    probe.fail=true;rejects([&]{bank.replace_source_texture(15,18);});require(bank.selected_overlay_texture()==18&&probe.observed_selection==18,"device failure rolled back source selection");++contracts;
    probe.fail=false; // Invalid-ID guards must not pass via injected upload failure.
    rejects([&]{bank.replace_source_texture(15,15);});++contracts;
    rejects([&]{bank.replace_source_texture(15,30);});++contracts;
return contracts;}
}
