#pragma once
#include "sgn/polygon_device_context.hpp"
#include <bit>

namespace session_archive {
using namespace sgn;
struct Frame {PolygonDeviceState initial;CapturedPolygonFrame commands;};
struct Writer {
    Bytes bytes;
    void u32(std::uint32_t value){for(unsigned shift=0;shift<32;shift+=8)bytes.push_back(static_cast<std::uint8_t>(value>>shift));}
    void u64(std::uint64_t value){u32(static_cast<std::uint32_t>(value));u32(static_cast<std::uint32_t>(value>>32));}
    void word(std::uint32_t& value){u32(value);}
    void word(std::int32_t& value){u32(std::bit_cast<std::uint32_t>(value));}
    void word(float& value){u32(std::bit_cast<std::uint32_t>(value));}
};
struct Reader {
    std::span<const std::uint8_t> bytes;std::size_t at{};
    std::uint32_t u32(){if(at>bytes.size()||bytes.size()-at<4)throw FormatError("short Session frame archive");std::uint32_t value{};for(unsigned shift=0;shift<32;shift+=8)value|=std::uint32_t(bytes[at++])<<shift;return value;}
    std::uint64_t u64(){const auto lo=u32();return lo|(std::uint64_t(u32())<<32);}
    void word(std::uint32_t& value){value=u32();}
    void word(std::int32_t& value){value=std::bit_cast<std::int32_t>(u32());}
    void word(float& value){value=std::bit_cast<float>(u32());}
    std::size_t count(std::size_t stride,std::size_t maximum){const auto n=u32();if(n>maximum||at>bytes.size()||std::size_t(n)>(bytes.size()-at)/stride)throw FormatError("Session archive count outside file/budget");return n;}
};
template<class Codec>void viewport(Codec& c,sgn::retail::ViewportState& v){
    c.word(v.width);c.word(v.height);c.word(v.center_pixel_x);c.word(v.center_pixel_y);
    c.word(v.center_x);c.word(v.center_y);c.word(v.scale_x);c.word(v.scale_y);c.word(v.right_edge);c.word(v.bottom_edge);
    c.word(v.min_x);c.word(v.min_y);c.word(v.max_x);c.word(v.max_y);c.word(v.depth_bits);c.word(v.bytes_per_pixel);
    c.word(v.full_width);c.word(v.full_height);c.word(v.cropped);c.word(v.selector);c.word(v.origin_x);c.word(v.origin_y);
    for(auto& x:v.clip_virtual)c.word(x);for(auto& x:v.clip_screen)c.word(x);for(auto& x:v.clip_expanded)c.word(x);
    c.word(v.applied_selector);c.word(v.requested_selector);c.word(v.ui_scale_x);c.word(v.ui_scale_y);
}
inline Bytes encode(const CapturedPolygonFrame& frame,const PolygonDeviceState& initial){
    Writer w;w.u32(0x534e4753);w.u32(0x32304657); //SGNSWF02, canonical little endian
    w.u64(frame.source_tick);w.u64(frame.resource_epoch);w.u64(frame.payload_bytes);
    const auto option=[&](const std::optional<std::uint32_t>& v){w.u32(v?1u:0u);w.u32(v.value_or(0));};
    for(const auto& v:initial.render)option(v);for(const auto& stage:initial.stage)for(const auto& v:stage)option(v);for(const auto& v:initial.texture)option(v);
    w.u32(static_cast<std::uint32_t>(frame.commands.size()));
    for(const auto& command:frame.commands){w.u32(static_cast<std::uint32_t>(command.index()));
        std::visit([&](const auto& item){using T=std::decay_t<decltype(item)>;
            if constexpr(std::is_same_v<T,CapturedRenderState>){w.u32(item.state);w.u32(item.value);}
            else if constexpr(std::is_same_v<T,CapturedTexture>){w.u32(item.stage);w.u32(item.token);}
            else if constexpr(std::is_same_v<T,CapturedTextureStageState>){w.u32(item.stage);w.u32(item.state);w.u32(item.value);}
            else {w.u32(item.type);w.u32(item.fvf);w.u32(item.flags);w.u32(item.indexed?1u:0u);auto v=item.viewport;viewport(w,v);
                w.u32(static_cast<std::uint32_t>(item.vertices.size()));for(const auto& vertex:item.vertices)for(auto word:vertex.words)w.u32(word);
                w.u32(static_cast<std::uint32_t>(item.indices.size()));for(auto index:item.indices)w.u32(index);}
        },command);
    }return std::move(w.bytes);
}
inline Frame decode(std::span<const std::uint8_t> bytes){
    if(bytes.size()>64u*1024u*1024u)throw FormatError("Session archive exceeds file budget");Reader r{bytes};
    if(r.u32()!=0x534e4753||r.u32()!=0x32304657)throw FormatError("Session archive signature");
    Frame result;auto& frame=result.commands;frame.source_tick=r.u64();frame.resource_epoch=r.u64();const auto payload=r.u64();
    if(payload>32u*1024u*1024u)throw FormatError("Session archive payload budget");frame.payload_bytes=static_cast<std::size_t>(payload);
    const auto option=[&](std::optional<std::uint32_t>& v){const auto present=r.u32(),value=r.u32();if(present>1||(!present&&value))throw FormatError("Session archive noncanonical optional");if(present)v=value;};
    for(auto& v:result.initial.render)option(v);for(auto& stage:result.initial.stage)for(auto& v:stage)option(v);for(auto& v:result.initial.texture)option(v);
    const auto commands=r.count(4,20000);std::size_t observed_payload{};frame.commands.reserve(commands);
    for(std::size_t i=0;i<commands;++i){switch(r.u32()){
        case 0:{const auto state=r.u32(),value=r.u32();frame.commands.emplace_back(CapturedRenderState{state,value});break;}
        case 1:{const auto stage=r.u32(),token=r.u32();frame.commands.emplace_back(CapturedTexture{stage,token});break;}
        case 2:{const auto stage=r.u32(),state=r.u32(),value=r.u32();frame.commands.emplace_back(CapturedTextureStageState{stage,state,value});break;}
        case 3:{CapturedPolygonDraw draw;draw.type=r.u32();draw.fvf=r.u32();draw.flags=r.u32();const auto indexed=r.u32();if(indexed>1)throw FormatError("Session archive indexed flag");draw.indexed=indexed!=0;viewport(r,draw.viewport);
            const auto vertices=r.count(40,32u*1024u*1024u/40);if(observed_payload>payload||vertices>(payload-observed_payload)/40)throw FormatError("Session archive vertex payload budget");draw.vertices.resize(vertices);for(auto& vertex:draw.vertices)for(auto& word:vertex.words)word=r.u32();
            const auto indices=r.count(4,16u*1024u*1024u);if(indices>(payload-observed_payload-vertices*40)/2)throw FormatError("Session archive index payload budget");draw.indices.reserve(indices);for(std::size_t n=0;n<indices;++n){const auto index=r.u32();if(index>65535u||index>=vertices)throw FormatError("Session archive vertex index");draw.indices.push_back(static_cast<std::uint16_t>(index));}
            observed_payload+=vertices*40+indices*2;if(observed_payload>payload)throw FormatError("Session archive payload count mismatch");frame.commands.emplace_back(std::move(draw));break;}
        default:throw FormatError("Session archive unknown command");
    }}
    if(observed_payload!=payload||r.at!=bytes.size())throw FormatError("Session archive trailing bytes/payload mismatch");return result;
}
inline std::uint64_t hash(const CapturedPolygonFrame& frame,const PolygonDeviceState& initial){
    std::uint64_t h=1469598103934665603ull;for(auto byte:encode(frame,initial)){h^=byte;h*=1099511628211ull;}return h;
}
}
