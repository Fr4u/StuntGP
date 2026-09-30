#pragma once
#include <cstdint>
#include <limits>

namespace sgn::retail
{
// Missing process-source words only. Their native lifetime is the common
// RaceSessionPlatform::Storage::world, through its RaceSessionState member.
// Construction represents the pinned image once. Renderer recreation, binding,
// level detach and ordinary source frames do not reconstruct or clear these.
struct SessionOuterDeviceSourceState final
{
    SessionOuterDeviceSourceState() = default;
    SessionOuterDeviceSourceState(const SessionOuterDeviceSourceState &) = delete;
    SessionOuterDeviceSourceState &operator=(const SessionOuterDeviceSourceState &) = delete;
    SessionOuterDeviceSourceState(SessionOuterDeviceSourceState &&) = delete;
    SessionOuterDeviceSourceState &operator=(SessionOuterDeviceSourceState &&) = delete;
    std::uint32_t scene_active{0};    // 5DA1C8: PE zero;433EE0/433F20 own writes
    std::uint32_t finalized{0};       // 6244EC: PE zero;423400/423430, not scene_open
    std::uint32_t fog_a{0}, fog_b{0}; // 592054/58: PE zero;42F300 clears both
    std::uint32_t flag_592068{0}, flag_59206c{0}, flag_592070{0};
    std::uint32_t flag_592074{0}, flag_592078{0}; // PE zero;42F300/setup children
    std::uint32_t initialized_6244f8{0};          // solePEzero;42E6FA reads,42E712/7CF/7D9 write; not a host-ready flag
    std::uint32_t flag_6244f0{0};                 // PE zero;431379/42FA4A/42FA6B, retained42F300
};
struct SessionDemoTailSourceState final
{
    SessionDemoTailSourceState() = default;
    SessionDemoTailSourceState(const SessionDemoTailSourceState &) = delete;
    SessionDemoTailSourceState &operator=(const SessionDemoTailSourceState &) = delete;
    SessionDemoTailSourceState(SessionDemoTailSourceState &&) = delete;
    SessionDemoTailSourceState &operator=(SessionDemoTailSourceState &&) = delete;
    std::uint32_t age{720}; // 473A24: actual file bytes D0 02 00 00
    // R17: missing40DD80/40DDD0 velocity owner. PE file bytes 00 00 A0 C0
    // (-5.0f binary32); written only by40DDA8/40DDF6/40DE25.
    float velocity{-5.0f};        // 473A28
    float offset{0.0f};           // 4D5B18: PE virtual .data zero-fill, positive binary32 zero
    std::uint32_t initialized{0}; // 4D5B1C: PE zero;40DDB2/40DEA3
};
struct SessionOuterSourceState final
{
    SessionOuterSourceState() = default;
    SessionOuterSourceState(const SessionOuterSourceState &) = delete;
    SessionOuterSourceState &operator=(const SessionOuterSourceState &) = delete;
    SessionOuterSourceState(SessionOuterSourceState &&) = delete;
    SessionOuterSourceState &operator=(SessionOuterSourceState &&) = delete;
    // 612948: PE virtual zero-fill, reset446015;44EB45 tests exactly1.
    //  Source process-loop word, not the separate modern preview exit flag.
    std::uint32_t loop_exit_word_612948{0};
    [[nodiscard]] bool source_loop_exit_requested_44eb45() const noexcept
    {
        return loop_exit_word_612948 == 1u;
    }
    // PE virtual zero-fill, reset44602D/446033. The reviewed4421B0 block
    // references this pair; its runtime caller has not been established.
    // Keep both full-width words; do not bind them to the host clock or4421F0.
    std::uint32_t source_word_612974{0}, source_word_612978{0};
    // Opaque source words: PE virtual zero-fill, only direct writes are
    // the446039/446045 reset stores. No timer or host-Hz semantics inferred.
    std::uint32_t source_word_612980{0};
    std::uint32_t render_counter{0}; // 612984: PE zero;445FA0 store44603F resets
    std::uint32_t source_word_612988{0};
    // Opaque process words, initial PE zero. Only the proven445FA0 stores
    // are reconstructed here; no FPS producer or demo-start callee inferred.
    std::uint32_t source_word_61298c{0}, source_word_612994{0}, source_word_612998{0};
    std::uint32_t source_word_6129a0{0}, source_word_612be4{0}, source_word_61b62c{0};
    SessionOuterDeviceSourceState device;
    SessionDemoTailSourceState demo;
};
static_assert(sizeof(SessionOuterDeviceSourceState) == 11 * sizeof(std::uint32_t));
static_assert(sizeof(SessionDemoTailSourceState) == 4 * sizeof(std::uint32_t));
static_assert(sizeof(SessionOuterSourceState) == 27 * sizeof(std::uint32_t));
static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits == 24);
} // namespace sgn::retail
