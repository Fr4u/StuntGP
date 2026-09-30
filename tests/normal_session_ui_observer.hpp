#pragma once
#include "normal_session_native_observer.hpp"
#include "sgn/session_world_materials.hpp"

namespace normal_native_observer {
// No guest-address helper, model mapper or encoded image is used here.
inline void capture_ui_screen(const RaceSessionState& s,Observations& out){
    out.span("screen-records",std::span(s.screen_polygons));out.ref("screen-count",s.screen_polygon_count);
    const auto is_null=s.screen_base.is_null();
    out.value("screen-base-null",std::uint32_t(is_null));
    out.value("screen-base-native-address",is_null?std::uint64_t{0}:reinterpret_cast<std::uint64_t>(s.screen_polygons.data()));
}
inline void capture_ui_scalars(RaceSession& session,const SessionWorldMaterials& materials,const std::filesystem::path& output){
    const auto& s=session.state();const auto& ui=s.ui;const auto& m=materials.validate_for(session);Observations out;
    out.span("descriptor",std::span(ui.descriptor));out.ref("material-cache",ui.selected_material);
    out.ref("loading-tile-base",ui.loading_tile_base);out.ref("loading-fade",ui.loading_fade);
    out.ref("second-pause",ui.second_pause);out.ref("frame-pause",s.frame_pause);out.ref("hud-block",s.hud_block);
    out.span("star-phases",std::span(ui.stars.phase));capture_ui_screen(s,out);
    std::array<std::uint32_t,255*7> words;
    for(unsigned slot=0;slot<255;++slot)for(unsigned i=0;i<7;++i)words[slot*7+i]=m.names.materials.source_word(slot,i*4);
    out.values("physical-material-words",std::span(words));out.value("physical-material-active",std::uint32_t(m.names.materials.active()));out.value("physical-material-count",m.names.materials.size());
    // Observe every actual font word, including its raw scratch identity. The
    // formatter's missing scratch accessor remains an explicit separate gap.
    const auto& fonts=materials.validate_fonts_for(session);std::array<std::uint32_t,2*retail_font_record_words> font_words;
    for(unsigned f=0;f<2;++f)for(unsigned i=0;i<retail_font_record_words;++i)font_words[f*retail_font_record_words+i]=fonts.source_word(f,i*4);
    out.values("font-words",std::span(font_words));out.value("font-scratch-count",static_cast<std::uint64_t>(fonts.scratch_allocations()));
    out.span("font-index",std::span(fonts.index));out.span("glyph-map",std::span(fonts.glyph_map));out.ref("font-current",fonts.current);out.ref("font-count",fonts.count);out.ref("font-digit-advance",fonts.digit_advance_bits);
    out.save(output);
}
}
