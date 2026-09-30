#include "sgn/source_vehicle_draw_graph.hpp"
#include "sgn/race_session.hpp"
#include "sgn/bit_reinterpret.hpp"
#include <bit>
#include <cstddef>
#include <cstring>
#include <limits>

extern "C" std::uint32_t sgn_source_vehicle_detail_42b580(const void *, const void *, const void *, const void *,
                                                          const std::uint32_t *) noexcept;
extern "C" void sgn_source_vehicle_shadow_pose_41c44c(void *, const void *const *) noexcept;

namespace sgn::retail
{
namespace
{
using W = std::uint32_t;
using H = std::uint16_t;
using B = std::uint8_t;
static_assert(sizeof(void *) == 8 && sizeof(ModelTransform) == 80 && sizeof(contact::Vector4) == 16);
static_assert(offsetof(ModelTransform, scale) == 16 && offsetof(ModelTransform, forward) == 32 &&
              offsetof(ModelTransform, up) == 48);
using sgn::bit_reinterpret::as_word;
void admit(RaceSessionPlatform &p, W site)
{
    p.require_owner_thread();
    if (!p.source_construction_only_entry())
        throw SourceAllocationError(site, "vehicle draw graph requires actual source platform");
}
unsigned current(RaceSessionState &w, W site)
{
    const auto slot = w.current_slot;
    if (slot >= VehicleStorage::physical_slots)
        throw SourceAllocationError(site, "reached draw vehicle outside six actual owners");
    return slot;
}
SourcePmdModelView model(RaceSessionPlatform &p, std::optional<std::size_t> handle, W site)
{
    if (!handle)
        throw SourceAllocationError(site, "reached null vehicle draw model");
    return p.source_scene_resources().models().source_at(*handle);
}
W offset(W pointer, W delta, W site)
{
    if (pointer == 0)
        throw SourceAllocationError(site, "reached null source PMD pointer");
    if (delta > std::numeric_limits<W>::max() - pointer)
        throw SourceAllocationError(site, "reached PMD pointer leaves DWORD domain");
    return pointer + delta;
}
template <class T> T read(SourcePmdModelView m, W pointer, W delta, W site)
{
    return m.bytes().field<T>(offset(pointer, delta, site)).value(site);
}
template <class T> void store(SourcePmdModelView m, W pointer, W delta, T value, W site)
{
    m.bytes().field<T>(offset(pointer, delta, site)).set(value);
}
W selected_pointer(SourcePmdByteView descriptor, W selector, W site)
{
    if (selector >= 4)
        throw SourceAllocationError(site, "reached vehicle draw selector outside four PMD slots");
    return descriptor.field<W>(0x48 + selector * 4u).value();
}
W detail_42b580(RaceSessionPlatform &p, const VehicleVector &position)
{
    auto &w = p.state();
    const auto slot = w.controls.current; // 4722D0 captured independently for each call
    if (slot >= w.controls.controllers.size() || slot >= w.camera_presentation.size())
        throw SourceAllocationError(0x42b587, "reached current camera outside actual owners");
    const auto &c = w.controls.controllers[slot].camera;
    const auto &forward = w.camera_presentation[slot].forward_axis;
    return sgn_source_vehicle_detail_42b580(&position, &c.position, &forward, &c.lens,
                                            &w.impact_particles.rate_selector);
}
void effects_4166f0(RaceSessionPlatform &p)
{
    auto &w = p.state();
    const auto slot = current(w, 0x4166f7);
    const auto handle = w.vehicles.models[slot].model;
    const W enabled = w.vehicles.model_effects[slot].enabled;
    const auto m = model(p, handle, 0x416703);
    const auto descriptor = m.descriptor();
    const W selector = descriptor.field<W>(0x5c).value();
    const W parts = selected_pointer(descriptor, selector, 0x416706);
    const auto &availability = w.vehicles.anchors[slot].effect_available;
    const auto &scales = w.vehicles.model_effects[slot].current_scale;
    if (enabled == 0)
    {
        if (availability[0] == 1)
            store<H>(m, parts, 0x4c0, 0, 0x41671d);
        if (availability[1] == 1)
            store<H>(m, parts, 0x500, 0, 0x41672d);
        return;
    }
    if (availability[0] != 1)
        return;
    const W first = read<W>(m, parts, 0x4c8, 0x41673e);
    const W first_x = as_word(scales[0].x);
    store<H>(m, parts, 0x4c0, 1, 0x41674a);
    store<W>(m, first, 0x10, first_x, 0x416751);
    store<W>(m, first, 0x14, as_word(scales[0].y), 0x41675a);
    store<W>(m, first, 0x18, as_word(scales[0].z), 0x416763);
    if (availability[1] != 1)
        return;
    const W second_x = as_word(scales[1].x);
    store<H>(m, parts, 0x500, 1, 0x416776);
    const W second = read<W>(m, parts, 0x508, 0x41677d);
    store<W>(m, second, 0x10, second_x, 0x416783);
    const W second_y = as_word(scales[1].y), second_z = as_word(scales[1].z);
    store<W>(m, second, 0x14, second_y, 0x416792);
    store<W>(m, second, 0x18, second_z, 0x416795);
}
void near_parts_413da0(RaceSessionPlatform &p)
{
    auto &w = p.state();
    const auto slot = current(w, 0x413dac);
    const auto handle = w.vehicles.models[slot].model;
    auto &state = w.vehicles.presentation[slot];
    state.field_3134 = 1;
    const auto m = model(p, handle, 0x413db8);
    const auto d = m.descriptor();
    const W selector = d.field<W>(0x5c).value();
    const W parts = selected_pointer(d, selector, 0x413dbb);
    store<H>(m, parts, 0x2c0, 1, 0x413dbf);
    store<H>(m, parts, 0x300, 1, 0x413dc6);
    store<H>(m, parts, 0x340, 1, 0x413dcd);
    store<H>(m, parts, 0x380, 1, 0x413dd4);
    store<H>(m, parts, 0x3c0, 1, 0x413ddb);
    store<H>(m, parts, 0x400, 1, 0x413de2);
    state.field_3928 = state.field_3924;
    effects_4166f0(p);
    auto &after = w.vehicles.presentation[current(w, 0x413dff)];
    after.field_33c8 = 1;
    after.field_3508 = 1;
}
void distant_parts_413ce0(RaceSessionPlatform &p)
{
    auto &w = p.state();
    const auto slot = current(w, 0x413cea);
    const auto handle = w.vehicles.models[slot].model;
    auto &vehicle = w.vehicles.physics[slot];
    vehicle.brake_signal = 0;
    vehicle.wheels[0].braking_signal = 0;
    const auto m = model(p, handle, 0x413cfe);
    const auto d = m.descriptor();
    const W selector = d.field<W>(0x5c).value();
    vehicle.wheels[1].braking_signal = 0;
    vehicle.wheels[2].braking_signal = 0;
    const W parts = selected_pointer(d, selector, 0x413d0d);
    const W count = d.field<W>(0x24).value();
    vehicle.wheels[3].braking_signal = 0; // 413D17 after count read, before first flag
    store<H>(m, parts, 0x140, 0, 0x413d1d);
    store<H>(m, parts, 0x180, 0, 0x413d24);
    store<H>(m, parts, 0x1c0, 0, 0x413d2b);
    store<H>(m, parts, 0x200, 0, 0x413d32);
    store<H>(m, parts, 0x240, 0, 0x413d39);
    store<H>(m, parts, 0x2c0, 0, 0x413d40);
    store<H>(m, parts, 0x300, 0, 0x413d47);
    store<H>(m, parts, 0x340, 0, 0x413d4e);
    store<H>(m, parts, 0x380, 0, 0x413d55);
    store<H>(m, parts, 0x3c0, 0, 0x413d5c);
    store<H>(m, parts, 0x400, 0, 0x413d63);
    if (count > 19)
        store<H>(m, parts, 0x4c0, 0, 0x413d6c);
    if (count > 20)
        store<H>(m, parts, 0x500, 0, 0x413d78);
    w.vehicles.particles[slot].field_7628 = 0;
    auto &presentation = w.vehicles.presentation[slot];
    presentation.field_3928 = 0;
    presentation.field_33c8 = 0;
    presentation.field_3508 = 0;
}
void shadow_queue_42b7f0(RaceSessionPlatform &p, SourcePmdModelView shadow, std::size_t handle, ModelTransform &pose)
{
    auto &w = p.state();
    allocate_source_outer_model_matrices_44fdb0(pose, w.view_matrices);  // 42B7FC
    allocate_source_model_part_matrices_450150(shadow, w.view_matrices); // 42B803
    auto &queue = w.model_draws.background;
    const auto captured = queue.cursor; // 42B808
    pose.tail[0] = 0;                   // 42B815 before reached destination store
    if (!captured || *captured >= queue.records.size())
        throw SourceAllocationError(0x42b81f, "reached shadow queue command outside owner");
    queue.records[*captured].model_identity = p.source_scene_resources().models().identity(handle);
    std::memcpy(&queue.records[*captured].pose, &pose, sizeof(pose)); // 42B821, actual80-byte local
    // 41C448 passes literal0, so reached42B830 is precisely42B7E0.
    const auto flags = shadow.descriptor().field<H>(2);
    flags.set(static_cast<H>(flags.value() & 0xfff7u));
    if (!queue.cursor)
        throw SourceAllocationError(0x42b83f, "reached null shadow queue advance");
    queue.cursor = *queue.cursor + 1u; // 42B83F before terminator dereference
    if (*queue.cursor >= queue.records.size())
        throw SourceAllocationError(0x42b845, "reached shadow queue terminator outside owner");
    queue.records[*queue.cursor].model_identity = 0;
}
} // namespace
bool select_source_vehicle_draw_detail_413e10(RaceSessionPlatform &p)
{
    admit(p, 0x413e10);
    auto &w = p.state();
    const W previous = detail_42b580(p, w.vehicles.physics[current(w, 0x413e15)].previous_position); // 413E1B
    const auto second_slot = current(w, 0x413e26);
    const auto previous_handle = w.vehicles.models[second_slot].model;
    const auto &position = w.vehicles.physics[second_slot].position; // 413E2C captured before previous64 store
    model(p, previous_handle, 0x413e32).descriptor().field<W>(0x64).set(previous);
    const W selected = detail_42b580(p, position); // 413E35 recaptures actual current camera
    const auto final_slot = current(w, 0x413e43);
    const auto handle = w.vehicles.models[final_slot].model;
    model(p, handle, 0x413e49).descriptor().field<W>(0x5c).set(selected);
    if (selected >= 3)
    {
        distant_parts_413ce0(p);
        return false;
    }
    near_parts_413da0(p);
    return true; // completed4166F5 in this actual branch wrote integer bits1
}
void append_source_vehicle_shadow_41c420(RaceSessionPlatform &p)
{
    admit(p, 0x41c420);
    auto &w = p.state();
    const auto slot = current(w, 0x41c42b);
    if (w.vehicles.presentation[slot].field_3928 == 0)
        return; // no shadow/model/payload access on this source branch
    ModelTransform pose;
    std::memcpy(&pose, &w.model_draws.template_pose, sizeof(pose)); // 41C44A
    const auto &shadow = w.vehicles.shadows[slot];
    const std::array<const void *, 4> inputs{&shadow.position, &shadow.forward, &shadow.normal, &shadow.factor_392c};
    sgn_source_vehicle_shadow_pose_41c44c(&pose, inputs.data());
    const auto captured_shadow = w.vehicles.models[slot].shadow;
    const auto shadow_model = model(p, captured_shadow, 0x41c4e3);
    const W attributes = shadow_model.descriptor().field<W>(0x2c).value();
    const B old = read<B>(shadow_model, attributes, 3, 0x41c4e6);
    store<B>(shadow_model, attributes, 3, static_cast<B>((old & 0xf7u) | 0x70u), 0x41c4ef);
    shadow_queue_42b7f0(p, shadow_model, *captured_shadow, pose); // 41C4F7, actual background owner
    const auto after = current(w, 0x41c503);
    const auto body_handle = w.vehicles.models[after].model;
    const auto shadow_handle = w.vehicles.models[after].shadow;
    // Capture handles first, resolve only at the reached source accesses. Each
    // read precedes its write; shared descriptor identities preserve aliasing.
    const auto body = model(p, body_handle, 0x41c515);
    const auto body_descriptor = body.descriptor();
    const W x = body_descriptor.field<W>(0x10).value();
    const auto target = model(p, shadow_handle, 0x41c517).descriptor();
    target.field<W>(0x10).set(x);
    const W y = body_descriptor.field<W>(0x14).value();
    target.field<W>(0x14).set(y);
    const W z = body_descriptor.field<W>(0x18).value();
    target.field<W>(0x18).set(z);
    const W fourth = body_descriptor.field<W>(0x1c).value();
    target.field<W>(0x1c).set(fourth);
}
void SourceVehicleDrawKnownPrefix::before_detail()
{
    if (active_)
        throw SourceAllocationError(0x4181b3, "nested vehicle draw provenance scope");
    platform_ = nullptr;
    pending_ = false;
    near_ = false;
    consumed_ = false;
    glow_ = false;
    transport_reads_ = 0;
}
void SourceVehicleDrawKnownPrefix::after_detail(RaceSessionPlatform &p, bool completed_near)
{
    p.require_owner_thread();
    platform_ = &p;
    slot_ = p.state().current_slot;
    pending_ = true;
    near_ = completed_near;
}
void SourceVehicleDrawKnownPrefix::enter_presentation(RaceSessionPlatform &p)
{
    p.require_owner_thread();
    if (active_)
        throw SourceAllocationError(0x4181be, "nested vehicle presentation provenance scope");
    // No established adjacent source call pair means the original fallback stays required.
    active_ = pending_ && platform_ == &p && slot_ == p.state().current_slot;
    if (!active_)
    {
        platform_ = nullptr;
        pending_ = false;
        near_ = false;
    }
}
void SourceVehicleDrawKnownPrefix::leave_presentation() noexcept
{
    platform_ = nullptr;
    pending_ = false;
    near_ = false;
    active_ = false;
    consumed_ = false;
    glow_ = false;
    transport_reads_ = 0;
}
void SourceVehicleDrawKnownPrefix::enter_glow(RaceSessionPlatform &p)
{
    before_detail();
    p.require_owner_thread();
    platform_ = &p;
    slot_ = p.state().current_slot;
    active_ = true;
    glow_ = true;
}
SourceTransportWord SourceVehicleDrawKnownPrefix::transport(W function, W instruction, std::int32_t offset)
{
    if (!active_)
        return remaining_.transport(function, instruction, offset);
    platform_->require_owner_thread();
    if (platform_->state().current_slot != slot_)
        throw SourceAllocationError(instruction, "vehicle changed in source word transport scope");
    if (!glow_)
    {
        constexpr std::array<W, 3> functions{0x4138a0, 0x4138a0, 0x4139f0};
        constexpr std::array<W, 3> instructions{0x413917, 0x413932, 0x413ae5};
        constexpr std::array<std::int32_t, 3> offsets{-4, -0x14, -0x24};
        if (transport_reads_ < 3 && function == functions[transport_reads_] &&
            instruction == instructions[transport_reads_] && offset == offsets[transport_reads_])
        {
            ++transport_reads_;
            if (instruction == 0x413917 && near_)
                return SourceTransportWord::known(word(function, instruction, offset));
            // NEW repair (PORT-FIX-010): the original reads these stack DWORDs
            // (413917 far detail, 413932, 413AE5) without any producer and stores
            // them as the W of part direction rows (+3C/+2C). NEW defines them as
            // +0.0, the homogeneous W of a direction; all XYZ stay source.
            return SourceTransportWord::known(0u);
        }
    }
    else if (transport_reads_ == 0 && function == 0x413b60 && instruction == 0x44f09b && offset == -0x14)
    {
        // NEW repair (PORT-FIX-010): unproduced 44F09B basis y.W, as above.
        ++transport_reads_;
        return SourceTransportWord::known(0u);
    }
    return remaining_.transport(function, instruction, offset);
}
W SourceVehicleDrawKnownPrefix::word(W function, W instruction, std::int32_t frame_offset)
{
    if (active_ && near_ && !consumed_ && function == 0x4138a0 && instruction == 0x413917 && frame_offset == -4)
    {
        platform_->require_owner_thread();
        if (platform_->state().current_slot != slot_)
            throw SourceAllocationError(instruction, "vehicle changed within captured source draw prefix");
        consumed_ = true;
        return 1u; // 4166F5 PUSH ESI after413DA7 MOV ESI,1; not float1
    }
    return remaining_.word(function, instruction, frame_offset);
}
void SourceVehicleDrawGraph::invoke(RaceSessionPlatform &p, W site, W child, std::optional<unsigned> captured)
{
    if (site == 0x4181b3 && child == 0x413e10)
    {
        if (prefix_)
            prefix_->before_detail();
        const bool completed_near = select_source_vehicle_draw_detail_413e10(p);
        if (prefix_)
            prefix_->after_detail(p, completed_near);
        return;
    }
    if (prefix_ && site == 0x4181be && child == 0x414350)
    {
        prefix_->enter_presentation(p);
        try
        {
            remaining_.invoke(p, site, child, captured);
        }
        catch (...)
        {
            prefix_->leave_presentation();
            throw;
        }
        prefix_->leave_presentation();
        return;
    }
    if (prefix_ && site == 0x4181dc && child == 0x4154c0)
    {
        prefix_->enter_glow(p);
        try
        {
            remaining_.invoke(p, site, child, captured);
        }
        catch (...)
        {
            prefix_->leave_presentation();
            throw;
        }
        prefix_->leave_presentation();
        return;
    }
    if (prefix_)
        prefix_->before_detail(); // any intervening dispatch invalidates the adjacent-call proof
    if (site == 0x4181c3 && child == 0x41c420)
    {
        append_source_vehicle_shadow_41c420(p);
        return;
    }
    remaining_.invoke(p, site, child, captured);
}
void SourceVehicleDrawGraph::leader_notice(RaceSessionPlatform &p, unsigned leader, std::string_view text)
{
    if (prefix_)
        prefix_->before_detail();
    remaining_.leader_notice(p, leader, text);
}
} // namespace sgn::retail
