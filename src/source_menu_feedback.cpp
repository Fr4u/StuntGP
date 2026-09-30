#include "sgn/source_menu_feedback.hpp"
#include "sgn/retail_frontend_state.hpp"
#include "sgn/race_session_platform.hpp"
#include <array>
#include <bit>
namespace sgn::retail
{
namespace
{
constexpr std::array<std::uint32_t, 21> samples{{51u, 52u, 53u, 54u, 55u, 56u, 57u, 58u, 59u, 60u, 61u,
                                                 62u, 63u, 0u,  0u,  66u, 67u, 68u, 69u, 70u, 71u}}; // 440824 targets
constexpr std::array<std::uint32_t, 7> events{{1u, 1u, 1u, 1u, 3u, 3u, 2u}};                         // 4408C0 targets
} // namespace
std::uint32_t source_menu_sample_4407a0(std::uint32_t feedback)
{
    const auto index = feedback - 1u;
    return index <= 20u ? samples[index] : 0u; // unsigned JA, including feedback0
}
void source_menu_feedback_440770(FrontendState &f, std::uint32_t feedback, std::uint32_t frequency,
                                 SourceMenuSampleReceiving &receiver)
{
    f.shared_platform()->require_owner_thread();
    if (std::bit_cast<std::int32_t>(f.source.source_word(0x5ff8c0)) <= 0)
        return;
    const auto sample = source_menu_sample_4407a0(feedback);
    if (sample == 0)
        return;
    receiver.start_461150(f, sample, 0, frequency, 255, 0, 0, nullptr);
}
void source_menu_feedback_440880(FrontendState &f, std::uint32_t feedback, SourceMenuSampleReceiving &receiver)
{
    source_menu_feedback_440770(f, feedback, 0, receiver);
}
void source_menu_feedback_440890(FrontendState &f, std::uint32_t event, SourceMenuSampleReceiving &receiver)
{
    f.shared_platform()->require_owner_thread();
    const auto index = event - 11u;
    if (index > 6u)
        return;
    source_menu_feedback_440880(f, events[index], receiver);
}
} // namespace sgn::retail
