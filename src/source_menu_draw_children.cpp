#include "sgn/source_menu_draw_children.hpp"
#include "sgn/race_session.hpp"
#include <bit>
#include "sgn/source_script_path_formatter.hpp"
#include "sgn/source_menu_fill.hpp"
#include "sgn/host_wide_menu.hpp"
#include "sgn/bit_reinterpret.hpp"
namespace sgn::retail
{
namespace
{
using sgn::bit_reinterpret::as_float;
std::int32_t signed_word(std::uint32_t v)
{
    return std::bit_cast<std::int32_t>(v);
}
std::uint32_t store(double v)
{
    return std::bit_cast<std::uint32_t>(vehicle_f32(v));
}
using Descriptor = std::array<std::uint32_t, 12>;
Descriptor descriptor(FrontendState &f, std::uint32_t id)
{
    if (id >= 387)
        throw FormatError("frontend frame descriptor outside source bank");
    Descriptor d;
    for (unsigned i = 0; i < 12; ++i)
        d[i] = f.source.frontend_descriptors[id * 12 + i];
    return d;
}
void edge_439520(FrontendState &f, std::uint32_t layer, std::uint32_t argument, SourceMenuRect r, std::uint32_t id,
                 std::uint32_t row, std::uint32_t blend)
{
    // Pinned read-only geometry words4793DC/E0/E4:12,30,62.
    r[0] = store(double(as_float(r[0])) - 12.0);
    r[1] = store(double(as_float(r[1])) - 12.0);
    r[2] = store((double(as_float(r[2])) - 30.0) + 12.0);
    r[3] = store((double(as_float(r[3])) - 30.0) + 12.0);
    auto d = descriptor(f, id);
    const auto u = d[5];
    const auto v = store(double(row << 6) * 4.0 + double(as_float(d[6]))); // FILD64 with zero highDWORD
    d[0] = 2;
    d[10] = blend;
    d[11] = 255;
    d[3] = d[4] = store(30.0);
    const auto U = [&](double offset) { return store(offset + double(as_float(u))); };
    const auto V = [&](double offset) { return store(offset + double(as_float(v))); };
    const auto emit = [&] { clip_frontend_rectangle_438370(f.source, d, layer, argument); };
    d[1] = r[0];
    d[2] = r[1];
    d[6] = v;
    d[7] = U(120);
    d[8] = V(120);
    emit(); // top left
    d[1] = r[2];
    d[2] = r[1];
    d[5] = U(128);
    d[6] = v;
    d[7] = U(248);
    d[8] = V(120);
    emit();
    d[1] = r[0];
    d[2] = r[3];
    d[5] = u;
    d[6] = V(128);
    d[7] = U(120);
    d[8] = V(248);
    emit();
    d[1] = r[2];
    d[2] = r[3];
    d[5] = U(128);
    d[6] = V(128);
    d[7] = U(248);
    d[8] = V(248);
    emit();
    d[1] = store(30.0 + double(as_float(r[0])));
    d[2] = r[1];
    d[5] = U(120);
    d[6] = v;
    d[7] = U(128);
    d[8] = V(120);
    d[3] = store((double(as_float(r[2])) - double(as_float(r[0]))) - 30.0);
    emit();
    d[1] = store(30.0 + double(as_float(r[0])));
    d[2] = r[3];
    d[5] = U(120);
    d[6] = V(128);
    d[7] = U(128);
    d[8] = V(248);
    emit();
    d[1] = r[0];
    d[3] = store(30);
    d[4] = store((double(as_float(r[3])) - double(as_float(r[1]))) - 30.0);
    d[2] = store(30.0 + double(as_float(r[1])));
    d[5] = u;
    d[6] = V(120);
    d[7] = U(120);
    d[8] = V(128);
    emit();
    d[1] = r[2];
    d[2] = store(30.0 + double(as_float(r[1])));
    d[5] = U(128);
    d[6] = V(120);
    d[7] = U(248);
    d[8] = V(128);
    emit();
}
void bar(FrontendState &f, bool vertical, std::uint32_t layer, std::uint32_t fixed, std::uint32_t start,
         std::uint32_t end, std::array<std::uint32_t, 3> values)
{
    // 4399B0 horizontal /439B80 vertical. Three base pieces, then three thumb pieces.
    auto d = descriptor(f, 117);
    const unsigned pos = vertical ? 2u : 1u, extent = vertical ? 4u : 3u, uv0 = vertical ? 6u : 5u,
                   uv1 = vertical ? 8u : 7u;
    auto base = d[uv0];
    const auto base_u = d[5];
    d[0] = 2;
    d[11] = 255;
    d[10] = 0x70;
    d[pos] = start;
    d[vertical ? 1u : 2u] = fixed;
    d[extent] = store(5);
    const auto U = [&](double delta) { return store(delta + double(as_float(base))); };
    const auto emit = [&](std::uint32_t list) { clip_frontend_rectangle_438370(f.source, d, layer, list); };
    d[uv1] = U(20);
    emit(1);
    d[pos] = store(double(as_float(d[extent])) + double(as_float(d[pos])));
    d[uv0] = d[uv1];
    d[uv1] = U(32);
    const auto length = store(double(as_float(end)) - double(as_float(start)));
    d[extent] = store(double(as_float(length)) - 10.0);
    emit(1);
    d[pos] = store(double(as_float(d[extent])) + double(as_float(d[pos])));
    d[uv0] = d[uv1];
    d[extent] = store(5);
    d[uv1] = U(52);
    emit(1);
    if (vertical)
    {
        const double u = 120.0 + double(as_float(base_u));
        d[5] = store(u);
        d[7] = store(52.0 + u);
    }
    else
        base = store(60.0 + double(as_float(base)));
    d[vertical ? 1u : 2u] = fixed;
    d[uv0] = base;
    volatile double offset = double(as_float(length)) * double(as_float(values[2]));
    d[pos] = store(offset / double(as_float(values[0])) + double(as_float(start)));
    d[uv1] = U(20);
    d[extent] = store(5);
    d[10] = 0x58;
    emit(0);
    d[pos] = store(double(as_float(d[extent])) + double(as_float(d[pos])));
    d[uv0] = d[uv1];
    d[uv1] = U(32);
    volatile double size = double(as_float(length)) * double(as_float(values[1]));
    d[extent] = store(size / double(as_float(values[0])) - 10.0);
    emit(0);
    d[pos] = store(double(as_float(d[extent])) + double(as_float(d[pos])));
    d[uv0] = d[uv1];
    d[extent] = store(5);
    d[uv1] = U(52);
    emit(0);
}
std::span<const std::uint8_t> language(FrontendState &f, std::uint32_t id)
{
    const auto value = frontend_language_text_441170(f.source.language_strings(), id);
    return resolve_frontend_language_text(f.source.language_strings(), value);
}
} // namespace
void draw_frontend_frame_439450(FrontendState &f, std::uint32_t layer, std::uint32_t id, SourceMenuRect r,
                                std::uint32_t flags)
{
    f.shared_platform()->require_owner_thread();
    // Source C0 branches include unordered, unlike an ordinary strict less-than test.
    if (as_float(r[0]) >= as_float(r[2]) || as_float(r[1]) >= as_float(r[3]))
        return;
    if (flags & 0x40u)
    {
        auto shifted = r;
        for (auto &v : shifted)
            v = store(double(as_float(v)) + 8.0);
        edge_439520(f, 15, 0, shifted, id, 2, 0x70);
    }
    edge_439520(f, layer, 3, r, id, 1, 0x70);
    edge_439520(f, layer, 2, r, id, 0, 0x58);
}
void draw_frontend_panel_4398a0(FrontendState &f, std::uint32_t layer, std::uint32_t id, SourceMenuRect r,
                                std::uint32_t flags, std::array<std::uint32_t, 6> values)
{
    f.shared_platform()->require_owner_thread();
    draw_frontend_frame_439450(f, layer, id, r, flags);
    if (as_float(values[0]) > 0 && as_float(values[1]) > 0 && !(as_float(values[2]) > as_float(values[0])))
        bar(f, false, layer, store((double(as_float(r[3])) + 10.0) - 13.0), store(double(as_float(r[0])) + 10.0),
            store(double(as_float(r[2])) - 10.0), {values[0], values[1], values[2]});
    if (as_float(values[3]) > 0 && as_float(values[4]) > 0 && !(as_float(values[5]) > as_float(values[3])))
        bar(f, true, layer, store((double(as_float(r[2])) + 10.0) - 13.0), store(double(as_float(r[1])) + 10.0),
            store(double(as_float(r[3])) - 10.0), {values[3], values[4], values[5]});
}
void draw_frontend_unavailable_43ff90(FrontendState &f, std::uint32_t player)
{
    f.shared_platform()->require_owner_thread();
    {
        host_wide_menu::QueueScope wide(host_wide_menu::Kind::tiled); // NEW R20 (wide_menu) host tag only
        draw_frontend_tiled_438830(f, 19, 95, {0, 0, 0x44200000, 0x43f00000},
                                   {f.source.source_word(0x5ff454), f.source.source_word(0x5ff458)});
    }
    std::array<std::uint8_t, 256> text; // original local0x100-byte buffer, not a retained language owner
    const auto format = language(f, player ? 606u : 605u);
    if (format.empty())
        throw FormatError("unavailable message dereferences null language text");
    std::size_t written = 0;
    const auto append = [&](std::uint8_t c) {
        if (written >= text.size())
            throw FormatError("unavailable message exceeds source256-byte stack");
        text[written++] = c;
    };
    if (player)
    {
        SourceScriptPathCall call;
        auto platform = f.shared_platform();
        (void)format_unavailable_message_43ffdd(platform->state().boot_config, platform->source_application_literals(),
                                                format, player, text, call);
        written = call.initialized_prefix;
    }
    else
        for (auto c : format)
        {
            append(c);
            if (c == 0)
                break;
        }
    constexpr SourceMenuRect r{0x42200000u, 0x435c0000u, 0x44160000u, 0x43820000u};
    const auto frame_id = (f.source.source_word(0x5ff45c) & 0x20u) ? 111u : 112u;
    draw_frontend_frame_439450(f, 3, frame_id, r, 0x40);
    draw_frontend_fill_439380(f, 6, 2, r, 0, 0);
    queue_frontend_text_438990(f.source, 5, 0, 0x43a00000u, 0x43700000u, 4, std::span{text}.first(written), 0x3fc00000u,
                               0xffffffffu, 0x803, 0);
}
void SourceMenuPageDrawingChildren::unavailable_43ff90(FrontendState &f, std::uint32_t p)
{
    draw_frontend_unavailable_43ff90(f, p);
}
void SourceMenuPageDrawingChildren::frame_439450(FrontendState &f, std::uint32_t l, std::uint32_t d, SourceMenuRect r,
                                                 std::uint32_t flags)
{
    draw_frontend_frame_439450(f, l, d, r, flags);
}
void SourceMenuPageDrawingChildren::panel_4398a0(FrontendState &f, std::uint32_t l, std::uint32_t d, SourceMenuRect r,
                                                 std::uint32_t flags, std::array<std::uint32_t, 6> v)
{
    draw_frontend_panel_4398a0(f, l, d, r, flags, v);
}
} // namespace sgn::retail
