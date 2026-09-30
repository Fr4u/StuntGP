#pragma once
#include "sgn/source_audio_primary_format.hpp"
#include <optional>
#include <variant>

namespace sgn::retail
{
struct SourceAudioListenerAcquisitionFailure
{
    std::uint32_t source_site;               // 461A89 (null primary) or461A97 (nonzero QI).
    std::optional<AudioResult> query_result; // No invented HRESULT for null primary.
};
struct SourceAudioSamplesBoundary
{
    static constexpr std::uint32_t source_site = 0x462182u;
    [[noreturn]] void resume() const;
};
struct SourceAudioListenerConfigurationBoundary
{
    static constexpr std::uint32_t source_site = 0x461aa0u;
    AudioListenerId &listener; // Same62F11C. Reload before every later setter.
    [[noreturn]] void resume() const;
};
using SourceAudioListenerAcquisitionOutcome =
    std::variant<SourceAudioListenerAcquisitionFailure, SourceAudioSamplesBoundary,
                 SourceAudioListenerConfigurationBoundary>;
// Only the continuation after an actual successful461CF0 on this native owner.
// The source mode/enabled branches precede every primary/listener access.
SourceAudioListenerAcquisitionOutcome acquire_source_audio_listener_462174(const SourceAudioPrimaryFormatResult &,
                                                                           WindowsAudioDevice &, AudioPrimaryId &,
                                                                           AudioListenerId &,
                                                                           const std::uint32_t &spatial_mode,
                                                                           std::uint32_t &spatial_enabled);
SourceAudioListenerAcquisitionOutcome acquire_platform_source_audio_listener_462174(
    const SourceAudioPrimaryFormatResult &, RaceSessionPlatform &);
} // namespace sgn::retail
