#include "sgn/source_title_page.hpp"
#include "sgn/race_session.hpp"
#include "sgn/source_frontend_packed_word.hpp"
#include "sgn/retail_frontend_clip.hpp"
#include "sgn/host_wide_menu.hpp"
#include "sgn/bit_reinterpret.hpp"
#include "sgn/little_endian_words.hpp"
#include <bit>
#include <limits>

namespace sgn::retail
{
namespace
{
std::span<std::byte> page_bytes(FrontendState &f, std::uint32_t page, std::size_t size = 36)
{
    return frontend_page_object_bytes(std::as_writable_bytes(std::span{f.source.page_pool}), page, size);
}
using sgn::bit_reinterpret::as_float;
using sgn::bit_reinterpret::as_word;
using sgn::little_endian::read_le32;
using sgn::little_endian::write_le32;
void tiles(FrontendState &f, SourceTitlePageServices &calls, float x, float y, bool first_pass)
{
    // 45C320 /45C3D0, pinned read-only489290/94 =201/172; each call is synchronous.
    const auto mode = first_pass ? 13u : 12u, first = first_pass ? 33u : 29u, flags = first_pass ? 512u : 256u;
    calls.sprite_438710(f, mode, first, {as_word(x), as_word(y), 0x3f800000, flags});
    calls.sprite_438710(f, mode, first + 1, {as_word(vehicle_f32(201.0 + double(x))), as_word(y), 0x3f800000, flags});
    calls.sprite_438710(f, mode, first + 2, {as_word(x), as_word(vehicle_f32(172.0 + double(y))), 0x3f800000, flags});
    calls.sprite_438710(
        f, mode, first + 3,
        {as_word(vehicle_f32(201.0 + double(x))), as_word(vehicle_f32(172.0 + double(y))), 0x3f800000, flags});
}
} // namespace
std::uint32_t push_frontend_page_43dfa0(FrontendState &f, std::uint32_t key, std::uint32_t kind,
                                        SourcePageFactoryServices &factory, SourceNavigationPageServices &pages,
                                        CampaignScoreMusic &music)
{
    const auto signed_kind = std::bit_cast<std::int32_t>(kind);
    if (signed_kind < 1 || signed_kind > 40)
        return 0;
    auto word = [&](std::uint32_t address) -> std::uint32_t & { return f.source.source_word(address); };
    auto retained = word(0x5ff3f8);
    if (retained != 0)
        word(0x5ff3fc) = 1;
    else
    {
        retained = word(0x5ff3f0);
        if (retained != 0)
            word(0x5ff3f4) = 1;
    }
    if (retained != 0)
    {
        const auto count = word(0x5ff440);
        if (count >= 16)
            throw FormatError("title page push exceeds existing source stack");
        word(0x5ff400u + count * 4u) = retained;
        ++word(0x5ff440);
    }
    const auto page = factory.create_page_43e000(f, key, kind);
    return publish_frontend_page_43ee40(f, page, pages, music);
}
void reset_frontend_title_page_45c050(FrontendState &f, std::uint32_t page)
{
    const auto p = page_bytes(f, page);
    write_le32(p, 0x18, 0);
    write_le32(p, 0x1c, 0x3f800000);
    write_le32(p, 0x20, 0x3f800000);
    write_le32(p, 0x14, 0);
    f.source.source_word(0x5e37a4) = 0;
    f.source.source_word(0x5ff4a0) = 0;
}
std::uint32_t dispatch_frontend_title_page_45c080(FrontendState &f, std::uint32_t page, std::uint32_t event,
                                                  std::uint32_t argument, SourcePageFactoryServices &factory,
                                                  SourceNavigationPageServices &pages, CampaignScoreMusic &music,
                                                  SourceTitlePageServices &calls)
{
    f.shared_platform()->require_owner_thread();
    const auto p = page_bytes(f, page);
    switch (event)
    {
    case 15:
    case 16:
        (void)push_frontend_page_43dfa0(f, 2, 2, factory, pages, music);
        calls.feedback_440890(f, 16);
        f.source.source_word(0x5e37a4) = argument;
        f.source.source_word(0x5ff4a0) = argument; // 43DD90 raw ECX, not float conversion
        break;
    case 19:
        reset_frontend_title_page_45c050(f, page);
        f.source.source_word(0x5ff46c) = 1; // 4400A0
        set_frontend_packed_word_43dd10(f.source, 0x112a43);
        f.source.source_word(0x5ff448) = 96; // 43DD40 ECX
        f.source.source_word(0x5ff44c) = 0x3f000000;
        f.source.source_word(0x5ff450) = 0x3e99999a;
        write_le32(p, 0x20, 0);
        break;
    case 20:
        write_le32(p, 0x20, 0x3f800000);
        break;
    case 2: {
        const auto value = approach_frontend_value_43d810(as_float(read_le32(p, 0x1c)), as_float(read_le32(p, 0x20)),
                                                          as_float(0x3a83126f));
        const auto age = read_le32(p, 0x14);
        write_le32(p, 0x1c, as_word(value));
        if (std::bit_cast<std::int32_t>(age) >= 600)
            write_le32(p, 0x18,
                       as_word(vehicle_f32(double(as_float(read_le32(p, 0x18))) + double(as_float(0x3cf5c28f)))));
        write_le32(p, 0x14, age + 1u);
        break;
    }
    case 1: {
        if (std::bit_cast<std::int32_t>(read_le32(p, 0x14)) < 3600)
            break;
        auto &index = calls.demo_index_627fdc(f);
        calls.load_demo_43b0e0(f, 0x5fc608u + index * 148u);
        const auto next = std::bit_cast<std::int32_t>(index + 1u); // reload after43B0E0
        write_le32(p, 0x14, 0);
        const auto count = std::bit_cast<std::int32_t>(f.source.source_word(0x5fcaa8));
        if (count == 0 || (next == std::numeric_limits<std::int32_t>::min() && count == -1))
            throw FormatError("title demo selection reached source IDIV fault");
        index = std::bit_cast<std::uint32_t>(next % count);
        (void)push_frontend_page_43dfa0(f, 0, 25, factory, pages, music);
        break;
    }
    case 3: {
        const auto phase_bits = read_le32(p, 0x18);
        const auto whole = static_cast<std::uint32_t>(frontend_ftol_f32_462860(phase_bits));
        const float fraction = vehicle_f32(double(as_float(phase_bits)) - double(std::bit_cast<std::int32_t>(whole)));
        const auto cycle = whole & 7u;
        float x = 111.5f;
        if (cycle == 7)
            x = vehicle_f32(111.5 - double(fraction) * 640.0);
        const std::array<std::uint32_t, 2> numeric{as_word(fraction), read_le32(p, 0x1c)};
        std::uint32_t y_bits;
        sgn_title_y_45c1eb(numeric.data(), &y_bits);
        const auto y = as_float(y_bits);
        // NEW HOST (widescreen R37): the logo carousel (second copy parked at x+640, i.e. 111.5
        // units beyond the 4:3 edge) may use at most 111.5 units of the widened frontend clip, so
        // the parked copy stays hidden on ultra-wide outputs while the logo still scrolls across
        // the whole 16:9 screen. Host clip only; no source word changes.
        host_wide_menu::ClipLimitScope carousel(111.5f);
        tiles(f, calls, x, y, true);
        const auto next_x = vehicle_f32(double(x) + 640.0);
        tiles(f, calls, next_x, y, true);
        tiles(f, calls, x, y, false);
        tiles(f, calls, next_x, y, false);
        if (cycle == 6)
        {
            volatile double clip = double(fraction) * 480.0;
            set_frontend_clip_438190(f.source,
                                     {0, as_word(vehicle_f32(clip)), 0x44200000, as_word(vehicle_f32(clip + 16.0))});
            tiles(f, calls, x, y, false);
            tiles(f, calls, next_x, y, false);
            set_frontend_clip_438190(f.source, {0, 0, 0x44200000, 0x43f00000});
        }
        const auto color = (whole & 1u) == 1u ? 0xffff6060u : 0xffffffffu;
        calls.text_438ff0(f, 2, 4, {0x43a00000, 0x43dc0000, 40, 0x3fa00000, color, 4, 0, 0x3f800000, 65, 400, 0});
        calls.text_438ba0(f, 12, 4, {0x43a00000, 0x42300000, 2, 0x3f800000, 0xffffffff, 3}); // 45C480
        break;
    }
    default:
        break;
    }
    return 0;
}
void SourceMenuTitleChildren::page_event(FrontendState &f, std::uint32_t page, std::uint32_t event,
                                         std::uint32_t argument)
{
    const auto callback = read_le32(page_bytes(f, page, 20), 16);
    if (callback == 0x45c080)
    {
        (void)dispatch_frontend_title_page_45c080(f, page, event, argument, *this, *this, title_music_, *this);
        return;
    }
    other_page_event(f, page, callback, event, argument);
}
} // namespace sgn::retail
