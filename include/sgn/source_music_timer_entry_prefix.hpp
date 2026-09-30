#pragma once
#include <cstdint>
#include <memory>
#include <stdexcept>

namespace sgn::retail
{
class RaceSessionPlatform;
class SourceMusicTimerCapsReceiving;
class SourceMusicTimerCapsInstalled;
struct SourceMusicTimerPrefixObservation;

// NEW host borrow machinery for an explicit receiving provider. Every access
// selects the retained platform's actual audio_stream words. There is no
// mirrored TIMECAPS, public issuer, native pointer cast or readiness flag.
class SourceMusicTimerCapsAccess final
{
    struct Scope;
    std::shared_ptr<Scope> scope_;
    explicit SourceMusicTimerCapsAccess(std::shared_ptr<Scope>);
    std::shared_ptr<RaceSessionPlatform> checked_owner() const;
    friend class SourceMusicTimerCapsInstalled;
    friend struct SourceMusicPrefixExecution;
    friend SourceMusicTimerPrefixObservation receive_source_music_timer_prefix_45fda0(
        std::shared_ptr<RaceSessionPlatform>, SourceMusicTimerCapsReceiving &);

  public:
    SourceMusicTimerCapsAccess() = delete;
    SourceMusicTimerCapsAccess(const SourceMusicTimerCapsAccess &) = default;
    SourceMusicTimerCapsAccess &operator=(const SourceMusicTimerCapsAccess &) = default;
    ~SourceMusicTimerCapsAccess() = default;
    std::uint32_t minimum() const;
    std::uint32_t maximum() const;
    void minimum(std::uint32_t) const;
    void maximum(std::uint32_t) const;
};

// Explicit receiving seam, not an installed WinMM implementation. A copied
// access object is safe to retain, but all later/foreign-thread access fails.
class SourceMusicTimerCapsReceiving
{
  public:
    virtual ~SourceMusicTimerCapsReceiving() = default;
    virtual std::uint32_t time_get_dev_caps(SourceMusicTimerCapsAccess, std::uint32_t bytes) = 0;
};

class SourceMusicTimerCapabilityTrap final : public std::runtime_error
{
    std::uint32_t result_;

  public:
    explicit SourceMusicTimerCapabilityTrap(std::uint32_t result)
        : std::runtime_error("music timer capability prefix reached source INT3 at45FDC5"), result_(result)
    {
    }
    std::uint32_t capability_result() const noexcept
    {
        return result_;
    }
    static constexpr std::uint32_t source_site = 0x0045fdc5;
};

// Observation only: no later API accepts this as a music/native admission token.
struct SourceMusicTimerPrefixObservation
{
    std::uint32_t minimum{}, maximum{}, selected_period{};
    static constexpr std::uint32_t next_source_site = 0x0045fe17;
    static constexpr bool original_function_returned = false;
};

// One source receiving prefix, from its prologue through the owned period
// request and private count3 root copy. Requires the same platform period owner
// to be prepared explicitly; it is not created here. Retains that platform
// through provider return/unwind and stops before45FE17 GetDriveTypeA.
// No public scratch/resume API or new provider. Does not admit45FB50,45FF16,
// drive/file/parser work, readiness, or the complete45FDA0 call.
SourceMusicTimerPrefixObservation receive_source_music_timer_prefix_45fda0(std::shared_ptr<RaceSessionPlatform>,
                                                                           SourceMusicTimerCapsReceiving &);
} // namespace sgn::retail
