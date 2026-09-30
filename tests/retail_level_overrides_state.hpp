#pragma once
#include "sgn/retail_level_overrides.hpp"
#include <cstring>

namespace sgn::retail::level_override_oracle
{
constexpr unsigned record_size = 512;
// Explicit transfer contract. The original instruction emulator writes each
// field to its actual VA, independently of C++ member layout or padding.
template <class Record> void transfer(Record &record, LevelOverrideOptions &o, LevelOverrideState &s, bool read)
{
    const auto field = [&](auto &value, unsigned offset) {
        if (read)
            std::memcpy(&value, record.data() + offset, 4);
        else
            std::memcpy(record.data() + offset, &value, 4);
    };
#define FIELD(member, address, offset) field(member, offset)
    FIELD(o.selection_phase, 0x612bec, 0);
    FIELD(o.music, 0x60d730, 4);
    FIELD(o.audio_a, 0x60d734, 8);
    FIELD(o.audio_b, 0x60d738, 12);
    FIELD(o.render_a, 0x60d73c, 16);
    FIELD(o.render_b, 0x60d740, 20);
    FIELD(o.texture, 0x60d74c, 24);
    FIELD(o.split, 0x60d750, 28);
    FIELD(o.laps, 0x60d764, 32);
    FIELD(o.challenge_ticks, 0x60d768, 36);
    FIELD(o.record, 0x60d76c, 40);
    FIELD(o.record_f004, 0x60d770, 44);
    FIELD(o.record_f008, 0x60d774, 48);
    FIELD(o.record_f00c, 0x60d778, 52);
    FIELD(o.game_type, 0x60d77c, 56);
    FIELD(o.race_mode, 0x60d780, 60);
    FIELD(o.start_delay, 0x60d784, 64);
    FIELD(o.transient, 0x60d788, 68);
    FIELD(o.demo_override, 0x60df70, 72);
    FIELD(o.display_selector, 0x60df94, 76);
    FIELD(s.level.game_type, 0x60df98, 80);
    FIELD(s.level.music_track, 0x60dfa4, 84);
    FIELD(s.level.texture_suffix, 0x60dfec, 88);
    FIELD(s.level.time_ticks, 0x60eff4, 92);
    FIELD(s.level.lap_count, 0x60eff8, 96);
    FIELD(s.level.lap_record_ticks, 0x60f000, 100);
    FIELD(s.level.camera_mode, 0x60f01c, 104);
    FIELD(s.audio.scale, 0x489790, 108);
    FIELD(s.audio.input_a, 0x62e0a8, 112);
    FIELD(s.audio.input_b, 0x62e0f0, 116);
    FIELD(s.audio.output_a, 0x62e0f8, 120);
    FIELD(s.audio.output_b, 0x489798, 124);
    FIELD(s.audio.output_b_copy, 0x48979c, 128);
    FIELD(s.start_delay, 0x60f2c8, 132);
    FIELD(s.split, 0x576be0, 136);
    FIELD(s.render_a, 0x61297c, 140);
    FIELD(s.render_b, 0x612960, 144);
    FIELD(s.display_selector, 0x60f020, 148);
    FIELD(s.race_mode, 0x60eff0, 152);
    FIELD(s.demo_ticks, 0x612990, 156);
    FIELD(s.demo_active, 0x61299c, 160);
    FIELD(s.record_effc, 0x60effc, 164);
    FIELD(s.record_f004, 0x60f004, 168);
    FIELD(s.record_f008, 0x60f008, 172);
    FIELD(s.record_f00c, 0x60f00c, 176);
#undef FIELD
    if (read)
        std::memcpy(s.level.track_texture.data(), record.data() + 256, 64);
    else
        std::memcpy(record.data() + 256, s.level.track_texture.data(), 64);
}
} // namespace sgn::retail::level_override_oracle
