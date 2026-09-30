#pragma once
#include "sgn/retail_pause_meter.hpp"
#include "wire.hpp"
#include <bit>
namespace pause_meter_test {
using namespace sgn::retail;namespace wire=pause_meter_wire;
struct State:PauseMeterLabels {
    wire::StateBytes carrier{};RetailFontResources fonts;
    std::uint32_t selected{},specular{},quality{},legacy{};
    ViewportState viewport;
    std::array<PolygonSubmissionMaterial,255> materials{};
    std::array<std::uint32_t,35> quad{};
    std::array<PolygonSubmissionVertex,8> alpha{};
    UiLineClipScratch clip;std::array<std::uint32_t,2> labels{};std::vector<std::uint32_t> reads;
    std::span<const std::uint8_t> label(unsigned i){reads.push_back(labels[i]);return wire::label(labels[i]);}
    std::span<const std::uint8_t> read_left_4851e0()override{return label(0);}
    std::span<const std::uint8_t> read_right_4851e4()override{return label(1);}
    PauseMeterSubmission meter(){return {view(),quad,alpha,clip,*this};}
    TextSubmission view(){return {fonts,selected,specular,materials,quality,legacy,viewport};}
    void transfer(bool reading){
        const auto integer=[&](std::uint32_t& value,std::uint32_t at){if(reading)value=wire::word(carrier,at);else wire::put(carrier,at,value);};
        const auto signed_integer=[&](std::int32_t& value,std::uint32_t at){if(reading)value=std::bit_cast<std::int32_t>(wire::word(carrier,at));else wire::put(carrier,at,std::bit_cast<std::uint32_t>(value));};
        const auto real=[&](float& value,std::uint32_t at){if(reading)value=std::bit_cast<float>(wire::word(carrier,at));else wire::put(carrier,at,std::bit_cast<std::uint32_t>(value));};
        for(unsigned f=0;f<2;++f)for(unsigned at=0;at<wire::font_size;at+=4)integer(fonts.source_word(f,at),f*wire::font_size+at);
        if(reading){std::memcpy(fonts.index.data(),carrier.data()+wire::index,160);std::memcpy(fonts.glyph_map.data(),carrier.data()+wire::map,256);}
        else {std::memcpy(carrier.data()+wire::index,fonts.index.data(),160);std::memcpy(carrier.data()+wire::map,fonts.glyph_map.data(),256);}
        std::array<std::uint32_t*,7> globals={&fonts.current,&fonts.count,&fonts.digit_advance_bits,&selected,&specular,&quality,&legacy};
        for(unsigned i=0;i<globals.size();++i)integer(*globals[i],wire::globals+4*i);
        auto at=wire::viewport;const auto si=[&](std::int32_t& v){signed_integer(v,at);at+=4;};
        const auto ui=[&](std::uint32_t& v){integer(v,at);at+=4;};const auto fp=[&](float& v){real(v,at);at+=4;};
        auto& v=viewport;si(v.width);si(v.height);si(v.center_pixel_x);si(v.center_pixel_y);
        fp(v.center_x);fp(v.center_y);fp(v.scale_x);fp(v.scale_y);fp(v.right_edge);fp(v.bottom_edge);
        si(v.min_x);si(v.min_y);si(v.max_x);si(v.max_y);si(v.depth_bits);si(v.bytes_per_pixel);si(v.full_width);si(v.full_height);
        ui(v.cropped);ui(v.selector);fp(v.origin_x);fp(v.origin_y);
        for(auto& x:v.clip_virtual)fp(x);for(auto& x:v.clip_screen)fp(x);for(auto& x:v.clip_expanded)fp(x);
        ui(v.applied_selector);ui(v.requested_selector);fp(v.ui_scale_x);fp(v.ui_scale_y);
        wire::require(at==wire::materials,"viewport wire field count");
        for(unsigned i=0;i<255;++i){integer(materials[i].primary_texture,wire::materials+i*28);integer(materials[i].alternate_texture,wire::materials+i*28+4);}
        for(unsigned i=0;i<35;++i)integer(quad[i],wire::quad+i*4);
        for(unsigned i=0;i<8;++i)for(unsigned j=0;j<10;++j)integer(alpha[i].words[j],wire::alpha+i*40+j*4);
        for(unsigned i=0;i<9;++i)integer(clip.words[i],wire::clip+4*i);
        for(unsigned i=0;i<2;++i)integer(labels[i],wire::labels+4*i);
    }
    void patch(std::uint32_t at,std::uint32_t value){transfer(false);wire::put(carrier,at,value);transfer(true);}
};
}
