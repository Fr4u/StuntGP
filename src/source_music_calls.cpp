#include "sgn/source_music_calls.hpp"
#include "sgn/race_session_platform.hpp"
#include "sgn/race_session.hpp"

namespace sgn::retail
{
SourceMusicCallBinding::SourceMusicCallBinding(std::shared_ptr<RaceSessionPlatform> p,
                                               std::shared_ptr<SourceBootFilePlatform> files,
                                               std::shared_ptr<SourceMusicTimerCapsReceiving> caps,
                                               std::shared_ptr<SourceMusicDriveTypeReceiving> drive,
                                               std::shared_ptr<SourceMusicDiskFreeSpaceReceiving> disk)
    : owner_(p), files_(std::move(files)), caps_(std::move(caps)), drive_(std::move(drive)), disk_(std::move(disk)),
      runtime_(SourceMusicWindowsRuntime::create_runtime(std::move(p)))
{
}
void SourceMusicCallBinding::install(std::shared_ptr<RaceSessionPlatform> p,
                                     std::shared_ptr<SourceBootFilePlatform> files,
                                     std::shared_ptr<SourceMusicTimerCapsReceiving> caps,
                                     std::shared_ptr<SourceMusicDriveTypeReceiving> drive,
                                     std::shared_ptr<SourceMusicDiskFreeSpaceReceiving> disk)
{
    if (!p || !files || !caps || !drive || !disk)
        throw std::invalid_argument("source music binding requires all retained dependencies");
    p->require_owner_thread();
    if (!p->source_construction_only_entry())
        throw std::logic_error("source music cannot bind preview platform");
    if (p->source_music_calls())
        throw std::logic_error("source music binding already installed");
    auto binding = std::shared_ptr<SourceMusicCallBinding>(
        new SourceMusicCallBinding(p, std::move(files), std::move(caps), std::move(drive), std::move(disk)));
    p->bind_source_music_calls(std::move(binding));
}
std::shared_ptr<SourceMusicCallBinding> RaceSessionPlatform::source_music_calls() const
{
    require_owner_thread();
    return source_music_calls_;
}
void RaceSessionPlatform::bind_source_music_calls(std::shared_ptr<SourceMusicCallBinding> value)
{
    require_owner_thread();
    if (!value || value->owner_.lock().get() != this)
        throw std::logic_error("source music binding belongs to another platform");
    if (source_music_calls_)
        throw std::logic_error("source music binding replacement forbidden");
    source_music_calls_ = std::move(value);
}
void SourceMusicCallBinding::start(std::uint32_t requested, std::uint32_t loop)
{
    const auto p = owner_.lock();
    if (!p)
        throw std::logic_error("source music binding platform expired");
    p->require_owner_thread();
    runtime_->start_music(requested, loop, *files_, *caps_, *drive_, *disk_);
}
void SourceMusicCallBinding::check_health()
{
    runtime_->check_health();
}
void SourceMusicCallBinding::stop()
{
    runtime_->stop_music();
}
void SourceMusicCallBinding::close()
{
    runtime_->close_source();
}
void SourceMusicCallBinding::advance_gain(AudioMusicState &expected)
{
    const auto p = owner_.lock();
    if (!p)
        throw std::logic_error("source music gain owner expired");
    p->require_owner_thread();
    if (&expected != &p->state().audio_maintenance.music)
        throw std::logic_error("source music maintenance belongs to another platform");
    runtime_->advance_music_gain();
}
void start_source_music_45f8f0(RaceSessionPlatform &p, std::uint32_t requested, std::uint32_t loop)
{
    p.require_owner_thread();
    // Source disabled gates do not require host audio construction.
    if (p.state().audio_driver.initialized == 0 || p.state().audio_level.output_a == 0)
        return;
    const auto binding = p.source_music_calls();
    if (!binding)
        throw SourceMusicWaveUnknown("45F8F0 source music dependencies not installed");
    binding->start(requested, loop);
}
void advance_source_music_delay_441f70(RaceSessionPlatform &p)
{
    p.require_owner_thread();
    auto &delay = p.state().startup.start_delay_ticks; // actual60D618
    auto value = delay;
    if (value == 0)
        return;
    --value;
    delay = value; // 441F7A: decrement persists even if the tail call stops
    if (value == 0)
        start_source_music_45f8f0(p, 0, 1);
}
void shutdown_source_music_45fab0(RaceSessionPlatform &p)
{
    p.require_owner_thread();
    if (p.state().audio_level.output_a == 0)
        return;
    // 45FB40 is only a tail jump to45FCE0; no additional source effects.
    const auto binding = p.source_music_calls();
    if (binding)
    {
        binding->close();
        return;
    }
    if (p.state().audio_stream.allocated != 0)
        throw SourceMusicWaveUnknown("45FAB0 allocated stream lacks native close binding");
    // Unallocated45FCE0 returns before any native dependency is needed.
}
} // namespace sgn::retail
