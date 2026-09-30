#pragma once
// Private UI scalar/material/screen portion of the full normal-parent codec.
// Fonts, submission scratch and controller HUD/vehicle/clock fields are
// separate required portions; this file never fills those gaps with zeros.
#include "normal_session_frame_export.hpp"
#include "sgn/session_world_materials.hpp"

namespace normal_session_owner {
inline constexpr std::uint32_t normal_screen_base=0x72140000;
inline void export_ui_screen_owners(RaceSessionState& s,Image& out,NativeAddresses& addresses){
    addresses.array("ui-screen-records",std::span(s.screen_polygons),normal_screen_base);
    out.create_live("ui-screen-records",normal_screen_base,std::span(s.screen_polygons));
    frame_pointer(out,"ui-screen-root",0x61c5f0,s.screen_base.is_null()?0u:normal_screen_base);
    frame_scalar(out,"ui-screen-count",0x61c604,s.screen_polygon_count);
}
inline void export_ui_scalar_owners(RaceSession& session,const SessionWorldMaterials& materials,Image& out,NativeAddresses& addresses){
    auto& s=session.state();auto& ui=s.ui;const auto& material_state=materials.validate_for(session);
    static_assert(sizeof(ui.descriptor)==35*4&&sizeof(SourceScreenPolygon)==272);
    out.create_live("ui-descriptor",0x576c38,std::span(ui.descriptor));
    addresses.array("ui-descriptor",std::span(ui.descriptor),0x576c38);
    frame_scalar(out,"ui-material-cache",0x61c5e4,ui.selected_material);
    frame_scalar(out,"ui-loading-tile-base",0x61b628,ui.loading_tile_base);
    frame_scalar(out,"ui-loading-fade",0x61b630,ui.loading_fade);
    frame_scalar(out,"ui-second-pause",0x612958,ui.second_pause);
    frame_scalar(out,"ui-frame-pause",0x61294c,s.frame_pause);
    frame_scalar(out,"ui-hud-block",0x61299c,s.hud_block);
    // Only the actual phase word is represented here. The adjacent two words
    // of each original star record are not fabricated from this scalar view.
    for(unsigned i=0;i<ui.stars.phase.size();++i)frame_scalar(out,"ui-star-phase-"+std::to_string(i),0x485258+i*12,ui.stars.phase[i]);
    export_ui_screen_owners(s,out,addresses); //raw count is independent of draw admission
    // Physical texture IDs are external-interface identity tokens. The source
    // executor must translate them at its real API boundary, as in the bounded
    // status oracle. They are not guest pointers to fabricated texture objects.
    std::array<std::uint32_t,255*7> words;
    for(unsigned slot=0;slot<255;++slot)for(unsigned i=0;i<7;++i)words[slot*7+i]=material_state.names.materials.source_word(slot,i*4);
    const auto owner=out.create_live("ui-physical-material-words",0x6227d4,std::span(words));
    const auto cursor=material_state.names.materials.source_cursor();
    if(cursor){
        require(cursor>=0x6227d4&&(cursor-0x6227d4)%28==0&&cursor-0x6227d4<=sizeof(words),"physical material cursor outside actual capacity");
        // At capacity255 the one-past address is also the cursor global's
        // address6243B8. Keep its allocation identity explicit.
        out.alias_in("ui-physical-material-cursor-target",owner,cursor-0x6227d4,0);
    }
    EncodedWords current;current.append(cursor,Origin::relocation,"ui-physical-material-cursor");
    (void)std::move(current).save(out,"ui-physical-material-cursor",0x6243b8);
}
}
