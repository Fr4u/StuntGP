#include "sgn/source_menu_command_consumption.hpp"
#include "sgn/race_session.hpp"
#include "sgn/race_session_lifecycle.hpp"
#include "sgn/retail_frontend_initialization.hpp"
#include "sgn/retail_font_selection.hpp"
#include "sgn/retail_quad_submission.hpp"
#include "sgn/host_wide_menu.hpp"
#include "sgn/host_wide_race_anchors.hpp"
#include "sgn/host_graphics_page.hpp"
#include "sgn/bit_reinterpret.hpp"
#include <bit>

namespace sgn::retail
{
namespace
{
using sgn::bit_reinterpret::as_float;
using sgn::bit_reinterpret::as_signed;
std::uint32_t store(double v)
{
    return std::bit_cast<std::uint32_t>(vehicle_f32(v));
}
std::span<std::uint32_t> payload(FrontendSourceStorage &s, std::uint32_t id, std::size_t words)
{
    if (id < 0x5da374u || (id & 3u) || id > 0x5df194u || words > (0x5df194u - id) / 4)
        throw FormatError("menu consumer pointer outside existing command payload");
    return std::span{s.menu_payload_5da374}.subspan((id - 0x5da374u) / 4, words);
}
void require_frame(FrontendState &f, const LoadingFrameWorld &frame)
{
    auto &w = f.world();
    if (frame.quads.descriptor.data() != w.ui.descriptor.data() ||
        &frame.quads.selected_material != &w.ui.selected_material || &frame.quads.viewport != &w.viewport ||
        &frame.render_counter != &w.outer.render_counter || &frame.scene_active != &w.outer.device.scene_active ||
        &frame.finalized != &w.outer.device.finalized)
        throw FormatError("menu submission does not borrow the frontend world");
}
TextSubmission text_view(SourceMenuSubmission &call)
{
    auto frame = call.frames.borrow_loading_frame();
    require_frame(call.frontend, frame);
    auto &w = call.frontend.world();
    return {w.ui.fonts,
            w.ui.selected_material,
            frame.submission.specular_enabled,
            frame.quads.materials,
            w.view_quality.second,
            w.view_quality.legacy_driver_restriction,
            w.viewport};
}
std::int32_t material_441270(SourceMenuSubmission &call, std::uint32_t source)
{
    auto &s = call.frontend.source;
    if (source >= s.lookup_b.size())
        throw FormatError("frontend source texture outside lookup owner");
    auto slot = s.lookup_b[source];
    if (as_signed(slot) >= 0)
    {
        if (slot >= s.lookup_c.size())
            throw FormatError("frontend resident texture slot outside lookup owner");
        s.lookup_c[slot] = 0xfffffffeu;
        return as_signed(slot);
    }
    // 4412C0: strict greater means the first maximum wins; only slots17..36.
    std::int32_t maximum = 0;
    slot = 0xffffffffu;
    for (std::uint32_t i = 17; i < 37; ++i)
    {
        const auto age = as_signed(s.lookup_c[i]);
        if (age > maximum)
        {
            maximum = age;
            slot = i;
        }
    }
    if (as_signed(slot) < 0)
        return -1;
    const auto previous = s.lookup_a[slot];
    if (as_signed(previous) >= 0)
    {
        if (previous >= s.lookup_b.size())
            throw FormatError("frontend evicted texture outside lookup owner");
        s.lookup_b[previous] = 0xffffffffu;
    }
    call.textures.replace_source_texture(slot, source); // 441310 -> existing430D40
    ++s.menu_draw_counts_5df518[1];                     // only after source replacement returns
    s.lookup_b[source] = slot;
    s.lookup_a[slot] = source;
    s.lookup_c[slot] = 0xfffffffeu;
    return as_signed(slot);
}
} // namespace
std::int32_t resolve_frontend_material_441240(SourceMenuSubmission &call, std::span<std::uint32_t, 12> d)
{
    if (d[9] & 0x10000u)
        d[9] = std::bit_cast<std::uint32_t>(material_441270(call, d[9] & 0xffffu));
    return as_signed(d[9]);
}
void consume_frontend_command_437f40(SourceMenuSubmission &call, std::uint32_t command, std::uint32_t list)
{
    auto &s = call.frontend.source;
    const auto depth = store(1.0 - double(as_signed(list)) * double(as_float(0x3c23d70au)));
    const auto tag = payload(s, command, 2)[1];
    if (tag == 1)
    {
        auto d = payload(s, command + 8, 12).first<12>();
        if (resolve_frontend_material_441240(call, d) < 0)
            return;
        auto &q = call.frontend.world().ui.descriptor;
        q[1] = d[1];
        q[0] = d[0];
        q[2] = d[2];
        q[4] = d[3];
        const auto material = d[9];
        q[3] = depth;
        q[5] = d[4];
        q[7] = d[6];
        q[6] = d[5];
        q[8] = d[7];
        q[11] = d[10];
        q[9] = d[8];
        q[10] = material;
        q[12] = d[11];
        auto frame = call.frames.borrow_loading_frame();
        require_frame(call.frontend, frame);
        auto &r = frame.quads;
        host_wide_menu::ConsumeScope wide(command); // NEW R20 (wide_menu): tags the draw 432E00 produces
        submit_quad_432e00({r.descriptor, r.vertices, r.selected_material, r.materials, r.viewport}, call.sink);
        if (s.menu_last_material_4793d4 != material)
            ++s.menu_draw_counts_5df518[0];
        s.menu_last_material_4793d4 = material;
        ++s.menu_draw_counts_5df518[3];
        return;
    }
    if (tag != 0)
        return; // source default: ignored command
    auto p = payload(s, command, 14);
    auto &fonts = call.frontend.world().ui.fonts;
    fonts.digit_advance_bits = p[8]; // 4291A0, before438130 font selection
    const auto scale = p[5];
    std::uint32_t flags = p[4] == 2 ? 2u : p[4] == 3 ? 4u : p[4] == 4 ? 6u : 0u;
    select_retail_font_428f70(fonts, as_float(scale) > 1.0f ? 1u : 0u);
    const auto input_flags = p[7];
    const std::uint32_t alignment = (input_flags & 1u) ? 1u : (input_flags & 4u) ? 2u : 0u;
    if (input_flags & 2u)
        p[3] = store(double(as_float(p[3])) - double(as_float(scale)) * 16.0 * 0.5);
    else if (input_flags & 8u)
        p[3] = store(double(as_float(p[3])) - double(as_float(scale)) * 16.0);
    // Source inline text at+38; pass the rest of the existing payload without
    // an eager NUL scan, so4291F0 keeps its reached-byte traversal and clipping.
    const auto text_id = command + 56u;
    (void)payload(s, text_id, 0);
    const auto text = s.source_bytes(text_id, 0x5df194u - text_id);
    if (p[7] & 0x800u)
    {
        submit_text_4291f0(
            text_view(call), text, alignment,
            {as_float(p[2]), as_float(p[3]), as_float(depth), 0xffffffffu, as_float(scale), flags | 0x10u}, call.sink);
        flags = (flags & ~4u) | 8u;
        // Source reloads scale/color/x/y after the first completed child.
        submit_text_4291f0(text_view(call), text, alignment,
                           {as_float(p[2]), as_float(p[3]), as_float(depth), p[6], as_float(p[5]), flags}, call.sink);
    }
    else
    {
        if (p[7] & 0x100u)
            flags |= 8u;
        else if (p[7] & 0x200u)
            flags |= 0x10u;
        submit_text_4291f0(text_view(call), text, alignment,
                           {as_float(p[2]), as_float(p[3]), as_float(depth), p[6], as_float(scale), flags}, call.sink);
    }
    if (s.menu_last_material_4793d4 != 999u)
        ++s.menu_draw_counts_5df518[0];
    s.menu_last_material_4793d4 = 999u;
    ++s.menu_draw_counts_5df518[2];
}
void consume_frontend_lists_437ed0(SourceMenuSubmission &call)
{
    auto &s = call.frontend.source;
    s.menu_draw_counts_5df518[0] = 0;
    call.frontend.world().ui.descriptor.fill(0);
    s.menu_draw_counts_5df518[1] = 0;
    s.menu_draw_counts_5df518[2] = 0;
    s.menu_draw_counts_5df518[3] = 0;
    s.menu_last_material_4793d4 = 0xffffffffu;
    for (int list = 79; list >= 0; --list)
    {
        auto command = s.menu_words_5df194[80 + static_cast<unsigned>(list)];
        while (command)
        {
            consume_frontend_command_437f40(call, command, static_cast<std::uint32_t>(list));
            command = payload(s, command, 1)[0]; // reread next AFTER the child
        }
    }
}
void consume_frontend_commands_437ec0(SourceMenuSubmission &call)
{
    call.frontend.shared_platform()->require_owner_thread();
    {
        // NEW HOST (widescreen R37): the menu stays one centred 4:3 composition (same scale and
        // centre), but the 432E00/4291F0 viewport clip reaches the real screen edges, so rows, icons
        // and texts sliding in or out are no longer cut at the 4:3 edges. Identity on 4:3.
        host_wide_overlay::AnchorScope wide_clip(host_wide_overlay::race::panel);
        consume_frontend_lists_437ed0(call);
        draw_host_graphics_page_arrows_r29(call.frontend, call.sink, text_view(call)); // NEW HOST (R29), after all lists
    }
    host_wide_menu::consumed_all(); // NEW R20 (wide_menu) host bookkeeping, before 437E80
    clear_frontend_draw_lists_437e80(call.frontend.source);
    for (unsigned i = 17; i < 37; ++i)
        ++call.frontend.source.lookup_c[i]; // 441220, wrap
}
SourceMenuConsumptionChildren::SourceMenuConsumptionChildren(
    MenuKeyboardAcquisition &keys, Win32MenuDirectInputStartup &input, CampaignScoreMusic &music,
    SessionOuterFrameServices &frames, PolygonSubmissionSink &sink, RaceTextureReplacement &textures)
    : SourceMenuCommandQueueChildren(keys, input, music), frames_(frames), sink_(sink), textures_(&textures)
{
}
void SourceMenuConsumptionChildren::submit_lists_437ec0(FrontendState &frontend)
{
    SourceMenuSubmission call{frontend, frames_, sink_, *textures_};
    consume_frontend_commands_437ec0(call);
}
} // namespace sgn::retail
