#include "sgn/source_vehicle_bootstrap.hpp"
#include "sgn/source_race_config_receivers.hpp"
#include "sgn/source_initial_grid.hpp"
#include "sgn/source_trial_start.hpp"
#include "sgn/source_world_strip.hpp"
#include "sgn/source_race_lifecycle.hpp"
#include "sgn/retail_frontend_state.hpp"
#include "sgn/race_session.hpp"
#include "sgn/bit_reinterpret.hpp"
#include <bit>
#include <limits>
extern "C" void sgn_trial_normalize(float *);
extern "C" void sgn_bootstrap_maximum_z(float *, const float *);
extern "C" void sgn_bootstrap_negate_x(float *);
extern "C" void sgn_bootstrap_extent(const float *, float *);
namespace sgn::retail
{
namespace
{
using W = std::uint32_t;
using sgn::bit_reinterpret::as_float;
float f32(double value)
{
    return static_cast<float>(value);
}
[[noreturn]] void gap(W site, const char *reason)
{
    throw SourceRaceLifecycleBoundary(site, 0, reason);
}
// These are reads of the live normalized PMD allocation, not a model/geometry
// snapshot. Only the reached scalar is checked; count does not preflight parts.
struct ModelMemory
{
    SourcePmdByteView file, descriptor;
    explicit ModelMemory(const SourcePmdModelView &m) : file(m.bytes()), descriptor(m.descriptor()) {}
    W descriptor_word(W offset) const
    {
        return descriptor.field<W>(offset).value();
    }
    W address(W base, W delta, W site) const
    {
        const auto sum = std::uint64_t(base) + delta;
        if (sum > std::numeric_limits<W>::max())
            gap(site, "normalized PMD pointer leaves DWORD allocation extent");
        return static_cast<W>(sum);
    }
    template <class T> T read(W base, W delta, W site) const
    {
        if (base == 0)
            gap(site, "reached NULL normalized bootstrap PMD pointer");
        try
        {
            return file.field<T>(address(base, delta, site)).value();
        }
        catch (const SourceAllocationError &)
        {
            throw;
        }
        catch (const FormatError &)
        {
            gap(site, "reached bootstrap PMD scalar outside actual allocation");
        }
    }
    W head(W slot, W site) const
    {
        if (slot >= 4)
            gap(site, "bootstrap PMD LOD slot outside actual descriptor48..54");
        return descriptor_word(0x48u + slot * 4u);
    }
    void vector(W pointer, W offset, VehicleVector &target, W site, bool paired_tail = false) const
    {
        // Source MOVs preserve all four words and publish each reached lane.
        target.x = as_float(read<W>(pointer, offset, site));
        target.y = as_float(read<W>(pointer, offset + 4u, site));
        const auto z = read<W>(pointer, offset + 8u, site);
        if (paired_tail)
        {
            const auto w = read<W>(pointer, offset + 12u, site);
            target.z = as_float(z);
            target.w = as_float(w);
        }
        else
        {
            target.z = as_float(z);
            target.w = as_float(read<W>(pointer, offset + 12u, site));
        }
    }
};
class Bootstrap final
{
    FrontendState &frontend_;
    std::shared_ptr<RaceSessionPlatform> platform_;
    RaceSessionState &w_;
    W current(W site) const
    {
        if (w_.current_slot >= 6)
            gap(site, "bootstrap current4D5BB8 leaves six source vehicles");
        return w_.current_slot;
    }
    W selected_template(W site) const
    {
        if (w_.load_selection.template_vehicle >= 6)
            gap(site, "bootstrap template474630 leaves six source vehicles");
        return w_.load_selection.template_vehicle;
    }
    W controller(W site) const
    {
        if (w_.controls.current >= 4)
            gap(site, "bootstrap controller4722D0 leaves four source controllers");
        return w_.controls.current;
    }
    ModelMemory model(W slot, W site) const
    {
        const auto handle = w_.vehicles.models[slot].model;
        if (!handle)
            gap(site, "active source bootstrap vehicle has NULL model26B0");
        auto &models = platform_->source_scene_resources().models();
        // No eager validation of all cars or fallback to decoded Models::at.
        if (!models.source_backed(*handle))
            gap(site, "bootstrap model is not owned by actual source PMD bank");
        return ModelMemory(models.source_at(*handle));
    }
    void contact_initial()
    { // 4085E0 captures current once; tail4082D0 rereads it.
        auto &c = w_.vehicles.contact[current(0x4085e0)];
        c.body.position.x = 0;
        c.body.position.y = as_float(0x447a0000);
        c.body.position.z = as_float(0xc61c4000);
        c.body.mass = as_float(0x447a0000);
        initialize_model_matrix_identity(c.body.orientation);
        initialize_model_matrix_identity(c.body.incremental);
        initialize_model_matrix_identity(c.matrix_7338);
        c.body.velocity.x = 0;
        c.body.velocity.y = 0;
        c.body.velocity.z = 0;
        const auto id = current(0x4082d0);
        auto &reached = w_.vehicles.contact[id];
        contact::response::initialize_rig(w_.cars[id].model, reached.body.position, reached.rig);
    }
    void camera_template()
    { // 408710, separate captured contact and later template rereads.
        const auto camera_id = controller(0x408716);
        auto &contact = w_.vehicles.contact[selected_template(0x40871c)];
        const auto &forward = w_.camera_presentation[camera_id].forward_axis;
        VehicleVector direction;
        direction.x = forward.x;
        direction.y = forward.y;
        const float old_y = direction.y;
        direction.z = forward.z;
        direction.w = forward.w;
        direction.x = f32(double(direction.x) - 0.0);
        direction.y = f32(double(direction.y) - old_y);
        direction.z = f32(double(direction.z) - 0.0);
        sgn_trial_normalize(&direction.x); // 44ED40, same source x87 body.
        const double x = double(direction.x) * 2000.0;
        const double y = double(direction.y) * 2000.0;
        const auto position_id = controller(0x408791);
        direction.y = f32(y);
        direction.z = f32(double(direction.z) * 2000.0);
        direction.x = f32(x);
        direction.y = f32(double(direction.y) + 500.0);
        const auto &position = w_.controls.controllers[position_id].camera.position;
        contact.body.position.x = f32(double(direction.x) + position.x);
        contact.body.position.y = f32(double(direction.y) + position.y);
        contact.body.position.z = f32(double(direction.z) + position.z);
        for (W i = 0; i < contact.rig.count; ++i)
        {
            if (i >= contact.rig.probes.size())
                gap(0x4087e6, "bootstrap contact probe pointer outside owned six probes");
            auto &probe = contact.rig.probes[i];
            probe.current.x = f32(double(probe.local.x) + contact.body.position.x);
            probe.current.y = f32(double(probe.local.y) + contact.body.position.y);
            probe.current.z = f32(double(probe.local.z) + contact.body.position.z);
        }
        initialize_model_matrix_identity(contact.body.orientation);
        initialize_model_matrix_identity(contact.body.incremental);
        initialize_model_matrix_identity(contact.matrix_7338);
        const auto later_camera = controller(0x40882a), later_template = selected_template(0x408830);
        contact.body.velocity.x = 0;
        contact.body.velocity.y = 0;
        contact.body.velocity.z = 0;
        auto &vehicle = w_.vehicles.physics[later_template];
        const auto &later_position = w_.controls.controllers[later_camera].camera.position;
        vehicle.position.x = f32(double(direction.x) + later_position.x);
        vehicle.position.y = f32(double(direction.y) + later_position.y);
        vehicle.position.z = f32(double(direction.z) + later_position.z);
        initialize_model_matrix_identity(vehicle.orientation);
        initialize_model_matrix_identity(w_.vehicles.physics[selected_template(0x40886d)].incremental);
        initialize_model_matrix_identity(w_.vehicles.physics[selected_template(0x40887d)].secondary_rotation);
        auto &velocity = w_.vehicles.physics[selected_template(0x40888e)].velocity;
        velocity.x = 0;
        velocity.y = 0;
        velocity.z = 0; // All separate W/probe previous lanes survive.
    }
    void extent_anchors()
    { // 417E10: selected descriptor5C, low16 vertex count, C0 choice.
        for (W id = 0; id < 6; ++id)
        {
            if (w_.vehicles.race[id].active != 1)
                continue;
            const auto m = model(id, 0x417e36);
            if (m.descriptor_word(0x24) <= 11)
            {
                w_.vehicles.presentation[id].field_3134 = 0;
                continue;
            }
            const W selected = m.descriptor_word(0x5c), head = m.head(selected, 0x417e54);
            const W count = m.read<std::uint16_t>(head, 0x2e8, 0x417e5a);
            const W vertices = m.read<W>(head, 0x2f0, 0x417e61);
            float maximum = 0;
            for (W i = 0; i < count; ++i)
            {
                const float z = as_float(m.read<W>(vertices, i * 16u + 8u, 0x417e70));
                // FCOM/FNSTSW TEST AH,1 also selects on unordered comparison.
                sgn_bootstrap_maximum_z(&maximum, &z);
            }
            const W transform = m.read<W>(head, 0x2c8, 0x417e83);
            auto &a = w_.vehicles.anchors[id];
            m.vector(transform, 0x20, a.forward, 0x417e94);
            const W px = m.read<W>(transform, 0, 0x417eab);
            sgn_bootstrap_negate_x(&a.forward.x); // 417EAD..B1 FCHS/FSTP.
            a.position.x = as_float(px);
            a.position.y = as_float(m.read<W>(transform, 4, 0x417ec1));
            const W pz = m.read<W>(transform, 8, 0x417ec7), pw = m.read<W>(transform, 12, 0x417eca);
            a.position.z = as_float(pz);
            a.position.w = as_float(pw);
            sgn_bootstrap_extent(&maximum, &a.extent);
            w_.vehicles.presentation[id].field_3134 = 1;
        }
        w_.current_slot = 0;
    }
    void effect_anchors()
    { // 4162C0, part20 flag is read BEFORE second count comparison.
        for (W id = 0; id < 6; ++id)
        {
            if (w_.vehicles.race[id].active != 1)
                continue;
            auto &a = w_.vehicles.anchors[id];
            a.effect_available[0] = 0;
            a.effect_available[1] = 0;
            const auto m = model(id, 0x4162fa);
            if (m.descriptor_word(0x24) <= 19)
                continue;
            const W selected = m.descriptor_word(0x5c);
            a.effect_available[0] = 1;
            const W head = m.head(selected, 0x416309);
            const W first = m.read<W>(head, 0x4c8, 0x416310);
            m.vector(first, 0, a.effect_position[0], 0x41631b);
            m.vector(first, 16, a.effect_base_scale[0], 0x416331, true);
            if (m.read<std::uint16_t>(head, 0x500, 0x416350) != 1)
                continue;
            if (model(id, 0x41635f).descriptor_word(0x24) <= 20)
                continue;
            const W second = m.read<W>(head, 0x508, 0x416365);
            a.effect_available[1] = 1;
            m.vector(second, 0, a.effect_position[1], 0x41637c);
            m.vector(second, 16, a.effect_base_scale[1], 0x416392, true);
        }
        w_.current_slot = 0;
    }
    void pair_a_anchors()
    { // 414F00 uses literal raw slot0, including source threshold23.
        for (W id = 0; id < 6; ++id)
        {
            if (w_.vehicles.race[id].active != 1)
                continue;
            const auto m = model(id, 0x414f17);
            if (m.descriptor_word(0x24) < 23)
                continue;
            const W head = m.head(0, 0x414f1d), first = m.read<W>(head, 0x5c8, 0x414f20);
            m.vector(first, 0, w_.vehicles.trail_inputs[id].pair_a[0], 0x414f26);
            const auto next = model(id, 0x414f4f);
            const W second_head = next.head(0, 0x414f51);
            const W second = next.read<W>(second_head, 0x608, 0x414f54);
            next.vector(second, 0, w_.vehicles.trail_inputs[id].pair_a[1], 0x414f5a);
        }
        w_.current_slot = 0;
    }
    void pair_b_anchors()
    { // 4150D0 clears flags even for inactive vehicles.
        for (W id = 0; id < 6; ++id)
        {
            auto &t = w_.vehicles.trail_inputs[id];
            t.pair_b_available[0] = 0;
            t.pair_b_available[1] = 0;
            if (w_.vehicles.race[id].active != 1)
                continue;
            const auto m = model(id, 0x415103);
            if (m.descriptor_word(0x24) < 21)
                continue;
            const W head = m.head(0, 0x415109);
            t.pair_b_available[0] = 1;
            const W first = m.read<W>(head, 0x548, 0x41511e);
            m.vector(first, 0, t.pair_b[0], 0x415121);
            const auto next = model(id, 0x415139);
            if (next.descriptor_word(0x24) < 22)
                continue;
            const W next_head = next.head(0, 0x41513f);
            t.pair_b_available[1] = 1;
            const W second = next.read<W>(next_head, 0x588, 0x415154);
            next.vector(second, 0, t.pair_b[1], 0x415157);
        }
        w_.current_slot = 0;
    }
    static std::optional<std::size_t> model_identity(W source_word)
    {
        // Native token1 is handle0. Assignment does not dereference/resolve the
        // source pointer: invalid identity fails only at a later actual consumer.
        if (source_word == 0)
            return {};
        return std::size_t(source_word - 1u);
    }

  public:
    Bootstrap(FrontendState &f, std::shared_ptr<RaceSessionPlatform> p)
        : frontend_(f), platform_(std::move(p)), w_(platform_->state())
    {
    }
    void run(SourceBootFilePlatform &files, NativeSourceMenuCrt &crt)
    {
        initialize_source_vehicle_config_prefix_413676(frontend_, files, crt);
        const auto id = current(0x41368b);
        auto &vehicle = w_.vehicles.physics[id];
        auto &parameters = w_.vehicles.parameters[id];
        vehicle.position.x = 0;
        vehicle.position.y = 0;
        vehicle.position.z = as_float(0xc63ea000);
        parameters.tyres[0].radius = as_float(0x420a0000);
        parameters.tyres[1].radius = as_float(0x420a0000);
        parameters.tyres[2].radius = as_float(0x41c40000);
        parameters.tyres[3].radius = as_float(0x41c40000);
        parameters.wheelbase = as_float(0x437a0000);
        parameters.track_front = as_float(0x42fd3334);
        parameters.track_rear = as_float(0x43340000);
        parameters.mass = as_float(0x447a0000);
        parameters.com_a = as_float(0x3f000000);
        parameters.com_b = as_float(0x3f000000);
        parameters.com_height = as_float(0x41f00001);
        const auto reset = current(0x41d300);
        reset_race_record(w_.vehicles.race[reset], w_.vehicles.physics[reset]);
        clear_wheel_acceleration(w_.vehicles.physics[current(0x413450)]);
        contact_initial();
        camera_template();
        initialize_source_vehicle_strip_41d180(platform_);
        for (W slot = 1; slot < 6; ++slot)
        {
            clone_source_template_vehicle_413744(w_, slot);
            auto &position = w_.vehicles.physics[slot].position;
            position.x = f32(double(position.x) + 500.0);
            place_source_vehicle_408640(frontend_, slot, position);
            const W model_word = w_.cars[slot].source_word00, shadow_word = w_.cars[slot].source_word04;
            w_.vehicles.models[slot].model = model_identity(model_word);
            w_.vehicles.models[slot].shadow = model_identity(shadow_word);
        }
        for (W slot = 0; slot < 6; ++slot)
            if (w_.cars[slot].source_word00 == 0)
                w_.vehicles.race[slot].active = 0;
        extent_anchors();
        effect_anchors();
        pair_a_anchors();
        pair_b_anchors();
        // 419900 is exactly RET in pinned PE, with no stores/children.
        initialize_source_grid_4130d0(frontend_, 1); // Only at original4137A3.
    }
};
} // namespace
void initialize_source_vehicles_413670(FrontendState &frontend, SourceBootFilePlatform &files, NativeSourceMenuCrt &crt)
{
    const auto p = frontend.shared_platform();
    p->require_owner_thread();
    if (!p->source_construction_only_entry() || &frontend.world() != &p->state())
        throw FormatError("413670 requires the same source Platform and world");
    Bootstrap(frontend, p).run(files, crt);
}
} // namespace sgn::retail
