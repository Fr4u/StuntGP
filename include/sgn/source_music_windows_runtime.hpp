#pragma once
#include "sgn/source_music_periodic.hpp"
#include "sgn/source_music_buffer_directsound.hpp"
#include "sgn/source_music_wave_winmm.hpp"
#include "sgn/source_music_timer_dispatch.hpp"
#include "sgn/source_application_literals.hpp"
#include "sgn/source_music_open.hpp"

namespace sgn::retail
{
struct SourceBootFilePlatform;
// Concrete composition. Construction does not create a source device, start
// music or fabricate timer/drive/disk outputs. The caller supplies the existing
// App literal reference; volume accesses remain live and checked at4602B1.
class SourceMusicWindowsRuntime final : public SourceMusicPeriodicIO,
                                        public std::enable_shared_from_this<SourceMusicWindowsRuntime>
{
    std::weak_ptr<RaceSessionPlatform> owner_;
    SourceApplicationLiteralRef literals_;
    std::shared_ptr<SourceMusicWaveWinmm> wave_;
    SourceMusicBufferDirectSound buffers_;
    SourceMusicTimerDispatch timers_; // last constructed, first destroyed
    SourceMusicWindowsRuntime(std::shared_ptr<RaceSessionPlatform>, SourceApplicationLiteralRef);
    std::shared_ptr<RaceSessionPlatform> owner() const;

  public:
    static std::shared_ptr<SourceMusicWindowsRuntime> create_runtime(std::shared_ptr<RaceSessionPlatform>);
    SourceMusicInitializationResult initialize(SourceMusicTimerCapsReceiving &, SourceMusicDriveTypeReceiving &,
                                               SourceMusicDiskFreeSpaceReceiving &);
    SourceMusicOpenResult open_source(std::span<const char>, SourceMusicOpenArguments, SourceMusicTimerCapsReceiving &,
                                      SourceMusicDriveTypeReceiving &, SourceMusicDiskFreeSpaceReceiving &);
    // Same platform music selection, CFG path/cache, stream and DirectSound buffer.
    // Returns only when45F8F0 returns; unresolved child paths throw, never
    // continue the caller with an invented success value.
    void start_music(std::uint32_t requested, std::uint32_t loop, SourceBootFilePlatform &,
                     SourceMusicTimerCapsReceiving &, SourceMusicDriveTypeReceiving &,
                     SourceMusicDiskFreeSpaceReceiving &);
    void close_source();
    void stop_music();         // 45F9D0, including its own source gates
    void advance_music_gain(); // 45F880 ->45F7D0 on this buffer registry
    void check_health();
    void apply_music_levels(std::int32_t pan, std::uint32_t volume); // 45F7D0
    std::uint32_t create(RaceSessionState &, SourceMusicBufferDescription &, AudioMusicBuffer &,
                         std::uint32_t) override;
    std::uint32_t lock(AudioBufferId, std::uint32_t, std::uint32_t, SourceMusicClearOutputs &, std::uint32_t) override;
    std::uint32_t unlock(AudioBufferId, std::span<std::uint8_t>, std::uint32_t) override;
    std::uint32_t release(AudioBufferId) override;
    std::uint32_t end_period(std::uint32_t) override;
    std::uint32_t exchange_flags(std::uint32_t &, std::uint32_t) override;
    std::uint32_t kill_timer(std::uint32_t) override;
    std::uint32_t stop_buffer(AudioBufferId) override;
    std::uint32_t set_position(AudioBufferId, std::uint32_t) override;
    std::uint32_t get_status(AudioBufferId, std::uint32_t &) override;
    std::uint16_t volume_word_4897f8() override;
    std::uint32_t set_volume(AudioBufferId, std::int32_t) override;
    std::uint32_t play(AudioBufferId, std::uint32_t, std::uint32_t, std::uint32_t) override;
    std::uint32_t read_locked(AudioFileId, std::span<std::uint8_t>, std::uint32_t) override;
    std::uint32_t start_timer(std::uint32_t, std::uint32_t, std::weak_ptr<RaceSessionPlatform>, std::uint32_t,
                              std::uint32_t) override;
    std::uint32_t cursors(AudioBufferId, SourceMusicCursorOutputs &) override;
    std::uint32_t milliseconds() override;
    std::uint32_t lock_split(AudioBufferId, std::uint32_t, std::uint32_t, SourceMusicSplitLockOutputs &,
                             std::uint32_t) override;
    std::uint32_t unlock_split(AudioBufferId, std::span<std::uint8_t>, std::uint32_t, std::span<std::uint8_t>,
                               std::uint32_t) override;
    void completion_thread(std::weak_ptr<RaceSessionPlatform>, std::uint32_t, std::uint32_t,
                           std::optional<std::uint32_t> &) override;
};
} // namespace sgn::retail
