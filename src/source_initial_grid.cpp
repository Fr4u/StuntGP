#include "sgn/source_initial_grid.hpp"
#include "sgn/source_application_literals.hpp"
#include "sgn/source_trial_start.hpp"
#include "sgn/source_race_lifecycle.hpp"
#include "sgn/retail_frontend_state.hpp"
#include "sgn/race_session.hpp"
#include "sgn/contact_response.hpp"
#include "sgn/bit_reinterpret.hpp"
#include <bit>
extern "C" void sgn_trial_normalize(float *);
namespace sgn::retail
{
namespace
{
using W = std::uint32_t;
float f32(double x)
{
    return static_cast<float>(x);
}
using sgn::bit_reinterpret::as_float;
[[noreturn]] void gap(W site, const char *reason)
{
    throw SourceRaceLifecycleBoundary(site, 0, reason);
}
void normalize(VehicleVector &v)
{
    sgn_trial_normalize(&v.x);
}
void zero_xyz(Vec3 &v)
{
    v.x = 0;
    v.y = 0;
    v.z = 0;
}
class InitialGrid final
{
    FrontendState &frontend_;
    std::weak_ptr<RaceSessionPlatform> borrow_;
    RaceSessionPlatform &platform_;
    RaceSessionState &w_;
    W vehicle(W site) const
    {
        if (w_.current_slot >= 6)
            gap(site, "initial grid current pointer outside actual six-slot bank");
        return w_.current_slot;
    }
    static float point(const SourceCameraSplineRef &p, W i, W lane, W site)
    {
        return as_float(p.read_word(std::uint64_t(i) * 16u + lane, site));
    }
    float offset(W address, W site) const
    {
        return as_float(platform_.source_application_literals().read_initial_grid_offset_word(address, site));
    }
    SourceCameraSplineRef start(W site) const
    {
        const auto *p = w_.grid.start_spline.source();
        if (!p)
            gap(site, "reached571328 dereference has no source PMD identity");
        return *p;
    }
    void basis(W id, const VehicleVector &right, const VehicleVector &up, const VehicleVector &forward)
    {
        auto &s = w_.vehicles.physics[id];
        // Source copies uninitialized stack W at six sites. Preserve only native opaque
        // lanes; do not claim reconstruction of unknown guest stack bytes.
        const float rw = s.orientation[3], uw = s.orientation[7], fw = s.orientation[11];
        s.orientation = vehicle_identity();
        s.orientation[0] = right.x;
        s.orientation[1] = right.y;
        s.orientation[2] = right.z;
        s.orientation[3] = rw;
        s.orientation[4] = up.x;
        s.orientation[5] = up.y;
        s.orientation[6] = up.z;
        s.orientation[7] = uw;
        s.orientation[8] = forward.x;
        s.orientation[9] = forward.y;
        s.orientation[10] = forward.z;
        s.orientation[11] = fw;
        w_.vehicles.contact[id].body.orientation = s.orientation;
    }
    void rolling()
    { // 412E80 does a fresh id2 lookup; it never overwrites571328.
        const auto route = find_source_camera_spline_428800(borrow_, 2);
        VehicleVector up{0, 1, 0, 0};
        W percent = 87;
        w_.current_slot = 0;
        for (W row = 0; row < 6; ++row)
        {
            if (!route)
                gap(0x412ec3, "412E80 reached null id2 spline at count read");
            const W count = route->read_word(8, 0x412ec3);
            const W index = (count * percent) / 100u; // Low IMUL DWORD, then unsigned magic division.
            const auto points = route->at_normalized_offset(route->read_word(4, 0x412ed0));
            percent -= 2;
            auto id = vehicle(0x412ee8);
            if (w_.vehicles.race[id].active == 1)
            {
                VehicleVector forward;
                const auto nx = point(points, index + 1u, 0, 0x412ef5), cx = point(points, index, 0, 0x412ef8);
                forward.x = f32(double(nx) - cx);
                const auto ny = point(points, index + 1u, 4, 0x412f00), cy = point(points, index, 4, 0x412f03);
                forward.y = f32(double(ny) - cy);
                const auto nz = point(points, index + 1u, 8, 0x412f09), cz = point(points, index, 8, 0x412f0c);
                forward.z = f32(double(nz) - cz);
                normalize(forward);
                reset_source_vehicle_motion_413490(frontend_, vehicle(0x412f17));
                restart_source_vehicle_413530(frontend_, vehicle(0x412f22));
                id = vehicle(0x412f2d);
                auto &s = w_.vehicles.physics[id];
                zero_xyz(s.velocity);
                zero_xyz(s.acceleration);
                const VehicleVector right{f32(0.0 - double(forward.z)), 0, f32(double(forward.x) - 0.0), 0};
                basis(id, right, up, forward); // No right/up normalization in this branch.
                const auto &m = s.orientation;
                const W table = 0x4744f4u + row * 16u;
                VehicleVector position;
                // 412FF6 starts with Z*forward, unlike412DB1. Re-read every source operand.
                const double x0 = double(offset(table + 4, 0x412ff6)) * m[8];
                const double x1 = double(offset(table, 0x412fff)) * m[4];
                position.x = f32((x0 + x1) + double(m[0]) * offset(table - 4, 0x41300b));
                const double y0 = double(offset(table, 0x413013)) * m[5];
                const double y1 = double(offset(table + 4, 0x41301b)) * m[9];
                position.y = f32((y0 + y1) + double(m[1]) * offset(table - 4, 0x41302f));
                const double z0 = double(offset(table, 0x413037)) * m[6];
                const double z1 = double(offset(table + 4, 0x41303f)) * m[10];
                position.z = f32((z0 + z1) + double(m[2]) * offset(table - 4, 0x413050));
                position.x = f32(double(position.x) + point(points, index, 0, 0x41305d));
                position.y = f32(double(position.y) + point(points, index, 4, 0x413065));
                position.z = f32(double(position.z) + point(points, index, 8, 0x41306e));
                position.y = f32(double(position.y) + 150.0);
                position.w = s.position.w;
                place_source_vehicle_408640(frontend_, id, position);
            }
            ++w_.current_slot; // Fresh global after all synchronous source children.
        }
        w_.current_slot = 0;
    }

  public:
    InitialGrid(FrontendState &f, const std::shared_ptr<RaceSessionPlatform> &p)
        : frontend_(f), borrow_(p), platform_(*p), w_(p->state())
    {
    }
    void standard()
    { // 412BF0 has its own source store even when called directly.
        VehicleVector up{0, 1, 0, 0};
        if (!w_.grid.start_spline)
            w_.grid.start_spline.bind_source(find_source_camera_spline_428800(borrow_, 1));
        const auto route = start(0x412c28);
        const auto points = route.at_normalized_offset(route.read_word(4, 0x412c28));
        const W last = route.read_word(8, 0x412c2b) - 1u;
        VehicleVector forward;
        const auto fx = point(points, 0, 0, 0x412c32), lx = point(points, last, 0, 0x412c39);
        forward.x = f32(double(fx) - lx);
        const auto fy = point(points, 0, 4, 0x412c3e), ly = point(points, last, 4, 0x412c41);
        forward.y = f32(double(fy) - ly);
        const auto fz = point(points, 0, 8, 0x412c47), lz = point(points, last, 8, 0x412c4a);
        forward.z = f32(double(fz) - lz);
        normalize(forward);
        w_.current_slot = 0;
        for (W row = 0; row < 6; ++row)
        {
            auto id = vehicle(0x412c6b);
            if (w_.vehicles.race[id].active == 1)
            {
                reset_source_vehicle_motion_413490(frontend_, id);
                restart_source_vehicle_413530(frontend_, vehicle(0x412c7d));
                id = vehicle(0x412c88);
                auto &s = w_.vehicles.physics[id];
                zero_xyz(s.velocity);
                zero_xyz(s.acceleration);
                VehicleVector right;
                right.x = f32(double(forward.y) * up.z - double(forward.z) * up.y);
                right.y = f32(double(forward.z) * up.x - double(forward.x) * up.z);
                right.z = f32(double(forward.x) * up.y - double(forward.y) * up.x);
                normalize(right);
                normalize(up);
                normalize(forward); // Each active slot, in this order.
                basis(vehicle(0x412d0c), right, up, forward);
                w_.vehicles.race[id].place = row;
                VehicleVector position;
                if (w_.level.game_type == 3)
                {
                    position.x = point(points, 0, 0, 0x412d98);
                    position.y = point(points, 0, 4, 0x412d9d);
                    position.z = point(points, 0, 8, 0x412da3);
                    position.w = point(points, 0, 12, 0x412da9);
                }
                else
                {
                    const W table = 0x4743f4u + row * 16u;
                    const auto &m = s.orientation;
                    const double x0 = double(offset(table, 0x412db1)) * m[4];
                    const double x1 = double(offset(table + 4, 0x412db9)) * m[8];
                    position.x = f32((x0 + x1) + double(offset(table - 4, 0x412dc7)) * m[0]);
                    const double y0 = double(offset(table, 0x412dd1)) * m[5];
                    const double y1 = double(offset(table + 4, 0x412dd9)) * m[9];
                    position.y = f32((y0 + y1) + double(m[1]) * offset(table - 4, 0x412dea));
                    const double z0 = double(offset(table, 0x412df2)) * m[6];
                    const double z1 = double(offset(table + 4, 0x412dfa)) * m[10];
                    position.z = f32((z0 + z1) + double(m[2]) * offset(table - 4, 0x412e0b));
                    position.x = f32(double(position.x) + point(points, 0, 0, 0x412e16));
                    position.y = f32(double(position.y) + point(points, 0, 4, 0x412e1e));
                    position.z = f32(double(position.z) + point(points, 0, 8, 0x412e27));
                    position.w = s.position.w;
                }
                place_source_vehicle_408640(frontend_, id, position);
            }
            ++w_.current_slot;
        }
        w_.current_slot = 0;
    }
    void run(W selector)
    { // Captured ECX survives lookup even for selectors other than1/2.
        if (!w_.grid.start_spline)
        {
            w_.grid.start_spline.bind_source(find_source_camera_spline_428800(borrow_, 1));
            if (!w_.grid.start_spline)
                return; // 4130ED source normal-return null path.
        }
        if (selector == 1)
            standard();
        else if (selector == 2)
            rolling();
    }
    void pair()
    {
        run(w_.clock.race_mode);
        const auto saved = w_.current_slot;
        w_.current_slot = 0;
        for (W count = 0; count < 6; ++count)
        {
            const auto id = vehicle(0x4082d0);
            auto &c = w_.vehicles.contact[id];
            contact::response::initialize_rig(w_.cars[id].model, c.body.position, c.rig);
            ++w_.current_slot;
        }
        const auto selector = w_.clock.race_mode;
        w_.current_slot = saved;
        run(selector);
    }
};
std::shared_ptr<RaceSessionPlatform> source_platform(FrontendState &f)
{
    const auto p = f.shared_platform();
    p->require_owner_thread();
    if (!p->source_construction_only_entry() || &f.world() != &p->state())
        throw FormatError("initial grid requires same source Platform");
    return p;
}
} // namespace
void initialize_source_grid_4130d0(FrontendState &f, W selector)
{
    const auto p = source_platform(f);
    InitialGrid(f, p).run(selector);
}
void initialize_source_standard_grid_412bf0(FrontendState &f)
{
    const auto p = source_platform(f);
    InitialGrid(f, p).standard();
}
void initialize_source_race_grid_pair_40d549(FrontendState &f)
{
    const auto p = source_platform(f);
    InitialGrid(f, p).pair();
}
} // namespace sgn::retail
