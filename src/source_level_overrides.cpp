#include "sgn/source_level_overrides.hpp"
#include "sgn/source_application_literals.hpp"
#include "sgn/race_session.hpp"
#include "sgn/race_session_platform.hpp"
#include <cstring>

namespace sgn::retail
{
void apply_source_demo_override_445310(RaceSessionPlatform &platform)
{
    platform.require_owner_thread();
    auto &s = platform.state();
    s.demo_ticks = 3600;           // 445315
    s.hud_block = 1;               // 44531F
    s.clock.countdown_pending = 4; // 445324
    s.level.camera_mode = 10;      // 44532E
    s.clock.race_mode = 1;         // 445338
    s.level.game_type = 2;         // 44533D
}
void apply_source_challenge_override_445120(RaceSessionPlatform &platform)
{
    platform.require_owner_thread();
    auto &s = platform.state();
    const auto ticks = s.options.challenge_ticks; // 445120, before any write
    s.clock.lap_count = 100;                      // 445125
    s.level.texture_suffix = 2;                   // 445134
    s.clock.remaining_ticks = ticks;              // 44513E, then conditional second store
    if (ticks < 1200u)
        s.clock.remaining_ticks = 5400; // 445145 unsigned
}
void apply_source_level_overrides_4453a0(RaceSessionPlatform &platform, SourceLevelOverrideChildren &children)
{
    platform.require_owner_thread();
    auto &s = platform.state();
    auto &options = s.options;
    if (options.selection_phase != 1)
        return;
    auto eax = options.music;  // 4453B3
    options.transient = 0;     // 4453B8 before music publication
    s.level.music_track = eax; // 4453C4
    if (eax == 0)
        s.level.music_track = 1;
    eax = options.start_delay;
    auto ecx = options.split;
    auto edx = options.audio_b;
    s.clock.countdown_pending = eax; // 4453E2
    eax = options.audio_a;
    s.split_setting = ecx;
    s.audio_level.input_b = edx;
    s.audio_level.input_a = eax;
    update_level_audio(s.audio_level, options); // 4453FD ->460BC0, actual owners
    eax = options.display_selector;
    ecx = options.render_a;
    edx = options.render_b;
    s.impact_particles.rate_selector = ecx; // 445413
    s.render_b = edx;                       // 44541B
    s.selected_camera_preset = eax;         // 445421
    if (eax == 0)
        s.selected_camera_preset = 1;
    eax = options.texture;
    if (eax != 8)
    {
        const auto app = platform.source_application_literals();
        // Source computes uint32 table VA. Bounds are checked only by the
        // reached App read; earlier writes are never preflighted or rolled back.
        const auto source = app.read_texture_override_pointer_445439(0x482becu + eax * 4u);
        s.level.texture_suffix = eax; // 445440 BEFORE strlen or copy
        std::uint32_t length = 0;
        while (app.read_texture_override_byte(source + length, 0x44544a) != 0)
            ++length;
        ++length; // includes NUL; source NOT ECX after REPNE SCASB
        std::uint32_t offset = 0;
        auto &target = s.level.track_texture;
        for (std::uint32_t count = length >> 2; count != 0; --count)
        {
            const auto word = app.read_texture_override_word_44545c(source + offset);
            if (offset > target.size() || target.size() - offset < 4)
                throw SourceBootConfigError(0x44545c, "Texture DWORD store exceeds actual Session local64 owner");
            // Native Windows little endian; the source read precedes the store.
            std::memcpy(target.data() + offset, &word, 4);
            offset += 4;
        }
        for (std::uint32_t count = length & 3u; count != 0; --count)
        {
            const auto byte = app.read_texture_override_byte(source + offset, 0x445463);
            if (offset >= target.size())
                throw SourceBootConfigError(0x445463, "Texture BYTE store exceeds actual Session owner");
            target[offset++] = static_cast<char>(byte);
        }
    }
    eax = options.laps;
    if (eax != 0)
        s.clock.lap_count = eax;
    eax = options.game_type;
    if (eax != 0)
        s.level.game_type = eax;
    if (s.level.game_type == 5)
        s.clock.lap_count = 100;
    eax = options.race_mode;
    s.clock.race_mode = eax;
    if (eax == 0)
        s.clock.race_mode = 1;
    eax = options.demo_override;
    s.demo_ticks = 0; // 4454B3
    s.hud_block = 0;  // 4454BF
    if (eax != 0)
        apply_source_demo_override_445310(platform);
    if (s.level.game_type == 4)
        children.prepare_time_trial_445360(platform);
    children.prepare_cars_444db0(platform); // unconditional4454DE
    if (s.level.game_type == 3)
        apply_source_challenge_override_445120(platform);
    eax = options.record;
    ecx = options.record_f004;
    edx = options.record_f008;
    s.records.lap_ticks = eax;          // 445502
    s.records.previous_lap_ticks = eax; // 445507
    eax = options.record_f00c;
    s.finish.best_total_ticks = ecx; // 445511
    s.records.score = edx;           // 445517
    s.finish.best_total_score = eax; // 44551D
}
void SourceLevelScriptChildrenImpl::apply_overrides_4453a0(RaceSessionPlatform &platform)
{
    apply_source_level_overrides_4453a0(platform, children_);
}
} // namespace sgn::retail
