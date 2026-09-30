#include "sgn/retail_score_popup.hpp"
#include "sgn/host_wide_race_anchors.hpp"
#include "sgn/bit_reinterpret.hpp"
#include "sgn/format_require.hpp"
#include <bit>
#include <cstring>
#include <type_traits>

namespace sgn::retail
{
namespace
{
using Popup = VehicleScorePopup;
static_assert(std::is_trivially_copyable_v<Popup> && std::is_standard_layout_v<Popup>);
static_assert(sizeof(Popup) == 0x138 && offsetof(Popup, age) == 0x28 && offsetof(Popup, text) == 0x38);
static_assert(offsetof(Popup, category) == 0x0c && offsetof(Popup, enabled) == 0x40);
static_assert(offsetof(Popup, presentation_flags) == 0xac && sizeof(Popup) - 0xac == 35 * 4);
static_assert(std::is_trivially_copyable_v<VehicleScoreState> && std::is_standard_layout_v<VehicleScoreState>);
static_assert(offsetof(VehicleScoreState, sound_class) == 0x1380 && sizeof(VehicleScoreState) == 0x138c);
using sgn::bit_reinterpret::as_float;
std::uint32_t word(double value)
{
    return std::bit_cast<std::uint32_t>(static_cast<float>(value));
}
using sgn::format_check::require;
// Original4720D8..472227. The source's inclusive index11 reads adjacent %d
// and static-data bytes. Preserve them; do not substitute a twelfth glyph.
constexpr std::array<std::array<std::uint32_t, 7>, 12> digits = {
    {{0x0000000cu, 0x40800000u, 0x40800000u, 0x43400000u, 0x43800000u, 0x423c0000u, 0x427c0000u},
     {0x0000000cu, 0x40800000u, 0x43820000u, 0x43400000u, 0x44000000u, 0x423c0000u, 0x427c0000u},
     {0x0000000cu, 0x40800000u, 0x44010000u, 0x43400000u, 0x44400000u, 0x423c0000u, 0x427c0000u},
     {0x0000000cu, 0x40800000u, 0x44410000u, 0x43400000u, 0x44800000u, 0x423c0000u, 0x427c0000u},
     {0x0000000cu, 0x43440000u, 0x40800000u, 0x43c00000u, 0x43800000u, 0x423c0000u, 0x427c0000u},
     {0x0000000cu, 0x43440000u, 0x43820000u, 0x43c00000u, 0x44000000u, 0x423c0000u, 0x427c0000u},
     {0x0000000cu, 0x43440000u, 0x44010000u, 0x43c00000u, 0x44400000u, 0x423c0000u, 0x427c0000u},
     {0x0000000cu, 0x43440000u, 0x44410000u, 0x43c00000u, 0x44800000u, 0x423c0000u, 0x427c0000u},
     {0x0000000cu, 0x43c20000u, 0x40800000u, 0x44100000u, 0x43800000u, 0x423c0000u, 0x427c0000u},
     {0x0000000cu, 0x43c20000u, 0x43820000u, 0x44100000u, 0x44000000u, 0x423c0000u, 0x427c0000u},
     {0x0000000cu, 0x43c20000u, 0x44010000u, 0x44100000u, 0x44400000u, 0x423c0000u, 0x427c0000u},
     {0x00006425u, 0x3f800000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x3f800000u}}};
} // namespace
void submit_score_popups_401b00(ScorePopupSubmission frame, PolygonSubmissionSink &sink)
{
    // NEW HOST (wide_hud R20): popups are 2D, laid out around 320/240 by 401220.
    host_wide_overlay::AnchorScope anchor(host_wide_overlay::race::center);
    const auto bytes = std::as_bytes(std::span(&frame.score, 1));
    const auto character = [&](std::size_t at) {
        require(at < bytes.size(), "score popup text outside live score state");
        return std::bit_cast<std::int8_t>(std::to_integer<std::uint8_t>(bytes[at]));
    };
    for (std::size_t remaining = frame.score.popups.size(); remaining; --remaining)
    {
        const auto &popup = frame.score.popups[remaining - 1];
        if (popup.active != 1 || popup.age <= 2)
            continue;
        require(frame.controls.current < frame.controls.controllers.size(),
                "score popup current controller outside owner");
        if (popup.vehicle != frame.controls.controllers[frame.controls.current].vehicle)
            continue;
        std::array<std::uint32_t, 35> descriptor;
        std::memcpy(descriptor.data(), reinterpret_cast<const std::byte *>(&popup) + 0xac, sizeof descriptor);
        const auto submit = [&] {
            submit_quad_432e00({descriptor, frame.vertices, frame.selected_material, frame.materials, frame.viewport},
                               sink);
        };
        descriptor[3] = 0x3f6b851f;
        submit();
        if (popup.category <= 2)
        {
            descriptor[14] = 0;
            descriptor[1] = word((double(as_float(descriptor[4])) - 2.0) + as_float(descriptor[1]));
            std::swap(descriptor[6], descriptor[8]);
            submit();
        }
        if (popup.enabled != 1)
            continue;
        descriptor[16] = word(double(as_float(descriptor[16])) * 0.75);
        descriptor[17] = word(double(as_float(descriptor[17])) * 0.75);
        std::size_t cursor = (remaining - 1) * sizeof(Popup) + offsetof(Popup, text), length = 0;
        auto current = character(cursor);
        while (character(cursor + length) != 0)
            ++length;
        const double x = double(as_float(descriptor[14])) + as_float(descriptor[1]);
        descriptor[2] =
            word((double(as_float(descriptor[17])) * 8.0 + as_float(descriptor[2])) + as_float(descriptor[15]));
        // Initial enabled==1 guarantees a terminator by text byte9. The wider
        // representation view also preserves live subsequent character reads.
        const auto displacement = std::int32_t{24} - static_cast<std::int32_t>(length) * 34;
        descriptor[1] = word((double(displacement) * as_float(descriptor[16])) * 0.5 + x);
        while (current != 0)
        {
            const int index = int(current) - 48;
            if (index < 0 || index > 11)
                break;
            const auto &digit = digits[static_cast<unsigned>(index)];
            descriptor[18] = 0xffffff;
            descriptor[10] = digit[0];
            descriptor[4] = digit[5];
            descriptor[5] = digit[6];
            for (unsigned i = 0; i < 4; ++i)
                descriptor[6 + i] = digit[1 + i];
            descriptor[14] = word(double(as_float(digit[5])) * 0.5);
            descriptor[15] = word(double(as_float(digit[6])) * 0.5);
            descriptor[3] = 0x3f6e147b;
            descriptor[1] = word(double(as_float(descriptor[1])) - double(as_float(digit[5])) * 0.5);
            descriptor[2] = word(double(as_float(descriptor[2])) - double(as_float(digit[6])) * 0.5);
            submit();
            current = character(++cursor);
            descriptor[1] = word(double(as_float(digit[5])) * 0.5 + as_float(descriptor[1]));
            descriptor[2] = word(double(as_float(digit[6])) * 0.5 + as_float(descriptor[2]));
            descriptor[1] = word(double(as_float(descriptor[16])) * 34.0 + as_float(descriptor[1]));
        }
    }
}
} // namespace sgn::retail
