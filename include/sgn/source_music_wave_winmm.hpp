#pragma once
#include "sgn/source_music_wave_open.hpp"
#include <memory>

namespace sgn::retail
{
class RaceSessionPlatform;
// Native file boundary only. No timer, COM device or game callback is installed.
// One backend per owning platform; identifiers are never native pointer bits.
class SourceMusicWaveWinmm final : public SourceMusicWaveIO, public SourceMusicLockedReadIO
{
    struct Impl;
    std::unique_ptr<Impl> impl_;

  public:
    explicit SourceMusicWaveWinmm(std::shared_ptr<RaceSessionPlatform>);
    ~SourceMusicWaveWinmm() override;
    SourceMusicWaveWinmm(const SourceMusicWaveWinmm &) = delete;
    SourceMusicWaveWinmm &operator=(const SourceMusicWaveWinmm &) = delete;
    AudioFileId open(AudioStreamState &, std::array<std::uint32_t, 18> &, std::uint32_t) override;
    std::uint32_t descend(AudioFileId, SourceMusicWaveChunkAccess &, SourceMusicWaveChunkAccess *,
                          std::uint32_t) override;
    std::uint32_t read(AudioFileId, SourceMusicWaveBytes &, std::uint32_t) override;
    std::uint32_t ascend(AudioFileId, SourceMusicWaveChunkAccess &, std::uint32_t) override;
    std::uint32_t seek(AudioFileId, std::uint32_t, std::uint32_t) override;
    std::uint32_t close(AudioFileId, std::uint32_t) override;
    // Same read implementation for live locked regions; requires actual capacity.
    std::uint32_t read_locked(AudioFileId, std::span<std::uint8_t>, std::uint32_t) override;
    // Direct wiring to the recovered bodies on this backend's actual owner.
    // Keep this backend alive while its file is open; no temporary session.
    std::uint32_t open_source_file();
    std::uint32_t read_source_stream(std::span<std::uint8_t>, std::uint32_t requested_bits);
};
} // namespace sgn::retail
