#include "sgn/vehicle_presentation.hpp"
#include "sgn/model_resources.hpp"
#include <bit>
#include <cmath>

namespace sgn::retail
{
namespace
{
std::int32_t decrement(std::int32_t value) noexcept
{
    return std::bit_cast<std::int32_t>(std::bit_cast<std::uint32_t>(value) - 1u);
}
std::int32_t idle_random(VehicleRandom &random) noexcept
{
    // Complete 00463289 then 0042f1f0 with ECX=32. The signed product cannot
    // overflow for this bound; the source magic division truncates toward zero.
    random.state = random.state * 0x343fdu + 0x269ec3u;
    const auto sample = static_cast<std::int32_t>((random.state >> 16) & 0x7fff) - 0x3fff;
    return (sample * 32) / 32767;
}
} // namespace
void initialize_cold_presentation(ColdPresentationStorage storage, std::optional<std::size_t> model,
                                  std::optional<std::size_t> shadow, const ModelTransform &template_pose)
{
    if (storage.presentation.size() != 8 || storage.models.size() != 8 || storage.wheels.size() != 8)
        throw std::invalid_argument("cold presentation requires eight original allocation slots");
    for (std::size_t i = 0; i < 8; ++i)
    {
        storage.presentation[i] = {};
        storage.models[i] = {};
        storage.wheels[i] = {};
    }
    auto &first = storage.models[0];
    first.model = model;
    first.shadow = shadow;
    // 40A3EF copies the current 80 bytes, including scale W and reference words.
    const auto vectors = std::bit_cast<std::array<VehicleVector, 5>>(template_pose);
    first.position = vectors[0];
    first.scale = vectors[1];
    first.target = vectors[2];
    first.up = vectors[3];
    first.template_tail = template_pose.tail;
    // 0040a3f1..0040a425 adds origin XYZ to the copied target XYZ.
    first.target.x = vehicle_f32(double(first.position.x) + first.target.x);
    first.target.y = vehicle_f32(double(first.position.y) + first.target.y);
    first.target.z = vehicle_f32(double(first.position.z) + first.target.z);
    first.body_rotation = vehicle_identity();
    for (auto &wheel : storage.wheels[0])
        wheel.rotation = vehicle_identity();
}
void update_vehicle_effect_parts(const VehiclePresentationEffects &effects, VehicleEffectPart first,
                                 VehicleEffectPart second)
{
    if (effects.field_226c == 0)
    {
        if (effects.field_328c == 1)
            first.flags = 0;
        if (effects.field_33cc == 1)
            second.flags = 0;
    }
    else if (effects.field_328c == 1)
    {
        first.flags = 1;
        first.scale = effects.first_scale;
        // The original second activation is nested under the first one.
        if (effects.field_33cc == 1)
        {
            second.flags = 1;
            second.scale = effects.second_scale;
        }
    }
}
void prepare_vehicle_presentation(VehiclePresentationState &state, const VehiclePresentationEffects &effects,
                                  VehiclePresentationParts parts)
{
    state.field_3134 = 1;
    for (auto flag : parts.flags_11_to_16)
        flag.get() = 1;
    state.field_3928 = state.field_3924;
    update_vehicle_effect_parts(effects, parts.part19, parts.part20);
    state.field_33c8 = 1;
    state.field_3508 = 1;
}
void animate_vehicle_wheels(const VehicleParameters &parameters, const VehicleState &state,
                            std::span<VehicleWheelAnimation, 4> animation)
{
    for (std::size_t i = 0; i < 4; ++i)
    {
        const double distance = double(state.wheels[i].speed) * 180.0;
        const double radius = double(parameters.tyres[i].radius) * std::bit_cast<float>(std::uint32_t{0x40490fdb});
        animation[i].previous_angle = animation[i].current_angle;
        animation[i].current_angle = vehicle_f32(-(distance / radius) + animation[i].current_angle);
        animation[i].rotation = vehicle_rotation(0, animation[i].current_angle);
    }
}
void locate_vehicle_wheels(const VehicleParameters &parameters, const VehicleState &state,
                           VehiclePresentationState &out, VehicleWheelPositionWriteAudit *audit)
{
    // 0041446c..004144a7: X stays in x87; Y and Z origins are stored as floats.
    const double x = double(state.up.x) + state.position.x;
    const float y = vehicle_f32(double(state.up.y) + state.position.y);
    const float z = vehicle_f32(double(state.up.z) + state.position.z);
    for (std::size_t i = 0; i < 4; ++i)
    {
        const double forward = (i < 2 ? -double(parameters.com_b) : double(parameters.com_a)) * parameters.wheelbase;
        const double lateral = double(i < 2 ? parameters.track_rear : parameters.track_front) * (i % 2 ? -0.5 : 0.5);
        const float fx = vehicle_f32(forward * state.basis_z.x), fy = vehicle_f32(forward * state.basis_z.y);
        const float lx = vehicle_f32(lateral * state.basis_x.x), ly = vehicle_f32(lateral * state.basis_x.y);
        const float dx = vehicle_f32(double(lx) + fx), dy = vehicle_f32(double(ly) + fy);
        const double dz = lateral * state.basis_x.z + forward * state.basis_z.z;
        // Only the fourth wheel stores its intermediate Z sum (004146d4).
        const double sum_z = i == 3 ? double(vehicle_f32(dz)) : dz;
        out.wheel_centers[i] = {vehicle_f32(double(dx) + x), vehicle_f32(double(dy) + y), vehicle_f32(sum_z + z),
                                out.wheel_centers[i].w};
        // 414538 reads an unwritten EBP-4 word; four literal copies publish it.
        //  The native payload is retained; this callback only records the write.
        if (audit && audit->retained_word)
            audit->retained_word(audit->context, static_cast<unsigned>(i), out.wheel_centers[i].w);
    }
}
void idle_vehicle_suspension(VehicleState &physics, VehiclePresentationState &state, VehicleRandom &random)
{
    if (physics.contact_class != 0)
        return;
    const float speed = std::fabs(physics.speed);
    if (speed > 3.0f)
    {
        state.idle_hold = 300;
        return;
    }
    state.idle_hold = decrement(state.idle_hold);
    if (state.idle_hold < 0)
        state.idle_hold = 0;
    if (state.idle_hold >= 180)
        return;
    state.idle_interval = decrement(state.idle_interval);
    if (state.idle_interval > 0)
        return;
    state.idle_interval = 8;
    float amount = vehicle_f32(1.0 / (double(state.idle_hold / 16 + 1) + speed));
    if (state.idle_hold == 0 && idle_random(random) == 1)
        amount = 2.0f;
    for (auto &displacement : physics.suspension)
        displacement = vehicle_f32(double(amount) + displacement);
}
} // namespace sgn::retail
