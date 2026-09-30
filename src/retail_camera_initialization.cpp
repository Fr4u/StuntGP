#include "sgn/retail_camera_initialization.hpp"
#include "sgn/source_allocation_bank.hpp"
#include "sgn/retail_camera_tables.hpp"
#include "sgn/retail_model_matrices.hpp"
#include "sgn/format_require.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <limits>
#include <string>

namespace sgn::retail
{
namespace
{
float f32(double x)
{
    return static_cast<float>(x);
}
using sgn::format_check::require;
std::uint32_t word(std::span<const std::uint8_t> data, std::size_t at)
{
    require(at <= data.size() && data.size() - at >= 4, "camera PMD read outside input");
    return std::uint32_t(data[at]) | (std::uint32_t(data[at + 1]) << 8) | (std::uint32_t(data[at + 2]) << 16) |
           (std::uint32_t(data[at + 3]) << 24);
}
CameraVector vector(std::span<const std::uint8_t> data, std::size_t at)
{
    return {std::bit_cast<float>(word(data, at)), std::bit_cast<float>(word(data, at + 4)),
            std::bit_cast<float>(word(data, at + 8)), std::bit_cast<float>(word(data, at + 12))};
}
void valid_reset(CameraModeReset reset)
{
    if (!reset.reset)
        for (auto active : reset.active)
            require(active != 1, "active camera effect target has no reset callback");
}
void valid_init(const CameraInitializationBindings &b, std::uint32_t split)
{
    if (split != 0 || (b.selected_preset >= 1 && b.selected_preset <= 4))
        valid_reset(b.effect_reset);
}
void free_orientation(CameraState &s)
{
    const auto m = camera_rotation(static_cast<std::int32_t>(s.control_angles[0] >> 12),
                                   static_cast<std::int32_t>(s.control_angles[1] >> 12), 0);
    // 44eb70 rotates (0,0,-1) with all source multiply/add operations retained.
    const CameraVector offset{f32((double(m[8]) * -1.0 + double(m[4]) * 0.0) + double(m[0]) * 0.0),
                              f32((double(m[9]) * -1.0 + double(m[5]) * 0.0) + double(m[1]) * 0.0),
                              f32((double(m[10]) * -1.0 + double(m[6]) * 0.0) + double(m[2]) * 0.0), 0};
    s.target = s.position;
    s.target.x = f32(double(offset.x) + s.position.x);
    s.target.y = f32(double(offset.y) + s.position.y);
    s.target.z = f32(double(offset.z) + s.position.z);
    const auto angle = (s.control_angles[2] >> 12) & 8191;
    s.up.x = std::bit_cast<float>(camera_detail::trig_bits[angle]);
    s.up.y = std::bit_cast<float>(camera_detail::trig_bits[angle + 2048]);
    s.up.z = 0;
}
void planes(CameraState &s, CameraPresentationState &p)
{
    const std::array<CameraVector, 4> inputs{{{-s.lens, 0, -s.half_width, 0},
                                              {s.lens, 0, -s.half_width, 0},
                                              {0, s.lens, -s.half_height, 0},
                                              {0, -s.lens, -s.half_height, 0}}};
    const auto &m = s.camera_to_world_rotation;
    for (std::size_t i = 0; i < 4; ++i)
    {
        auto v = inputs[i];
        camera_normalize(v);
        auto &out = p.planes[i];
        out.x = f32((double(v.z) * m[8] + double(v.y) * m[4]) + double(v.x) * m[0]);
        out.y = f32((double(v.x) * m[1] + double(v.z) * m[9]) + double(v.y) * m[5]);
        out.z = f32((double(v.x) * m[2] + double(v.z) * m[10]) + double(v.y) * m[6]);
    }
}
} // namespace
void camera_prepare_resources(CameraBank &bank, CameraSessionState &session, std::uint32_t scene,
                              std::optional<std::string_view> text, std::span<const std::uint8_t> pmd)
{
    if (scene != 1)
    {
        bank = {};
        return;
    }
    if (text)
    {
        camera_load_authored_text(bank, *text);
        session.authored_cycle_index = 0;
        return;
    }
    require(pmd.size() >= 328 && std::string_view(reinterpret_cast<const char *>(pmd.data()), 9) == "PMD V1.83",
            "invalid embedded camera PMD header");
    const auto count = word(pmd, 0xe8);
    if (count == 0)
        return;
    const std::uint64_t offset = 328ull + word(pmd, 0x58);
    require(count <= 32 && offset + std::uint64_t(count) * 96 <= pmd.size(),
            "embedded camera section outside bank or PMD");
    auto staged = bank;
    for (std::uint32_t i = 0; i < count; ++i)
    {
        const auto at = static_cast<std::size_t>(offset + std::uint64_t(i) * 96);
        auto &record = staged.records[i];
        record.position = vector(pmd, at + 0x10);
        record.target = vector(pmd, at + 0x30);
        record.up = vector(pmd, at + 0x40);
        record.lens = std::bit_cast<float>(word(pmd, at + 4));
        record.tracking = 0;
        record.twist = 0;
    }
    staged.count = count;
    bank = staged;
    session.authored_cycle_index = 0;
}
void camera_initialize_controllers(CameraInitializationBindings b, const SplineSet &splines, std::uint32_t split)
{
    valid_init(b, split);
    const auto spline = splines.find(1);
    b.controls.current = 0;
    auto &primary = b.controls.controllers[0];
    auto &life = b.lifecycle[0];
    primary.active = 1;
    primary.vehicle = 0;
    life.pending_record.reset();
    life.pending_mode = 0;
    primary.camera.pending_lens = 0;
    primary.camera.requested_lens = 0;
    b.presentation[0].spline = spline;
    camera_initialize_values(primary.camera, life, b.shared_model_identity);
    b.session.path_speed = 0;
    b.session.path_parameter = 0;
    for (unsigned i = 1; i < 4; ++i)
    {
        b.controls.current = i;
        b.controls.controllers[i] = primary;
        b.lifecycle[i] = life;
        b.presentation[i] = b.presentation[0];
        auto &c = b.controls.controllers[i];
        c.active = 0;
        c.vehicle = i;
        camera_restore_preset(c.camera, b.observed_vehicles[i], b.selected_preset, split, b.effect_reset,
                              b.shared_model_identity);
    }
    const unsigned enabled = (split == 1 || split == 2) ? 1 : split == 3 ? 2 : split == 4 ? 3 : 0;
    for (unsigned i = 1; i <= enabled; ++i)
        if (b.observed_vehicles[i].active == 1)
            b.controls.controllers[i].active = 1;
    b.controls.current = 0;
}
void camera_initialize_level(CameraInitializationBindings b, const CameraLevelResources &resources, std::uint32_t scene,
                             std::uint32_t split)
{
    valid_init(b, split);
    auto bank = b.bank;
    auto session = b.session;
    std::optional<std::string_view> text;
    if (scene == 1)
    {
        require(!resources.track_name.empty() && resources.track_name.size() < 96, "invalid camera track name");
        const auto path = canonical("config\\cams\\" + std::string(resources.track_name) + ".cam");
        const auto names = resources.assets.resources(path);
        if (std::find(names.begin(), names.end(), path) != names.end())
        {
            const auto raw = resources.assets.resource(path);
            text = std::string_view(reinterpret_cast<const char *>(raw.data()), raw.size());
        }
    }
    camera_prepare_resources(bank, session, scene, text, resources.track_pmd);
    b.bank = bank;
    b.session = session;
    camera_initialize_controllers(b, resources.splines, split);
}
void camera_begin_outer_frame(contact::player::Controls &controls)
{
    require(controls.current < 4, "current camera controller outside range");
    camera_clear_acceleration(controls.controllers[controls.current].camera);
}
namespace
{
void prepare_view_impl(contact::player::Controls &controls, std::array<CameraLifecycle, 4> &lives,
                       std::array<CameraPresentationState, 4> &presentations, CameraSessionState &session,
                       const SplineSet *splines, CameraPathInput input, CameraModeReset reset,
                       CameraMatrix &shared_model_identity, ViewMatrixArena *arena,
                       CameraProjectionPublication *publication, const std::uint32_t *source_spline_count = nullptr)
{
    require(controls.current < 4, "current camera controller outside range");
    auto &s = controls.controllers[controls.current].camera;
    auto &p = presentations[controls.current];
    (void)lives; // distinct source clip fields are preserved; this stage writes planes only.
    if (publication)
    {
        camera_rebuild_projection(s, shared_model_identity); // 402E90 ->44F4C0 before mode dispatch
        publication->projection = s.projection;
        publication->inverse_depth_scale = s.inverse_depth_scale;
        publication->depth_offset = s.depth_offset;
    }
    bool path_exists = false;
    if (s.mode == 3)
    {
        if (splines)
            path_exists = splines->find(1).has_value();
        else
        {
            if (!source_spline_count)
                throw SourceAllocationError(0x402eab, "source menu camera requires actual624498 registry owner");
            // 428801 reads count; zero takes42880C ->428822 without touching
            // 624474, descriptor/model pointers or even constructing a track view.
            if (*source_spline_count != 0)
                throw SourceAllocationError(0x42880e, "nonzero source spline registry requires canonical table walk");
        }
    }
    std::optional<CameraPathSample> position, target;
    if (s.mode == 3)
    {
        if (!path_exists)
            valid_reset(reset);
        else
        {
            require(p.spline.has_value(), "mode3 has no bound source spline descriptor");
            const auto spline = *p.spline;
            const auto count = spline.descriptor.point_count;
            const float offset = f32(double(std::bit_cast<std::int32_t>(spline.descriptor.opaque[2])));
            position = camera_sample_path(spline, f32(double(count) * session.path_parameter));
            const float distance = f32((double(offset) * std::bit_cast<float>(0x3a83126fu) + session.path_parameter) *
                                       std::bit_cast<std::int32_t>(count));
            target = camera_sample_path(spline, distance);
            require(std::isfinite(session.path_speed) && std::isfinite(session.path_parameter),
                    "invalid camera path playback state");
        }
    }
    if (!publication)
        camera_rebuild_projection(s, shared_model_identity);
    if (s.mode == 1 || s.mode == 2)
        free_orientation(s);
    else if (s.mode == 3)
    {
        if (!path_exists)
            camera_select_mode(s, 1, reset);
        else
        {
            s.position.x = position->position.x;
            s.position.y = f32(double(position->position.y) + 500);
            s.position.z = position->position.z;
            s.target.x = target->position.x;
            s.target.y = f32(double(target->position.y) + 500);
            s.target.z = target->position.z;
            camera_advance_path(session.path_parameter, session.path_speed, input);
            s.mode = 3;
        }
    }
    camera_finish_view_402ecc(s, p, arena);
}
// clang-format off: keep the original closing comment text (FixNamespaceComments would replace it)
} //anonymous implementation namespace
// clang-format on
void camera_prepare_view(contact::player::Controls &controls, std::array<CameraLifecycle, 4> &lives,
                         std::array<CameraPresentationState, 4> &presentations, CameraSessionState &session,
                         const SplineSet &splines, CameraPathInput input, CameraModeReset reset, CameraMatrix &identity,
                         ViewMatrixArena *arena)
{
    prepare_view_impl(controls, lives, presentations, session, &splines, input, reset, identity, arena, nullptr);
}
void camera_prepare_view_non_path(contact::player::Controls &controls, std::array<CameraLifecycle, 4> &lives,
                                  std::array<CameraPresentationState, 4> &presentations, CameraSessionState &session,
                                  CameraMatrix &identity, ViewMatrixArena &arena,
                                  CameraProjectionPublication publication)
{
    // No lookup or invented empty track. Mode3 fails at lookup after its actual
    // projection publication; other modes never consume input/reset arguments.
    prepare_view_impl(controls, lives, presentations, session, nullptr, {}, {}, identity, &arena, &publication);
}

void camera_prepare_view_source_zero_splines(contact::player::Controls &controls, std::array<CameraLifecycle, 4> &lives,
                                             std::array<CameraPresentationState, 4> &presentations,
                                             CameraSessionState &session, CameraMatrix &identity,
                                             ViewMatrixArena &arena, CameraProjectionPublication publication,
                                             const std::uint32_t &count, CameraModeReset reset)
{
    prepare_view_impl(controls, lives, presentations, session, nullptr, {}, reset, identity, &arena, &publication,
                      &count);
}

} // namespace sgn::retail

namespace sgn::retail
{
void camera_free_orientation_402ad0(CameraState &s)
{
    free_orientation(s);
}
void camera_finish_view_402ecc(CameraState &s, CameraPresentationState &p, ViewMatrixArena *arena)
{
    if (arena)
        allocate_camera_view_matrices(s, *arena, s.twist);
    else
        camera_view_basis(s);
    const auto &m = arena ? resolve_arena_matrix(*arena, s.camera_to_world_reference) : s.camera_to_world_rotation;
    s.side = {m[0], m[1], m[2], m[3]};
    p.up_axis = {m[4], m[5], m[6], m[7]};
    p.forward_axis = {m[8], m[9], m[10], m[11]};
    planes(s, p);
}
} // namespace sgn::retail
