#include "sgn/source_menu_command_queue.hpp"
#include "sgn/race_session.hpp"
#include "sgn/host_wide_menu.hpp"
#include "sgn/bit_reinterpret.hpp"
#include <bit>

namespace sgn::retail
{
namespace
{
using sgn::bit_reinterpret::as_float;
using sgn::bit_reinterpret::as_signed;
std::uint32_t stored(double v)
{
    return std::bit_cast<std::uint32_t>(vehicle_f32(v));
}
std::uint32_t &payload_word(FrontendSourceStorage &s, std::uint32_t identity)
{
    if (identity < 0x5da374u || identity > 0x5df190u || (identity & 3u) != 0)
        throw FormatError("frontend command pointer outside its payload owner");
    return s.menu_payload_5da374[(identity - 0x5da374u) / 4];
}
} // namespace
std::uint32_t allocate_frontend_command_438550(FrontendSourceStorage &s, std::uint32_t bytes) noexcept
{
    const auto old = s.menu_counter_5da370;
    const auto next = ((bytes + 3u) & 0xfffffffcu) + old;
    const auto identity = 0x5da374u + old;
    if (as_signed(next) > 20000)
        return 0; // 43856A signed JLE, source wrap retained
    const auto peak = s.menu_peak_5df514;
    s.menu_counter_5da370 = next;
    if (as_signed(next) > as_signed(peak))
        s.menu_peak_5df514 = next;
    return identity;
}
void link_frontend_command_438590(FrontendSourceStorage &s, std::uint32_t ecx, std::uint32_t edx, std::uint32_t command)
{
    const auto list = edx + ecx * 4u;
    if (list >= 80)
        throw FormatError("frontend command list index outside existing80 lists");
    const auto previous_tail = s.menu_words_5df194[list];
    payload_word(s, command) = 0; // 4385AA before the empty-list branch
    if (previous_tail == 0)
    {
        s.menu_words_5df194[list] = command;
        s.menu_words_5df194[80 + list] = command;
    }
    else
    {
        payload_word(s, previous_tail) = command;
        s.menu_words_5df194[list] = command;
    }
}
void queue_frontend_rectangle_438500(FrontendSourceStorage &s, std::uint32_t ecx, std::uint32_t edx,
                                     const std::array<std::uint32_t, 12> &descriptor)
{
    const auto command = allocate_frontend_command_438550(s, 56);
    if (command == 0)
        return; // full queue silently omits this command in the source
    for (unsigned i = 0; i < 12; ++i)
        payload_word(s, command + 8u + i * 4u) = descriptor[i];
    payload_word(s, command + 4u) = 1; // command tag after all12 payload stores
    link_frontend_command_438590(s, ecx, edx, command);
    host_wide_menu::queued_command(command); // NEW R20 (wide_menu) host tag of this command identity, no source state
}
void clip_frontend_rectangle_438370(FrontendSourceStorage &s, const std::array<std::uint32_t, 12> &input,
                                    std::uint32_t layer, std::uint32_t argument)
{
    // Clip words are signed integers (source FILD), not stored float bits.
    // NEW HOST (widescreen R37): a full-frame clip reaches the real screen edges on a
    // wide canvas (host bounds; the stored clip words are unchanged). Identity on 4:3.
    host_wide_menu::FrontendClipBounds wide;
    const bool widened = host_wide_menu::widened_frontend_clip(s.frontend_clip, wide);
    const float left = widened ? wide.left : vehicle_f32(double(as_signed(s.frontend_clip[0])));
    if (!(double(as_float(input[3])) + double(as_float(input[1])) >= double(left)))
        return;
    const float top = widened ? wide.top : vehicle_f32(double(as_signed(s.frontend_clip[1])));
    if (!(double(as_float(input[4])) + double(as_float(input[2])) >= double(top)))
        return;
    const float right = widened ? wide.right : vehicle_f32(double(as_signed(s.frontend_clip[2])));
    if (double(as_float(input[1])) > double(right))
        return;
    const float bottom = widened ? wide.bottom : vehicle_f32(double(as_signed(s.frontend_clip[3])));
    if (double(as_float(input[2])) > double(bottom))
        return;
    auto d = input; // 4383F3, after the four source visibility guards
    if (!(as_float(d[1]) >= left))
    {
        const double distance = double(left) - double(as_float(d[1]));
        d[1] = std::bit_cast<std::uint32_t>(left);
        const float fraction = vehicle_f32(distance / double(as_float(d[3]))); // 438414 spill
        d[3] = stored(double(as_float(d[3])) - distance);
        volatile double product = (double(as_float(d[7])) - double(as_float(d[5]))) * double(fraction);
        d[5] = stored(product + double(as_float(d[5])));
    }
    const double edge_x = double(as_float(d[3])) + double(as_float(d[1]));
    if (edge_x > double(right))
    {
        const double fraction = (edge_x - double(right)) / double(as_float(d[3])); // unspilled ST0
        d[3] = stored(double(right) - double(as_float(d[1])));
        volatile double product = (double(as_float(d[5])) - double(as_float(d[7]))) * fraction;
        d[7] = stored(product + double(as_float(d[7])));
    }
    if (!(as_float(d[2]) >= top))
    {
        const double distance = double(top) - double(as_float(d[2]));
        d[2] = std::bit_cast<std::uint32_t>(top);
        const float fraction = vehicle_f32(distance / double(as_float(d[4]))); // 43847D spill
        d[4] = stored(double(as_float(d[4])) - distance);
        volatile double product = (double(as_float(d[8])) - double(as_float(d[6]))) * double(fraction);
        d[6] = stored(product + double(as_float(d[6])));
    }
    const double edge_y = double(as_float(d[4])) + double(as_float(d[2]));
    if (edge_y > double(bottom))
    {
        const double fraction = (edge_y - double(bottom)) / double(as_float(d[4]));
        d[4] = stored(double(bottom) - double(as_float(d[2])));
        volatile double product = (double(as_float(d[6])) - double(as_float(d[8]))) * fraction;
        d[8] = stored(product + double(as_float(d[8])));
    }
    if (!(as_float(d[3]) > 1.0f) || !(as_float(d[4]) > 1.0f))
        return;
    queue_frontend_rectangle_438500(s, layer, argument, d);
}
void SourceMenuCommandQueueChildren::rectangle_438370(FrontendState &f, const std::array<std::uint32_t, 12> &descriptor,
                                                      std::uint32_t layer, std::uint32_t argument)
{
    f.shared_platform()->require_owner_thread();
    clip_frontend_rectangle_438370(f.source, descriptor, layer, argument);
}
} // namespace sgn::retail
