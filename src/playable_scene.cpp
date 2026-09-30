#include "sgn/playable_scene.hpp"
#include "sgn/playable_camera.hpp"
#include "sgn/live_scene_geometry.hpp"
#include "sgn/live_draw_batch.hpp"
#include "sgn/retail_model_matrices.hpp"
#include "sgn/playable_vehicle_stages.hpp"
#include "sgn/source_pmd_geometry.hpp"
#include <algorithm>

namespace sgn::retail
{
namespace
{
std::string_view track_prefix(const std::array<char, 64> &value)
{
    const auto end = std::find(value.begin(), value.end(), '\0');
    if (end == value.end())
        throw FormatError("source track texture prefix lacks terminator");
    return {value.data(), static_cast<std::size_t>(end - value.begin())};
}
} // namespace
struct PlayableScene::Impl final
{
    struct Entry
    {
        std::size_t handle{};
        std::optional<unsigned> vehicle;
        bool sky{}, shadow{};
        std::vector<std::size_t> meshes;
        LiveModelViews views;
    };
    RaceSession &owner;
    PlayableVehicleStages stages;
    std::vector<Entry> entries;
    std::vector<std::shared_ptr<const LiveGeometry>> geometry;
    std::vector<SceneInstance> instances;
    std::vector<LiveInstanceView> live_views;
    sgn::View view;
    ImmutablePresentationSnapshot published;
    std::uint64_t camera_continuity{};
    std::array<std::uint64_t, 6> last_vehicle_continuity{};
    bool pending_camera_cut{};
    std::uint32_t last_controller{}, last_camera_mode{}, last_target_vehicle{};
    std::optional<unsigned> last_camera_record;
    explicit Impl(RaceSession &session)
        : owner(session), stages(session), last_vehicle_continuity(session.state().presentation_vehicle_continuity)
    {
        const auto add = [&](std::size_t handle, std::optional<unsigned> vehicle, bool sky, bool shadow = false) {
            Entry entry;
            entry.handle = handle;
            entry.vehicle = vehicle;
            entry.sky = sky;
            entry.shadow = shadow;
            const auto count = vehicle ? session.models().distinct_lod_count(handle) : std::size_t{1};
            for (std::size_t lod = 0; lod < count; ++lod)
            {
                auto mesh = model_live_geometry(session.models(), handle, static_cast<unsigned>(lod),
                                                track_prefix(session.state().level.track_texture));
                coalesce_live_draws(mesh);
                entry.meshes.push_back(geometry.size());
                geometry.push_back(std::make_shared<const LiveGeometry>(std::move(mesh)));
            }
            entries.push_back(std::move(entry));
        };
        const auto track = session.scene_resources().model_handle(3);
        if (!track)
            throw FormatError("playable scene requires actual track model");
        add(*track, std::nullopt, false); // mesh zero stays the actual track texture binding
        if (const auto sky = session.scene_resources().model_handle(2))
            add(*sky, std::nullopt, true);
        // Original background shadow commands precede the regular car queue.
        // Renderer retains its documented opaque/translucent two-pass policy.
        for (unsigned slot = 0; slot < 6; ++slot)
            if (const auto handle = session.state().vehicles.models[slot].shadow)
                add(*handle, slot, false, true);
        for (unsigned slot = 0; slot < 6; ++slot)
            if (const auto handle = session.state().vehicles.models[slot].model)
                add(*handle, slot, false);
        instances.reserve(entries.size());
        live_views.reserve(entries.size());
        // Startup retains the source iteration cursor on inactive slot3. Wait
        // for actual405210 to select slot0; do not advance camera in construction.
    }
    void upload(Renderer &renderer) const
    {
        std::vector<Model> models;
        models.reserve(geometry.size());
        for (const auto &mesh : geometry)
            models.push_back(mesh->model);
        renderer.load_scene(models, owner.assets());
        for (std::size_t mesh = 0; mesh < geometry.size(); ++mesh)
            renderer.stage_mesh(mesh, *geometry[mesh]);
    }
    void stage(Renderer &renderer) const
    {
        for (const auto &instance : instances)
            renderer.stage_mesh(instance.mesh, *geometry[instance.mesh]);
    }
    void update()
    {
        if (published && published->tick == owner.state().simulation_tick)
            throw FormatError("playable source stages already ran for this tick");
        const auto snapshot = playable_camera_after_tick(owner, stages);
        view = playable_camera_view(snapshot);
        const auto &s = owner.state();
        instances.clear();
        live_views.clear();
        auto next = std::make_shared<PresentationSnapshot>();
        next->tick = s.simulation_tick;
        next->view = view;
        const auto controller = snapshot.controller();
        const auto &control = s.controls.controllers[controller];
        const auto record = s.camera_lifecycle[controller].current_record;
        const auto target_continuity = s.presentation_vehicle_continuity.at(control.vehicle);
        const bool target_reset = last_vehicle_continuity.at(control.vehicle) != target_continuity;
        // New presentation policy:405210 reads vehicles before443A80/418A90
        // can reset them. Cut both the reset snapshot and the following camera
        // snapshot, which first observes the placement. Never infer cuts from
        // position deltas or change the source update order.
        if (!published || last_controller != controller || last_camera_mode != control.camera.mode ||
            last_target_vehicle != control.vehicle || last_camera_record != record || target_reset ||
            pending_camera_cut)
            ++camera_continuity;
        last_controller = controller;
        last_camera_mode = control.camera.mode;
        last_target_vehicle = control.vehicle;
        last_camera_record = record;
        pending_camera_cut = target_reset;
        last_vehicle_continuity = s.presentation_vehicle_continuity;
        next->camera_continuity = camera_continuity;
        next->objects.reserve(entries.size());
        for (auto &entry : entries)
        {
            const auto *prepared =
                entry.vehicle ? (entry.shadow ? stages.shadow_view(*entry.vehicle) : stages.car_view(*entry.vehicle))
                              : stages.static_view(owner.models().identity(entry.handle));
            if (!prepared)
                continue;
            const auto &command = prepared->command;
            if (owner.models().identity(entry.handle) != command.model_identity)
                throw FormatError("playable source model identity changed after upload");
            const auto lod = owner.models().source_lod_index(entry.handle, prepared->source_lod);
            if (lod >= entry.meshes.size())
                throw FormatError("playable scene selected LOD was not loaded");
            const auto &pose = command.pose;
            if (entry.vehicle)
            {
                // Brake/UV/forced flags and shadow multiply attributes are
                // actual mutable PMD fields written during the shared CPU stage.
                const auto mesh = entry.meshes[lod];
                auto live = model_live_geometry(owner.models(), entry.handle, prepared->source_lod,
                                                track_prefix(s.level.track_texture));
                coalesce_live_draws(live);
                geometry[mesh] = std::make_shared<const LiveGeometry>(std::move(live));
            }
            const auto &outer = prepared->outer;
            entry.views = prepared->views;
            const auto &m = outer.rotation;
            const Pose instance_pose{{m[0], m[1], m[2]},
                                     {m[4], m[5], m[6]},
                                     {m[8], m[9], m[10]},
                                     {outer.position.x, outer.position.y, outer.position.z},
                                     pose.scale.x};
            instances.push_back({entry.meshes[lod], instance_pose, entry.views.parts});
            live_views.push_back({entry.views.model_to_view});
            // New presentation identity names the draw entity, not its mutable
            // LOD or resource token. Every vector below owns its storage.
            const auto identity =
                entry.vehicle
                    ? (0x2000000000000000ull | std::uint64_t(*entry.vehicle) * 2u | std::uint64_t(entry.shadow))
                    : (0x1000000000000000ull | std::uint64_t(entry.sky));
            const auto continuity = entry.vehicle ? s.presentation_vehicle_continuity.at(*entry.vehicle) : 0;
            next->objects.push_back({identity, continuity, entry.meshes[lod], instance_pose, entry.views.parts,
                                     entry.views.model_to_view, geometry[entry.meshes[lod]]});
        }
        validate_instances(instances, geometry.size());
        published = std::move(next);
    }
};
PlayableScene::PlayableScene(RaceSession &session) : impl_(std::make_unique<Impl>(session)) {}
PlayableScene::PlayableScene(RaceSession &session, Renderer &renderer) : PlayableScene(session)
{
    impl_->upload(renderer);
}
PlayableScene::~PlayableScene() = default;
void PlayableScene::update_after_tick(RaceSession &session)
{
    if (&session != &impl_->owner)
        throw FormatError("playable scene is bound to a different Session");
    impl_->update();
}
void PlayableScene::update_after_tick(RaceSession &session, Renderer &renderer)
{
    update_after_tick(session);
    impl_->stage(renderer);
}
ImmutablePresentationSnapshot PlayableScene::snapshot() const noexcept
{
    return impl_->published;
}
void PlayableScene::rebind_renderer(Renderer &renderer) const
{
    impl_->upload(renderer);
}
void PlayableScene::resize(RaceSession &session, unsigned width, unsigned height)
{
    if (&session != &impl_->owner)
        throw FormatError("playable scene is bound to a different Session");
    playable_camera_resize(session, width, height);
}
const sgn::View &PlayableScene::view() const noexcept
{
    return impl_->view;
}
std::span<const SceneInstance> PlayableScene::instances() const noexcept
{
    return impl_->instances;
}
std::span<const LiveInstanceView> PlayableScene::live_views() const noexcept
{
    return impl_->live_views;
}
} // namespace sgn::retail
