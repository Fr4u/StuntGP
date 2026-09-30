#pragma once
#include "sgn/source_music_windows_runtime.hpp"
#include "sgn/source_boot_config.hpp"
#include "sgn/retail_campaign_score_navigation.hpp"

namespace sgn::retail
{
// One host binding retained by the platform. Dependencies must themselves
// borrow the platform weakly, so no cycle retains the game session.
class SourceMusicCallBinding final : public CampaignScoreMusic
{
    std::weak_ptr<RaceSessionPlatform> owner_;
    std::shared_ptr<SourceBootFilePlatform> files_;
    std::shared_ptr<SourceMusicTimerCapsReceiving> caps_;
    std::shared_ptr<SourceMusicDriveTypeReceiving> drive_;
    std::shared_ptr<SourceMusicDiskFreeSpaceReceiving> disk_;
    std::shared_ptr<SourceMusicWindowsRuntime> runtime_; // dies before receivers
    SourceMusicCallBinding(std::shared_ptr<RaceSessionPlatform>, std::shared_ptr<SourceBootFilePlatform>,
                           std::shared_ptr<SourceMusicTimerCapsReceiving>,
                           std::shared_ptr<SourceMusicDriveTypeReceiving>,
                           std::shared_ptr<SourceMusicDiskFreeSpaceReceiving>);
    friend class RaceSessionPlatform;

  public:
    static void install(std::shared_ptr<RaceSessionPlatform>, std::shared_ptr<SourceBootFilePlatform>,
                        std::shared_ptr<SourceMusicTimerCapsReceiving>, std::shared_ptr<SourceMusicDriveTypeReceiving>,
                        std::shared_ptr<SourceMusicDiskFreeSpaceReceiving>);
    void start(std::uint32_t requested, std::uint32_t loop) override;
    void check_health();
    void stop();
    void close();
    void advance_gain(AudioMusicState &expected_owner);
};
// Same entry used by the source race lifecycle; CampaignScoreMusic uses the
// retained binding directly. No legacy playback world is constructed here.
void start_source_music_45f8f0(RaceSessionPlatform &, std::uint32_t requested, std::uint32_t loop);
void advance_source_music_delay_441f70(RaceSessionPlatform &);
void shutdown_source_music_45fab0(RaceSessionPlatform &);
void shutdown_source_audio_4621d0(RaceSessionPlatform &);
} // namespace sgn::retail
