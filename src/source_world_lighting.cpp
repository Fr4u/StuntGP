#include "sgn/source_world_lighting.hpp"
#include "sgn/race_session.hpp"
#include "sgn/retail_time_trial.hpp"
#include "sgn/retail_camera_tables.hpp"
#include <bit>
#include <cstring>
#include <limits>

extern "C" std::uint32_t sgn_source_light_fade_42d88f(void *, std::uint32_t) noexcept;
extern "C" std::uint32_t sgn_source_light_visibility_42d9e7(void *, const void *const *) noexcept;
extern "C" void sgn_source_light_rotation_44fb00(const float *, float *) noexcept;
extern "C" void sgn_source_light_rotate_44eb8f(float *, const float *) noexcept;

namespace sgn::retail
{
namespace
{
using W = std::uint32_t;
W add(W a, W b)
{
    if (b > std::numeric_limits<W>::max() - a)
        throw SourceAllocationError(0x42dcc8, "source light offset overflow");
    return a + b;
}
template <class T> T read(std::span<const std::uint8_t> bytes, W offset)
{
    if (offset > bytes.size() || sizeof(T) > bytes.size() - offset)
        throw SourceAllocationError(0x42d9e7, "source light field outside owner");
    T value;
    std::memcpy(&value, bytes.data() + offset, sizeof(T));
    return value;
}
void write(std::span<std::uint8_t> bytes, W offset, W value)
{
    if (offset > bytes.size() || 4 > bytes.size() - offset)
        throw SourceAllocationError(0x42dd4b, "source light store outside owner");
    std::memcpy(bytes.data() + offset, &value, 4);
}
auto static_bytes(RaceSessionState &state)
{
    return std::span<std::uint8_t>(reinterpret_cast<std::uint8_t *>(&state.lights.source_menu_light_589628), 0x90);
}
void admit(const std::shared_ptr<RaceSessionPlatform> &platform, LightCameraView view)
{
    if (!platform || !platform->source_construction_only_entry())
        throw SourceAllocationError(0x42d9b0, "source light view requires actual source platform");
    platform->require_owner_thread();
    const auto &state = platform->state();
    const auto index = state.controls.current;
    if (index >= state.controls.controllers.size() || index >= state.camera_lifecycle.size() ||
        index >= state.camera_presentation.size() || index >= state.overlays.size() ||
        &view.camera != &state.controls.controllers[index].camera ||
        &view.lifecycle != &state.camera_lifecycle[index] || &view.presentation != &state.camera_presentation[index] ||
        &view.overlay != &state.overlays[index])
        throw SourceAllocationError(0x42d9b0, "light view does not borrow current camera from the same platform");
}
SourceSelectedLight pointed(const std::shared_ptr<RaceSessionPlatform> &platform,
                            const std::variant<SourcePmdLocation, SourceSceneStaticPointer> &base, W offset)
{
    if (const auto *pmd = std::get_if<SourcePmdLocation>(&base))
        return SourceSceneAllocationPointer::bind(platform, platform->source_allocations(),
                                                  {pmd->allocation, add(pmd->offset, offset)});
    if (offset)
        throw SourceAllocationError(0x42dcc8, "source light leaves the represented static589628 record");
    return std::get<SourceSceneStaticPointer>(base);
}
} // namespace
std::span<std::uint8_t> source_light_record(const std::shared_ptr<RaceSessionPlatform> &platform,
                                            const SourceSelectedLight &pointer)
{
    if (!platform)
        throw SourceAllocationError(0x42b490, "source light platform absent");
    platform->require_owner_thread();
    if (const auto *pmd = std::get_if<SourceSceneAllocationPointer>(&pointer))
    {
        const auto at = pmd->require_location(platform, platform->source_allocations());
        const auto bytes = platform->source_allocations().bytes(at.allocation);
        if (at.offset > bytes.size() || 0x90u > bytes.size() - at.offset)
            throw SourceAllocationError(0x42b490, "source light record outside actual bank allocation");
        return bytes.subspan(at.offset, 0x90);
    }
    if (const auto *literal = std::get_if<SourceSceneStaticPointer>(&pointer))
    {
        auto bytes = static_bytes(platform->state());
        if (*literal != SourceSceneStaticPointer::bind(platform, 0x589628, bytes))
            throw SourceAllocationError(0x42b490,
                                        "source light static pointer belongs to another owner or unrepresented range");
        literal->check();
        return bytes;
    }
    throw SourceAllocationError(0x42b490, "reached null source light pointer");
}
W source_light_pointer_token(const std::shared_ptr<RaceSessionPlatform> &platform, const SourceSelectedLight &pointer)
{
    if (!platform)
        throw SourceAllocationError(0x42b490, "source light pointer transport has no platform");
    platform->require_owner_thread();
    if (const auto *pmd = std::get_if<SourceSceneAllocationPointer>(&pointer))
    {
        const auto at = pmd->require_location(platform, platform->source_allocations());
        return platform->source_allocations().encode_address(at.allocation, at.offset);
    }
    if (std::holds_alternative<SourceSceneStaticPointer>(pointer))
    {
        (void)source_light_record(platform, pointer);
        return 0x589628;
    }
    return 0;
}
SourceSelectedLight source_light_pointer_from_token(const std::shared_ptr<RaceSessionPlatform> &platform, W token)
{
    if (!platform)
        throw SourceAllocationError(0x42b490, "source light pointer transport has no platform");
    platform->require_owner_thread();
    if (token == 0x589628)
        return SourceSceneStaticPointer::bind(platform, token, static_bytes(platform->state()));
    if (!token)
        return std::monostate{};
    return SourceSceneAllocationPointer::bind(platform, platform->source_allocations(),
                                              platform->source_allocations().decode_address(token));
}
void prepare_source_light_view_42d9b0(const std::shared_ptr<RaceSessionPlatform> &platform, LightCameraView view,
                                      LightGammaDevice *gamma)
{
    admit(platform, view);
    auto &state = platform->state();
    // Host representation admission only. An existing source tail is kept.
    state.light_selection.ordered.bind_initial_source();
    static_assert(offsetof(LightFadeState, target) == 0 && offsetof(LightFadeState, current) == 4 &&
                  offsetof(LightFadeState, blend) == 8);
    const auto factor =
        sgn_source_light_fade_42d88f(&state.light_fade, view.overlay.active == 1 || state.split_setting != 0 ? 1u : 0u);
    set_light_gamma(state.light_fade, std::bit_cast<float>(factor), gamma); // 42D948/42D967, existing4320C0 receiver
    const auto &globals = platform->source_scene_resources().models().source_loading_state().track;
    const auto first_count = globals.render_light_count;
    const auto base = globals.lights;
    state.light_selection.active_count = 0; // 42D9D1, even count0
    if (!first_count)
        return;
    // 42D9E0 captures the current camera AFTER the42D870 device callback.
    // The incoming view supplied the earlier fade owner; gamma may re-enter
    // host code and change the active controller. Do not retain that camera.
    const auto camera_index = state.controls.current;
    if (camera_index >= state.controls.controllers.size() || camera_index >= state.camera_lifecycle.size() ||
        camera_index >= state.camera_presentation.size())
        throw SourceAllocationError(0x42d9e0, "current source camera is outside its actual owners");
    const auto &camera = state.controls.controllers[camera_index].camera;
    const auto &presentation = state.camera_presentation[camera_index];
    const auto &lifecycle = state.camera_lifecycle[camera_index];
    W ordinal = 0, offset = 0;
    do
    {
        const auto pointer = pointed(platform, base, offset);
        auto record = source_light_record(platform, pointer);
        if (read<W>(record, 0) != 0)
        {
            const auto kind = read<W>(record, 4);
            if (kind <= 1)
            {
                const std::array<const void *, 5> owners{&camera.position.x, &presentation.forward_axis.x,
                                                         presentation.planes.data(), &lifecycle.clip_far,
                                                         &lifecycle.clip_near};
                // Reached fields of the actual144-byte record, direct x87
                // stores and source comparisons; preserve opaque uninitialized W.
                const auto included = sgn_source_light_visibility_42d9e7(record.data(), owners.data());
                if (included)
                {
                    const auto slot = state.light_selection.active_count;
                    state.light_selection.ordered.publish_source(slot, pointer); // 42DCAD before count reload
                    state.light_selection.active_count = state.light_selection.active_count + 1u; // 42DCB9
                }
            }
        }
        const auto count = globals.render_light_count; // fresh42DCC2, original base retained
        offset = add(offset, 0x90);
        ++ordinal;
        if (ordinal >= count)
            break;
    } while (true);
}
void prepare_source_animated_light_view_42dcf0(const std::shared_ptr<RaceSessionPlatform> &platform,
                                               LightCameraView view, LightGammaDevice *gamma)
{
    prepare_source_light_view_42d9b0(platform, view, gamma);
    auto &state = platform->state();
    const auto fast = state.light_animation.phase_fast, slow = state.light_animation.phase_slow;
    // 624530 pointer is captured without a payload read, even when NULL.
    const auto &base = state.light_vertices.source_binding();
    if (base.bank != &platform->source_allocations() || base.platform.lock() != platform)
        throw SourceAllocationError(0x42dd02, "source light buffer belongs to another platform");
    state.light_animation.phase_fast = (fast + 32u) & 8191u;
    const auto initial = state.light_selection.active_count;
    state.light_animation.phase_slow = (slow + 2u) & 8191u;
    state.light_vertex_offset = 0;
    if (!initial)
        return;
    W ordinal = 0;
    do
    {
        auto record = source_light_record(platform, state.light_selection.ordered.source_at(ordinal));
        write(record, 0x30, 0);
        const auto z = read<W>(record, 0x5c);
        write(record, 0x34, 0);
        const auto y = read<W>(record, 0x58), x = read<W>(record, 0x54);
        write(record, 0x38, 0x3f800000);
        const auto trig = [](W angle, W addend) {
            return std::bit_cast<float>(camera_detail::trig_bits[(angle & 8191u) + addend]);
        };
        const std::array<float, 6> samples{trig(x, 2048), trig(y, 2048), trig(z, 2048),
                                           trig(x, 0),    trig(y, 0),    trig(z, 0)};
        sgn_source_light_rotation_44fb00(samples.data(), state.source_light_rotation_61c3a4.data());
        sgn_source_light_rotate_44eb8f(reinterpret_cast<float *>(record.data() + 0x30),
                                       state.source_light_rotation_61c3a4.data());
        const auto count = state.light_selection.active_count;
        ++ordinal;
        if (ordinal >= count)
            break;
    } while (true);
}
} // namespace sgn::retail
