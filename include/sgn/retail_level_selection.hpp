#pragma once
#include "sgn/retail_level_overrides.hpp"
#include "sgn/retail_replay.hpp"
#include "sgn/retail_world.hpp"
#include <cstddef>

namespace sgn::retail
{
// 43A4A0 writes this separate configuration record. Selection only copies it;
// decoding its 64 tuning fields is outside this module's recovered contract.
// Keeping explicit source words here does not claim implemented tuning behavior.
struct SelectionConfiguration
{
    std::uint32_t present{};
    std::array<std::uint32_t, 64> tuning_words{};
};
struct SelectionPreviousRace
{
    std::int32_t lap_index{};
    std::uint32_t finish_state_plus_one{}, best_lap_ticks{};
    std::int32_t race_timer{};
    std::uint32_t total_ticks{}, stunt_score{};
};
struct SelectionRecord
{
    std::uint32_t kind{}, control{}, ai_skill{}, ai_mode{};
    std::array<char, 20> name{};
    std::uint32_t model{}, config{}, livery{}, tyre{};
    SelectionConfiguration configuration;
    SelectionPreviousRace previous_race;
};
struct LevelCarSlot
{
    // 60F02C/30 are preserved/copied by 444DB0, without assigned semantics.
    std::uint32_t source_word00{}, source_word04{}, model{};
    LevelCarSettings settings;
    std::array<char, 64> name{};
};
struct VehicleTextureSelection
{
    std::array<std::int8_t, 6> random_liveries{}; // sole4D5B28..2D
    std::array<std::uint8_t, 6> used_liveries{};  // sole4D5BB0..B5
};
// 413110 runs unconditionally at the start of413190, including when every
// model has an explicit livery. It consumes the shared463289 random stream.
void randomize_vehicle_liveries(VehicleTextureSelection &, VehicleRandom &);
static_assert(sizeof(SelectionConfiguration) == 0x104);
static_assert(sizeof(SelectionRecord) == 0x150 && offsetof(SelectionRecord, model) == 0x24);
static_assert(offsetof(SelectionRecord, configuration) == 0x34 && offsetof(SelectionRecord, previous_race) == 0x138);
static_assert(sizeof(LevelCarSlot) == 0x64 && offsetof(LevelCarSlot, settings) == 0x0c &&
              offsetof(LevelCarSlot, name) == 0x24);

// Original triples hold replay-header +3C, a vehicle pointer and a replay
// pointer. Native identities refer to the one world and the one ReplayBank.
struct GhostSelection
{
    std::uint32_t header_value{};
    std::optional<unsigned> vehicle;
    std::optional<ReplayHandle> replay;
};
struct TimeTrialSelectionState
{
    std::array<std::uint32_t, 4> availability{}; // 60D668/66C/670/674
    std::uint32_t split_selection{};             // 60D754
    std::uint32_t previous_completed{};          // 60DF74
    GhostSelection track, selected;              // 60DF78,60DF84
    GhostSelection result_a, result_b;           // 60F288,60F294
};
struct VehicleStorageClear
{
    virtual ~VehicleStorageClear() = default;
    // Mandatory synchronous write to every actual world-owned vehicle store.
    // The assembler qualifies this separately from selection's caller body.
    virtual void clear_vehicle_storage() = 0; // 444DB0 first 6*7724 bytes
};
struct LevelSelectionContext
{
    std::span<SelectionRecord, 6> selection;
    std::span<LevelCarSlot, 6> cars;
    ReplayBank &replays;
    TimeTrialGhosts &ghosts;
    TimeTrialSelectionState &time_trial;
    VehicleRandom &random;
    const AssetStore &assets;
    LevelAssetBindings &bindings;
    VehicleStorageClear &vehicles;
    std::string_view level_name; // original 589604, e.g. TRACK1
    std::uint32_t memory_bytes;  // 477C30, source low-memory branch retained
};
void reset_time_trial_ghosts(TimeTrialGhosts &, TimeTrialSelectionState &); // 443060
// Imports one complete authored RPL into arena slot0, retaining header+10
// source address bits. Its stale cursor must not be used before begin_playback.
// Partial headers and files larger than one slot are explicitly unsupported.
void load_track_replay(LevelSelectionContext &);                              // 442EF0 + 4199F0 resource path
void prepare_time_trial_selection(LevelSelectionContext &);                   // 445150
void prepare_time_trial_level(LevelSelectionContext &, LevelOverrideState &); // 445360
void prepare_level_cars(LevelSelectionContext &);                             // 444DB0 with mandatory world clear
class LevelSelectionDependencies final : public LevelOverrideDependencies
{
    LevelSelectionContext context_;

  public:
    explicit LevelSelectionDependencies(LevelSelectionContext context) : context_(context) {}
    void prepare_time_trial(LevelOverrideState &) override;
    void prepare_cars(LevelOverrideState &) override;
};
} // namespace sgn::retail
