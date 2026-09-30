#pragma once
#include "sgn/source_music_periodic.hpp"

namespace sgn::retail
{
// Concrete device operations on the already retained source DirectSound owner.
// This is a fidelity adapter, separate from NEW's XAudio2 preview generation.
class SourceMusicBufferDirectSound final : public SourceMusicLockIO
{
    struct Impl;
    std::unique_ptr<Impl> impl_;

  public:
    explicit SourceMusicBufferDirectSound(std::shared_ptr<RaceSessionPlatform>);
    ~SourceMusicBufferDirectSound() override;
    SourceMusicBufferDirectSound(const SourceMusicBufferDirectSound &) = delete;
    SourceMusicBufferDirectSound &operator=(const SourceMusicBufferDirectSound &) = delete;
    std::uint32_t create(RaceSessionState &, SourceMusicBufferDescription &, AudioMusicBuffer &, std::uint32_t outer);
    std::uint32_t lock(AudioBufferId, std::uint32_t, std::uint32_t, SourceMusicClearOutputs &, std::uint32_t) override;
    std::uint32_t lock_split(AudioBufferId, std::uint32_t, std::uint32_t, SourceMusicSplitLockOutputs &, std::uint32_t);
    std::uint32_t unlock(AudioBufferId, std::span<std::uint8_t>, std::uint32_t) override;
    std::uint32_t unlock_split(AudioBufferId, std::span<std::uint8_t>, std::uint32_t, std::span<std::uint8_t>,
                               std::uint32_t);
    std::uint32_t release(AudioBufferId);
    std::uint32_t status(AudioBufferId, std::uint32_t &);
    std::uint32_t cursors(AudioBufferId, SourceMusicCursorOutputs &);
    std::uint32_t volume(AudioBufferId, std::int32_t);
    std::uint32_t pan(AudioBufferId, std::int32_t);
    std::uint32_t play(AudioBufferId, std::uint32_t, std::uint32_t, std::uint32_t);
    std::uint32_t stop(AudioBufferId);
    std::uint32_t position(AudioBufferId, std::uint32_t);
    void clear_source_buffer(); // actual45FD40 on this owner's current buffer
};
} // namespace sgn::retail
