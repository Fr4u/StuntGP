#include "sgn/retail_level_overrides.hpp"
#include <array>
#include <algorithm>
#include <stdexcept>

namespace sgn::retail
{
void update_level_audio(LevelAudioState &state, LevelOverrideOptions &options) noexcept
{
    const auto scaled = [&](std::uint32_t input) {
        const std::uint32_t q = state.scale * input;
        const std::uint32_t p = (q << 8) - q;
        return static_cast<std::uint32_t>((std::uint64_t(p) * 0xd1b71759u) >> 45);
    };
    options.audio_a = state.input_a;
    state.output_a = scaled(state.input_a);
    options.audio_b = state.input_b;
    state.output_b = scaled(state.input_b);
    state.output_b_copy = state.output_b;
}
void apply_demo_override(LevelOverrideState &state) noexcept
{
    state.demo_ticks = 3600;
    state.demo_active = 1;
    state.start_delay = 4;
    state.level.camera_mode = 10;
    state.race_mode = 1;
    state.level.game_type = 2;
}
void apply_challenge_override(LevelOverrideState &state, std::uint32_t ticks) noexcept
{
    state.level.lap_count = 100;
    state.level.texture_suffix = 2;
    state.level.time_ticks = ticks < 1200 ? 5400 : ticks;
}
void apply_level_overrides(LevelOverrideOptions &options, LevelOverrideState &state,
                           LevelOverrideDependencies &dependencies)
{
    if (options.selection_phase != 1)
        return;
    if (options.texture > 8)
        throw std::invalid_argument("level texture selector outside original table");
    options.transient = 0;
    state.level.music_track = options.music == 0 ? 1 : options.music;
    state.start_delay = options.start_delay;
    state.split = options.split;
    state.audio.input_b = options.audio_b;
    state.audio.input_a = options.audio_a;
    update_level_audio(state.audio, options);
    state.render_a = options.render_a;
    state.render_b = options.render_b;
    state.display_selector = options.display_selector == 0 ? 1 : options.display_selector;
    if (options.texture != 8)
    {
        constexpr std::array<std::string_view, 8> textures{"TRACKSET00", "TRACKSET01", "TRACKSET02", "TRACKSET03",
                                                           "TRACKSET04", "TRACKSET05", "TRACKSET06", "DUMMY"};
        const auto text = textures[options.texture];
        state.level.texture_suffix = options.texture;
        std::copy(text.begin(), text.end(), state.level.track_texture.begin());
        state.level.track_texture[text.size()] = '\0';
    }
    if (options.laps != 0)
        state.level.lap_count = options.laps;
    if (options.game_type != 0)
        state.level.game_type = options.game_type;
    if (state.level.game_type == 5)
        state.level.lap_count = 100;
    state.race_mode = options.race_mode == 0 ? 1 : options.race_mode;
    state.demo_ticks = 0;
    state.demo_active = 0;
    if (options.demo_override != 0)
        apply_demo_override(state);
    if (state.level.game_type == 4)
        dependencies.prepare_time_trial(state);
    dependencies.prepare_cars(state);
    if (state.level.game_type == 3)
        apply_challenge_override(state, options.challenge_ticks);
    state.level.lap_record_ticks = options.record;
    state.record_effc = options.record;
    state.record_f004 = options.record_f004;
    state.record_f008 = options.record_f008;
    state.record_f00c = options.record_f00c;
}
} // namespace sgn::retail
