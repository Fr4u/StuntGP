#pragma once
#include <array>
#include <cstdint>

namespace sgn::retail
{
enum class ForceFeedbackParameterPointer
{
    not_written,
    record_condition,
    transient_periodic
};
// Only missing source words. Integrate ONE member in the persistent input
// owner; construction is the pinned PE image, never a per-frame/reset action.
struct SourceForceFeedbackStorage final
{
    SourceForceFeedbackStorage() = default;
    SourceForceFeedbackStorage(const SourceForceFeedbackStorage &) = delete;
    SourceForceFeedbackStorage &operator=(const SourceForceFeedbackStorage &) = delete;
    SourceForceFeedbackStorage(SourceForceFeedbackStorage &&) = delete;
    SourceForceFeedbackStorage &operator=(SourceForceFeedbackStorage &&) = delete;
    std::array<std::array<std::uint32_t, 8>, 4> physical_words{}; // 62D738+i*1DC; unused+14 retained
    std::uint32_t selected_data_4d5b24{};                         // normalized SOURCE identity, never a host address
    std::uint32_t initialized_576b70{}; // set BEFORE creation; no startup/shutdown reset writer
    std::array<std::uint32_t, 2> direction_576b78{};
    std::array<std::uint32_t, 2> axes_477640{0u, 4u};
    std::uint32_t axes_count_477648{1u}, gain_47764c{10000u}, deadband_477650{100u};
    // New native relocation provenance, not source scalar storage. A source
    // stack pointer cannot be serialized as a meaningful native32 pointer.
    // Its raw DWORD retains opaque bits; the actual synchronous COM argument
    // receives a live typed local. Never compare that raw DWORD to the EXE.
    std::array<std::array<ForceFeedbackParameterPointer, 12>, 10> parameter_pointer_kind{};
};
} // namespace sgn::retail
