#pragma once
#include <cstdint>

namespace sgn::retail
{
class FrontendState;
// Required source461150 receiver, intentionally not a WindowsAudioDevice or
// preview AudioDriver adapter. Invocation is synchronous on the frontend owner.
struct SourceMenuSampleReceiving
{
    virtual ~SourceMenuSampleReceiving() = default;
    virtual void start_461150(FrontendState &, std::uint32_t sample, std::uint32_t flags, std::uint32_t frequency,
                              std::uint32_t volume, std::int32_t pan, std::uint32_t force, std::uint32_t *handle) = 0;
};
std::uint32_t source_menu_sample_4407a0(std::uint32_t feedback);
void source_menu_feedback_440770(FrontendState &, std::uint32_t feedback, std::uint32_t frequency,
                                 SourceMenuSampleReceiving &);
void source_menu_feedback_440880(FrontendState &, std::uint32_t feedback, SourceMenuSampleReceiving &);
void source_menu_feedback_440890(FrontendState &, std::uint32_t event, SourceMenuSampleReceiving &);
} // namespace sgn::retail
