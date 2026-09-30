#include "sgn/source_menu_apply_options.hpp"
#include "sgn/source_menu_input.hpp"
#include "sgn/source_viewport_win64.hpp"
#include "sgn/source_numeric_win64.hpp"
#include "sgn/race_session.hpp"
#include "sgn/retail_level_overrides.hpp"
#include <cstdint>

extern "C" std::uint32_t sgn_menu_option_fild_fstp(std::uint32_t bits) noexcept;
namespace sgn::retail
{
void apply_source_menu_options_43c000(FrontendState &frontend)
{
    const auto p = frontend.shared_platform();
    p->require_owner_thread();
    auto &world = frontend.world();
    if (&world != &p->state())
        throw FormatError("43C000 frontend options belong to another world");
    const auto read = [&](std::uint32_t address) {
        return persistence_word(frontend.source.persistence.bytes, address - 0x5df560u);
    };
    // Original order: signed integer Y converted/stored to float, then X.
    // Actual x87 rounding/sticky effects are retained; no SSE/static_cast path.
    const auto y = sgn_menu_option_fild_fstp(read(0x5df5ec));
    const auto x = sgn_menu_option_fild_fstp(read(0x5df5e8));
    const auto admitted = set_viewport_origin_win64_424bd0(world.viewport, x, y, sgn_source_read_x87_control());
    if (admitted != SourceViewportAdmission::accepted)
        throw FormatError("43C014 viewport rejected inherited source x87 state");
    world.audio_level.input_b = read(0x5df5e0); // 43C019/1E, only after424BD0 returned
    const auto music = read(0x5df5dc);
    world.audio_level.input_a = music; // 43C02A before optional replacement
    if (music == 0)
        world.audio_level.input_a = 1;                     // 43C031 source minimum, not UI clamp
    world.impact_particles.rate_selector = read(0x5df5e4); // sole61297C owner
    [[maybe_unused]] const auto mode = frontend.source.source_word(0x5e37a0);
    // 43C04D calls462270, whose entire pinned body is RET. No listener reset.
    update_level_audio(world.audio_level, world.options); // 43C052 tail460BC0
}
void SourceMenuInputChildren::apply_options_43c000(FrontendState &frontend)
{
    apply_source_menu_options_43c000(frontend);
}
} // namespace sgn::retail
