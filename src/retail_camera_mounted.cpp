#include "sgn/retail_camera_mounted.hpp"
#include "sgn/assets.hpp"
#include "sgn/model_resources.hpp"
#include "camera_mounted_provider.hpp"
#include <bit>

namespace sgn::retail
{
namespace
{
float f32(double value)
{
    return static_cast<float>(value);
}
double rotate_component(const CameraMatrix &matrix, CameraVector axis, unsigned component)
{
    return (double(axis.z) * matrix[8 + component] + double(axis.y) * matrix[4 + component]) +
           double(axis.x) * matrix[component];
}
} // namespace
void detail::camera_mounted_core(CameraState &camera, CameraVehicle &vehicle, const MountedCameraProvider &provider)
{
    if (camera.mode != 6)
        throw FormatError("mounted camera requires source mode6");
    camera.previous_target = camera.target;
    vehicle.orbit_offset.x = 0;
    vehicle.orbit_offset.y = 100;
    vehicle.orbit_offset.z = 0; // 404D09..404D19; retain W
    provider.orbit_written(provider.context, vehicle.orbit_offset);
    const float rx = f32(double(vehicle.orbit_offset.x) * vehicle.right.x);
    const float ry = f32(double(vehicle.orbit_offset.x) * vehicle.right.y);
    const float rz = f32(double(vehicle.orbit_offset.x) * vehicle.right.z);
    const double ux = double(vehicle.orbit_offset.y) * vehicle.up.x; // 404D64 remains on x87
    const float uy = f32(double(vehicle.orbit_offset.y) * vehicle.up.y);
    const float uz = f32(double(vehicle.orbit_offset.y) * vehicle.up.z);
    const float fx = f32(double(vehicle.orbit_offset.z) * vehicle.forward.x);
    const float fy = f32(double(vehicle.orbit_offset.z) * vehicle.forward.y);
    const float fz = f32(double(vehicle.orbit_offset.z) * vehicle.forward.z);
    const float ox = f32(double(fx) + (ux + rx));
    const float oy = f32(double(fy) + f32(double(uy) + ry));
    const float oz = f32(double(fz) + f32(double(uz) + rz));
    camera.position.x = f32(double(ox) + vehicle.position.x);
    camera.position.y = f32(double(oy) + vehicle.position.y);
    camera.position.z = f32(double(oz) + vehicle.position.z);
    auto up = provider.axis(provider.context, 0x404e1c);
    up.x = -up.x; // 404E48; no normalization
    camera.up.x = f32(rotate_component(vehicle.orientation, up, 0));
    camera.up.y = f32(rotate_component(vehicle.orientation, up, 1));
    camera.up.z = f32(rotate_component(vehicle.orientation, up, 2));
    auto forward = provider.axis(provider.context, 0x404ec8);
    forward.y = -forward.y; // 404EF4
    const double x = rotate_component(vehicle.orientation, forward, 0);
    const float y = f32(rotate_component(vehicle.orientation, forward, 1));
    const float z = f32(rotate_component(vehicle.orientation, forward, 2));
    camera.target.x = f32(x * 256.0 + camera.position.x); // 46E2E0; X is not stored before addition
    camera.target.y = f32(double(f32(double(y) * 256.0)) + camera.position.y);
    camera.target.z = f32(double(f32(double(z) * 256.0)) + camera.position.z);
    camera.twist = 0;
    camera_move(camera, vehicle, {}); // 403460 returns before any collision or bypass write
    camera_translate(camera, vehicle);
}
namespace detail
{
struct SourcePmdMountedAccess
{
    static SourcePmdPartView part(SourcePmdByteView file, std::uint32_t head)
    {
        return {file, file.relocated_part(head)};
    }
};
CameraVector mounted_axis(SourcePmdModelView model, std::uint32_t site)
{
    const auto file = model.bytes();
    const auto head = model.descriptor().field<std::uint32_t>(0x48).value();
    const auto part = SourcePmdMountedAccess::part(file, head);
    const auto transform = part.transform().location(); // reuse physical section, alignment and full record admission
    const auto at = transform.offset + (site == 0x404e1c ? 0x30u : 0x20u);
    // 404E31..404E42 / 404EDD..404EEE: three DWORD copies into the EBP-24/20/1C
    // locals that 404E3F.. and 404EEB.. consume numerically. Field-exact reads.
    CameraVector out;
    out.x = file.field<float>(at).value(site);
    out.y = file.field<float>(at + 4).value(site);
    out.z = file.field<float>(at + 8).value(site);
    // 404E45/404EF1 MOV ECX,[ECX+C] then 404E50/404EFC MOV [EBP-18],ECX. The
    // pinned body 404450 never reads EBP-18 on this mode6 path before its
    // RET at404FAC (the next read, 4050D4, belongs to the mode7 body entered
    // at404FAD). The reached access keeps its extent/generation check.
    const auto w = file.load_transport_word(at + 12, site);
    if (w.is_known())
        out.w = std::bit_cast<float>(w.numeric(site));
    // NEW repair (PORT-FIX proposal): the forward W at +2C is stored by
    // 413935 from the unproduced 4138A0 stack word EBP-14 (413932), and the
    // far-detail up W at +3C by 41391A from EBP-4 (413917). Their bits are
    // original uninitialized stack. Only when the transported word is opaque
    // is this dead copy of EBP-18 held as +0.0; no caller reads the lane
    // (mounted() in source_camera_dispatch.cpp and camera_mounted_core use XYZ).
    else
        out.w = 0.0f;
    return out;
}
CameraVector mounted_axis(ModelResources &models, std::size_t handle, std::uint32_t site)
{
    if (models.source_backed(handle))
        return mounted_axis(models.source_at(handle), site);
    auto &model = models.at(handle);
    const auto &transform = model.part(model.source_lod_index(0), 0).transform;
    if (site == 0x404e1c)
        return {transform.up.x, transform.up.y, transform.up.z, std::bit_cast<float>(transform.up_w)};
    return {transform.forward.x, transform.forward.y, transform.forward.z, std::bit_cast<float>(transform.forward_w)};
}
} // namespace detail
namespace
{
void no_orbit_publication(void *, CameraVector) {}
void audit_axis(CameraMountedAudit *audit, std::uint32_t site)
{
    if (audit && audit->before_axis)
        audit->before_axis(audit->context, site);
}
} // namespace
void camera_mounted_tick(CameraState &camera, CameraVehicle &vehicle, const CameraMountedModelAxes &axes)
{
    detail::MountedCameraProvider provider{const_cast<CameraMountedModelAxes *>(&axes), no_orbit_publication,
                                           [](void *p, std::uint32_t site) {
                                               const auto &a = *static_cast<const CameraMountedModelAxes *>(p);
                                               return site == 0x404e1c ? a.up : a.forward;
                                           }};
    detail::camera_mounted_core(camera, vehicle, provider);
}
void camera_mounted_tick(CameraState &camera, CameraVehicle &vehicle, SourcePmdModelView model,
                         CameraMountedAudit *audit)
{
    struct Context
    {
        SourcePmdModelView model;
        CameraMountedAudit *audit;
    } context{model, audit};
    const detail::MountedCameraProvider provider{&context, no_orbit_publication, [](void *p, std::uint32_t site) {
                                                     auto &c = *static_cast<Context *>(p);
                                                     audit_axis(c.audit, site);
                                                     return detail::mounted_axis(c.model, site);
                                                 }};
    detail::camera_mounted_core(camera, vehicle, provider);
}
void camera_mounted_tick(CameraState &camera, CameraVehicle &vehicle, ModelResources &models, std::size_t handle,
                         CameraMountedAudit *audit)
{
    struct Context
    {
        ModelResources &models;
        std::size_t handle;
        CameraMountedAudit *audit;
    } context{models, handle, audit};
    const detail::MountedCameraProvider provider{&context, no_orbit_publication, [](void *p, std::uint32_t site) {
                                                     auto &c = *static_cast<Context *>(p);
                                                     audit_axis(c.audit, site);
                                                     return detail::mounted_axis(c.models, c.handle, site);
                                                 }};
    detail::camera_mounted_core(camera, vehicle, provider);
}
} // namespace sgn::retail
