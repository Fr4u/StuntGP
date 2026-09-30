#pragma once
#include "sgn/session_menu_state.hpp"

namespace sgn::retail
{
// New synchronous binding for the two source pointer slots. This object owns
// no source state. The caller must retain the slots through every text child.
// It deliberately resolves only the proved initial literal targets; additional
// mutable text targets require their own reconstructed lifetime/owner binding.
class SessionPauseMeterLabels final : public PauseMeterLabels
{
    const SessionPauseMeterLabelSlots &slots_;
    static std::span<const std::uint8_t> resolve(std::uint32_t identity)
    {
        switch (identity)
        {
        case 0x004857b0u:
            return session_pause_meter_initial_text::minus;
        case 0x004857b4u:
            return session_pause_meter_initial_text::plus;
        default:
            throw FormatError("pause meter label target has no reconstructed owner binding");
        }
    }

  public:
    explicit SessionPauseMeterLabels(SessionPauseMeterLabelSlots &slots) noexcept : slots_(slots) {}
    SessionPauseMeterLabels(const SessionPauseMeterLabels &) = delete;
    SessionPauseMeterLabels &operator=(const SessionPauseMeterLabels &) = delete;
    SessionPauseMeterLabels(SessionPauseMeterLabels &&) = delete;
    SessionPauseMeterLabels &operator=(SessionPauseMeterLabels &&) = delete;
    // 446E13 and446E3F are separate source reads with a complete text child
    // between them. Do not pre-read or validate the other pointer slot here.
    std::span<const std::uint8_t> read_left_4851e0() override
    {
        return resolve(static_cast<const volatile std::uint32_t &>(slots_.left));
    }
    std::span<const std::uint8_t> read_right_4851e4() override
    {
        return resolve(static_cast<const volatile std::uint32_t &>(slots_.right));
    }
};
} // namespace sgn::retail
