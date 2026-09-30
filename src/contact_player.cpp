#include "sgn/contact_player.hpp"
#include <algorithm>

namespace sgn::retail::contact::player
{
std::optional<unsigned> find_controller(const Controls &controls, VehicleId vehicle)
{
    for (unsigned i = 0; i < 4; ++i)
        if (controls.controllers[i].active == 1 && controls.controllers[i].vehicle == vehicle)
            return i;
    return std::nullopt;
}
void restore_primary(Controls &controls) noexcept
{
    controls.current = 0;
}
void clear_notices(std::array<Notice, 8> &notices) noexcept
{
    for (auto &notice : notices)
        notice.remaining = 0;
}
void post_notice(Controls &controls, std::int32_t mode, std::uint32_t tick, std::int32_t duration,
                 std::string_view text, VehicleId source, std::int32_t audience, std::int32_t priority)
{
    if (mode == 3 || audience < 0 || audience > 2)
        return;
    text = text.substr(0, text.find('\0'));
    // Retail strcpy overruns the fixed field for malformed text. The new host
    // refuses it explicitly; the qualification domain is at most 63 bytes.
    if (text.size() >= 64)
        throw FormatError("retail player notice exceeds 63 bytes");
    const auto add = [&](Controller &controller) {
        if (priority == 100)
            clear_notices(controller.notices);
        for (auto &notice : controller.notices)
            if (notice.remaining == 0)
            {
                notice.remaining = duration;
                notice.initial = duration;
                notice.tick = tick;
                notice.source_vehicle = source;
                notice.audience = audience;
                std::copy(text.begin(), text.end(), notice.text.begin());
                notice.text[text.size()] = '\0';
                return;
            }
    };
    if (audience == 2)
    {
        if (auto index = find_controller(controls, source))
            add(controls.controllers[*index]);
        return;
    }
    for (auto &controller : controls.controllers)
    {
        if (controller.active != 1 || (audience == 1 && controller.vehicle == source))
            continue;
        add(controller);
    }
}
} // namespace sgn::retail::contact::player
