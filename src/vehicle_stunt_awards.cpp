#include "sgn/vehicle_stunt_awards.hpp"
#include "sgn/vehicle_stunt_tables.hpp"
#include <algorithm>

namespace sgn::retail
{
namespace
{
std::uint32_t truncate_low_word(double value)
{
    if (!std::isfinite(value) || value < -9223372036854775808.0 || value >= 9223372036854775808.0)
        throw std::domain_error("stunt reward outside recovered integer conversion range");
    return static_cast<std::uint32_t>(static_cast<std::int64_t>(value));
}
} // namespace
std::int32_t vehicle_random_integer(VehicleRandom &random, std::int32_t amplitude) noexcept
{
    random.state = random.state * 0x343fdu + 0x269ec3u;
    const auto centered = static_cast<std::int32_t>((random.state >> 16) & 0x7fff) - 0x3fff;
    const auto product_bits = static_cast<std::uint32_t>(centered) * static_cast<std::uint32_t>(amplitude);
    const auto product = std::bit_cast<std::int32_t>(product_bits);
    const std::int64_t wide = std::int64_t(product) * std::bit_cast<std::int32_t>(std::uint32_t{0x80010003});
    const auto high = static_cast<std::uint32_t>(static_cast<std::uint64_t>(wide) >> 32);
    const auto shifted = std::bit_cast<std::int32_t>(high + product_bits) >> 14;
    return shifted + static_cast<std::int32_t>(static_cast<std::uint32_t>(shifted) >> 31);
}
std::uint32_t stunt_chain_points(std::uint32_t base, std::uint32_t chain)
{
    const auto &table = stunt_tables::point_steps;
    const auto found = std::find(table.begin(), table.end(), base);
    if (found == table.end())
        throw std::domain_error("stunt base score absent from original step table");
    auto index = static_cast<std::uint32_t>(found - table.begin()) + chain;
    if (index > 23)
        index = 23;
    return table[index];
}
void reward_stunt_energy(VehicleState &s, std::uint32_t percent)
{
    const auto amount = truncate_low_word((double(percent) * s.energy_capacity) * 0.01f);
    const double value = double(amount) + s.energy;
    s.energy = vehicle_f32(value);
    if (value > s.energy_capacity)
        s.energy = s.energy_capacity;
}
void award_stunt_pattern(VehicleState &s, contact::player::Controls &controls, std::uint32_t id,
                         VehicleScoreState &score, const VehicleStuntContext &context,
                         const VehicleStuntRewards &rewards, VehicleRandom &random, VehicleStuntChainAudio &audio,
                         const VehicleStuntPattern &pattern, unsigned repetitions)
{
    if (repetitions < 1 || repetitions > 3)
        throw std::domain_error("stunt repetition count outside source range");
    const auto index = repetitions - 1, score_index = pattern.score_indices[index];
    if (score_index >= rewards.points.size())
        throw std::domain_error("stunt score index outside source table");
    const auto points = stunt_chain_points(rewards.points[score_index], s.stunt.displayed_chain_count);
    if (!contact::player::find_controller(controls, id) || context.hud_block == 1)
    {
        s.score = vehicle_f32(double(std::bit_cast<std::int32_t>(points)) + s.score);
        return;
    }
    auto category = score_index >= 5000 ? 4u : (score_index < 10000 ? 3u : 5u);
    category += index;
    if (category > 5)
        category = 5;
    const auto message = pattern.messages[index];
    if (message >= rewards.messages.size() || s.stunt.displayed_chain_count >= stunt_tables::chain_sounds.size())
        throw std::domain_error("stunt message or chain sound outside source table");
    contact::player::post_notice(controls, context.game_mode, context.tick, 180, rewards.messages[message], id, 2, 0);
    const auto variant = vehicle_random_integer(random, 1) + 1;
    award_stunt_score(s, controls, id, score, context, category, static_cast<std::uint32_t>(variant), points);
    const float energy = s.upgrade_active == 1 ? rewards.upgraded_energy : rewards.normal_energy;
    reward_stunt_energy(s, truncate_low_word(energy));
    audio.play_chain(stunt_tables::chain_sounds[s.stunt.displayed_chain_count]);
}
void recognize_stunt(VehicleState &s, contact::player::Controls &controls, std::uint32_t id, VehicleScoreState &score,
                     const VehicleStuntContext &context, const VehicleStuntRewards &rewards, VehicleRandom &random,
                     VehicleStuntChainAudio &audio)
{
    if (context.race_mode == 4)
        return;
    for (unsigned i = 0; i < 10; ++i)
        if (const auto count = match_stunt_pattern(s, stunt_tables::patterns[i]))
        {
            award_stunt_pattern(s, controls, id, score, context, rewards, random, audio, stunt_tables::patterns[i],
                                count);
            return;
        }
    const auto pitch = s.stunt.phase_indices[2], yaw = s.stunt.phase_indices[3];
    if (pitch > 16 || yaw > 16)
        award_stunt_pattern(s, controls, id, score, context, rewards, random, audio, stunt_tables::patterns[11], 1);
    else if (pitch > 10 || yaw > 10)
        award_stunt_pattern(s, controls, id, score, context, rewards, random, audio, stunt_tables::patterns[10], 1);
}
void complete_stunt_landing(VehicleState &s, contact::player::Controls &controls, std::uint32_t id,
                            VehicleScoreState &score, const VehicleStuntContext &context,
                            const VehicleStuntRewards &rewards, VehicleRandom &random, VehicleStuntChainAudio &audio)
{
    auto &t = s.stunt;
    t.landing_counter = 15;
    t.landing_hold = 0;
    t.landing_pending = 0;
    if (t.previous_active == 1)
        recognize_stunt(s, controls, id, score, context, rewards, random, audio);
    record_stunt_height(s);
    record_stunt_statistic(s.stunt_statistics[1], 100);
    record_stunt_statistic(s.stunt_statistics[2], vehicle_f32(double(s.score) - s.launch_score));
    s.stunt_statistics[2].average = vehicle_f32(double(s.score) / s.stunt_statistics[2].count);
    t.displayed_chain_count = t.chain_count;
    reset_stunt_meter(s);
}
void update_stunt_landing(VehicleState &s, contact::player::Controls &controls, std::uint32_t id,
                          VehicleScoreState &score, const VehicleStuntContext &context,
                          const VehicleStuntRewards &rewards, VehicleRandom &random, VehicleStuntChainAudio &audio)
{
    auto &t = s.stunt;
    if (t.landing_hold == 0)
        return;
    --t.landing_hold;
    if (t.landing_hold == 0)
        t.previous_active = 0;
    if (t.landing_counter == 1)
    {
        if (t.landing_hold == 0)
        {
            fail_stunt_landing(s);
            return;
        }
        if (s.contact_class == 0 && ++t.landing_pending > 10)
            complete_stunt_landing(s, controls, id, score, context, rewards, random, audio);
    }
    else
    {
        ++t.landing_pending;
        if (double(t.landing_pending) >= 600.0f)
        {
            fail_stunt_landing(s);
            return;
        }
        for (const auto &wheel : s.wheels)
            if (wheel.contact_countdown < 4)
                return;
        t.landing_pending = 0;
        t.landing_counter = 1;
    }
}
} // namespace sgn::retail
