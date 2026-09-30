#include "sgn/retail_vehicle_presentation_frame.hpp"
#include "sgn/retail_vehicle_strip_draw.hpp"
#include "sgn/vehicle_draw_pose.hpp"
#include "sgn/vehicle_model_detail.hpp"
#include "sgn/vehicle_model_prepare.hpp"
#include "sgn/vehicle_shadow_draw.hpp"
#include "vehicle_presentation_frame_provider.hpp"

namespace sgn::retail
{
VehiclePresentationDraws detail::prepare_vehicle_presentation_frame_observed(VehiclePresentationFrame frame,
                                                                             PresentationCallAudit *audit)
{
    VehiclePresentationDraws result;
    const auto event = [&](unsigned site, unsigned entry, bool after) {
        if (audit && audit->boundary)
            audit->boundary(audit->context, {site, entry, frame.current_slot, after});
    };
    const auto call = [&](unsigned site, unsigned entry, auto &&action) {
        event(site, entry, false);
        action();
        event(site, entry, true);
    };
    event(0x418180, 0x418180, false);
    frame.current_slot = 0; // 418189: before even the generated strip call
    const auto strip_index = frame.draws.background.cursor;
    call(0x418193, 0x41d1c0, [&] {
        if (append_vehicle_strip_draw(frame.strip, frame.quality, frame.draws.template_pose, frame.draws.background,
                                      frame.matrices))
            result.strip = strip_index;
    });

    for (unsigned remaining = VehicleStorage::physical_slots; remaining; --remaining, ++frame.current_slot)
    {
        const auto slot = frame.current_slot;
        auto &vehicles = frame.vehicles;
        if (vehicles.race[slot].active != 1)
            continue; // 4181A6..4181AD
        const auto body = [&] {
            if (!vehicles.models[slot].model)
                throw FormatError("active vehicle presentation requires its model");
            return *vehicles.models[slot].model;
        };
        const auto initial = body();
        if (!frame.models.source_backed(initial) &&
            &frame.models.at(initial).current_detail_level() != &frame.selected_lod[slot])
            throw FormatError("vehicle presentation requires the sole bound model LOD owner");

        call(0x4181b3, 0x413e10, [&] {
            prepare_vehicle_model_detail(
                vehicles.physics[slot], vehicles.presentation[slot], vehicles.particles[slot],
                vehicle_presentation_effects(vehicles.model_effects[slot], vehicles.anchors[slot]), frame.models,
                body(), frame.camera.camera, frame.camera.presentation.forward_axis, frame.quality);
        });
        call(0x4181be, 0x414350, [&] {
            prepare_vehicle_model(vehicles.physics[slot], vehicles.trail_inputs[slot], vehicles.wheels[slot],
                                  vehicles.presentation[slot], vehicles.anchors[slot], vehicles.segments[slot],
                                  frame.models, body(), frame.model_basis, vehicles.rail[slot].effect_position);
        });

        // 41C420 returns before reading the shadow pointer when3928 is zero.
        call(0x4181c3, 0x41c420, [&] {
            if (vehicles.presentation[slot].field_3928 != 0)
            {
                if (!vehicles.models[slot].shadow)
                    throw FormatError("visible vehicle shadow requires its model");
                const auto index = frame.draws.background.cursor;
                if (append_vehicle_shadow_draw(vehicles.presentation[slot], vehicles.shadows[slot], frame.models,
                                               body(), *vehicles.models[slot].shadow, frame.draws.background,
                                               frame.matrices, frame.draws.template_pose))
                    result.shadows[slot] = index;
            }
        });

        const VehicleTrailRenderBindings trails{
            {frame.profiles, frame.staging, frame.sprites, frame.screen, frame.viewport, frame.camera,
             vehicles.physics[slot].orientation, frame.split_setting},
            vehicles.physics[slot],
            vehicles.trails[slot],
            vehicles.trail_inputs[slot],
            vehicles.model_effects[slot],
            vehicles.anchors[slot],
            vehicles.presentation[slot],
            VehicleTrailBasisSource(frame.models, vehicles.models[slot].model),
            frame.model_basis,
            vehicles.model_effects[slot].glow_phases,
            frame.quality,
            slot,
            frame.camera_vehicle_slot}; // legacy field name source_tick means QUALITY61297C
        call(0x4181c8, 0x414cf0, [&] { render_vehicle_speed_trail(trails); });
        call(0x4181cd, 0x414e50, [&] { render_vehicle_effect_trails(trails); });
        call(0x4181d2, 0x415040, [&] { render_vehicle_pair_a_trails(trails); });
        call(0x4181d7, 0x4153a0, [&] { render_vehicle_pair_b_trails(trails); });
        call(0x4181dc, 0x4154c0, [&] { render_vehicle_pair_b_glows_observed(trails, audit); });

        auto pose =
            vehicle_draw_pose(vehicles.physics[slot], vehicles.contact[slot].rig.origin_y_offset,
                              frame.draws.template_pose); // 4181E1..41829B, copy AFTER strip/shadow/trail mutations
        call(0x41829c, 0x418150,
             [&] { update_vehicle_draw_ambience(vehicles.model_effects[slot], frame.models, body()); });
        update_vehicle_draw_flags(vehicles.race[slot], vehicles.models[slot]); // 4182A1..4182CF
        result.cars[slot] = frame.draws.regular.cursor;
        call(0x4182df, 0x42b850, [&] {
            const auto handle = body();
            append_model_draw(frame.draws.regular, frame.models, handle, frame.models.current_detail_level(handle),
                              pose, vehicles.models[slot].field_2708, frame.matrices);
        });
    }
    frame.current_slot = 0; // 418305, after the physical iterator has reached slot6
    event(0x418180, 0x418180, true);
    return result;
}
VehiclePresentationDraws prepare_vehicle_presentation_frame(VehiclePresentationFrame frame)
{
    return detail::prepare_vehicle_presentation_frame_observed(frame, nullptr);
}
} // namespace sgn::retail
