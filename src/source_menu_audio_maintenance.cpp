#include "sgn/source_menu_audio_maintenance.hpp"
#include "sgn/retail_frontend_state.hpp"
#include "sgn/race_session.hpp"
#include "sgn/source_music_calls.hpp"
#include "sgn/retail_audio_maintenance.hpp"
#include "sgn/platform_audio.hpp"

namespace sgn::retail
{
void recycle_source_samples_460df0(RaceSessionPlatform &p)
{
    p.require_owner_thread();
    auto &driver = p.state().audio_driver;
    if (driver.duplicate_count == 0)
        return;
    for (unsigned i = 72; i < 104; ++i)
    {
        auto &slot = driver.samples[i];
        const auto buffer = slot.buffer;
        if (buffer.value == 0)
            continue;
        const auto spatial = slot.spatial; // 460E16 before GetStatus
        auto &native = p.source_audio_owner();
        std::uint32_t status = 0;
        if (native.source_sample_status(buffer, status) != 0 || status != 0)
            continue;
        if (spatial.value != 0)
            (void)native.source_sample_release_spatial(spatial);
        (void)native.source_sample_release(buffer);
        const auto previous_count = driver.duplicate_count; // 460E3C after both releases
        slot.buffer = {};
        const auto next_count = previous_count - 1u;
        slot.spatial = {};
        driver.duplicate_count = next_count;
    }
}
void recount_source_samples_462200(RaceSessionPlatform &p)
{
    p.require_owner_thread();
    auto &driver = p.state().audio_driver;
    auto limit = driver.duplicate_count + 72u; // wrapping ADD before initial zero store
    driver.playing_count = 0;
    if (limit == 0)
        return;
    std::uint32_t i = 0;
    do
    {
        if (i >= driver.samples.size())
            throw FormatError("46221F recount exceeds source sample owner");
        const auto buffer = driver.samples[i].buffer;
        if (buffer.value != 0)
        {
            std::uint32_t status = 0;
            if (p.source_audio_owner().source_sample_status(buffer, status) == 0 && (status & 1u) != 0)
                ++driver.playing_count;
        }
        const auto count = driver.duplicate_count; // 46223F reload after call
        ++i;
        limit = count + 72u;
    } while (i < limit);
}
void maintain_source_menu_audio_462280(FrontendState &frontend)
{
    const auto p = frontend.shared_platform();
    p->require_owner_thread();
    if (!p->source_construction_only_entry())
        throw FormatError("source menu maintenance requires source platform");
    auto &world = p->state();
    if (world.audio_driver.initialized == 0)
        return;
    recycle_source_samples_460df0(*p);
    if (world.audio_driver.spatial_mode == 1)
    {
        const auto listener = world.audio_maintenance.listener; // 462297 retained call-local identity
        auto &controller = world.controls.controllers[0];
        if (listener.value != 0 && controller.active != 0)
        {
            auto &native = p->source_audio_owner();
            const auto position = controller.camera.position;
            (void)native.source_sample_listener_position(listener, {position.x, position.y, position.z});
            const auto velocity = controller.camera.velocity; // reload after first native call
            (void)native.source_sample_listener_velocity(listener, {velocity.x, velocity.y, velocity.z});
            const auto forward = world.camera_presentation[0].forward_axis;
            const auto up = world.camera_presentation[0].up_axis;
            (void)native.source_sample_listener_orientation(listener, {forward.x, forward.y, forward.z},
                                                            {-up.x, -up.y, -up.z});
        }
        recount_source_samples_462200(*p);
    }
    // 45F880 source disabled gate precedes the runtime requirement. Native
    // callback-health checks belong to host message dispatch, not this source block.
    if (world.audio_level.output_a != 0)
    {
        const auto binding = p->source_music_calls();
        if (!binding)
            throw SourceMusicWaveUnknown("4622AC music gain requires installed source runtime");
        binding->advance_gain(world.audio_maintenance.music);
    }
    audio_advance_sound_gain(world.audio_level); // 4622B1 tail460CE0
}
} // namespace sgn::retail
