#pragma once
#include "sgn/retail_audio_driver.hpp"
#include <type_traits>

struct IDirectSoundBuffer;

namespace sgn::retail
{
// NEW Windows x64 transport for the existing source word 62DF08. A numeric
// token is only a borrowed identity; it is never proof of a COM reference.
class AudioMusicBufferValue
{
    IDirectSoundBuffer *cell_{};

  public:
    constexpr AudioMusicBufferValue() noexcept = default;
    AudioMusicBufferValue(std::uint64_t value) noexcept
    {
        *this = value;
    }
    AudioMusicBufferValue &operator=(std::uint64_t value) noexcept
    {
        cell_ = value == 0 ? nullptr : reinterpret_cast<IDirectSoundBuffer *>(static_cast<std::uintptr_t>(value));
        return *this;
    }
    operator std::uint64_t() const noexcept
    {
        return cell_ == nullptr ? 0 : static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(cell_));
    }
    // Synchronous borrowed output only. The source owner must remain alive
    // and stationary until return/unwind. No reset, Release or mirror store.
    IDirectSoundBuffer **native_address() & noexcept
    {
        return &cell_;
    }
    IDirectSoundBuffer *native_value() const noexcept
    {
        return cell_;
    }
    // Fixture source DWORD writes preserve the pre-existing high host bits.
    void source_low_word(std::uint32_t word) noexcept
    {
        *this = (static_cast<std::uint64_t>(*this) & 0xffffffff00000000ull) | word;
    }
};
struct AudioMusicBuffer
{
    AudioMusicBufferValue value{};
    AudioBufferId borrowed() const noexcept
    {
        return {static_cast<std::uint64_t>(value)};
    }
    bool operator==(const AudioMusicBuffer &other) const noexcept
    {
        return static_cast<std::uint64_t>(value) == static_cast<std::uint64_t>(other.value);
    }
};
static_assert(sizeof(IDirectSoundBuffer *) == sizeof(std::uint64_t));
static_assert(sizeof(std::uintptr_t) == sizeof(std::uint64_t));
static_assert(sizeof(AudioMusicBuffer) == sizeof(AudioBufferId));
static_assert(alignof(AudioMusicBuffer) == alignof(AudioBufferId));
static_assert(std::is_trivially_copyable_v<AudioMusicBuffer>);
static_assert(std::is_standard_layout_v<AudioMusicBuffer>);
static_assert(!std::is_convertible_v<AudioMusicBuffer, AudioBufferId>);
} // namespace sgn::retail
