#pragma once
// Independent raw native observations. This file does not include the camera
// exporter, owner wire, model exporter, or any guest-address helper.
#include "normal_session_native_observer.hpp"

namespace normal_native_observer {
inline void camera_descriptor(Observations& out, const std::string& key, const SplineDescriptor& d) {
    out.ref(key + "/id", d.id); out.ref(key + "/point-index", d.point_index);
    out.ref(key + "/point-count", d.point_count); out.ref(key + "/opaque", d.opaque);
    out.ref(key + "/length", d.length); out.ref(key + "/point-offset", d.point_offset);
}
inline void capture_camera(RaceSession& session, const std::filesystem::path& output) {
    const auto& s = session.state();
    Observations out;
    out.ref("camera/current-controller-index", s.controls.current);
    if (s.controls.current >= s.controls.controllers.size()) throw std::runtime_error("native camera observation controller index");
    out.value("camera/current-controller-address", reinterpret_cast<std::uint64_t>(&s.controls.controllers[s.controls.current]));
    out.span("camera/records", std::span(s.camera_bank.records));
    out.ref("camera/record-count", s.camera_bank.count);
    out.ref("camera/path-speed", s.camera_session.path_speed); out.ref("camera/path-parameter", s.camera_session.path_parameter);
    out.ref("camera/authored-cycle", s.camera_session.authored_cycle_index); out.ref("camera/selected-preset", s.selected_camera_preset);
    // Actual canonical descriptors have native size_t offsets and padding;
    // observe fields individually and retain the full native owner identity.
    const auto descriptors = session.splines().descriptors();
    out.value("camera/splines/address", reinterpret_cast<std::uint64_t>(descriptors.data()));
    out.value("camera/splines/count", static_cast<std::uint64_t>(descriptors.size()));
    out.value("camera/splines/native-stride", static_cast<std::uint32_t>(sizeof(SplineDescriptor)));
    out.span("camera/track-pmd", session.track_pmd());
    for (std::size_t i = 0; i < descriptors.size(); ++i) {
        const auto key = "camera/spline/" + std::to_string(i);
        camera_descriptor(out, key, descriptors[i]);
        out.value(key + "/address", reinterpret_cast<std::uint64_t>(&descriptors[i]));
    }
    if (!descriptors.empty()) {
        const auto first = session.splines().find(descriptors.front().id);
        if (!first) throw std::runtime_error("native canonical spline first record absent");
        out.span("camera/splines/storage", first->storage);
        out.value("camera/splines/point-storage-end", first->point_storage_end);
    }
    for (std::size_t i = 0; i < s.controls.controllers.size(); ++i) {
        const auto& controller = s.controls.controllers[i];
        const auto& c = controller.camera;
        const auto& life = s.camera_lifecycle[i];
        const auto& p = s.camera_presentation[i];
        const auto key = "camera/controller/" + std::to_string(i) + "/";
        out.value(key + "address", reinterpret_cast<std::uint64_t>(&controller));
        out.ref(key + "active", controller.active); out.ref(key + "vehicle-slot", controller.vehicle);
        if (controller.vehicle >= s.vehicles.physics.size()) throw std::runtime_error("native camera observation vehicle index");
        out.value(key + "vehicle-physics-address", reinterpret_cast<std::uint64_t>(&s.vehicles.physics[controller.vehicle]));
        out.ref(key + "mode", c.mode); out.ref(key + "position", c.position); out.ref(key + "scale", life.scale);
        out.ref(key + "target", c.target); out.ref(key + "up", c.up);
        out.ref(key + "transform-word48", life.transform_word_48); out.ref(key + "transform-word54", life.transform_word);
        // Keep native tokens; do not normalize them with the exporter's helper.
        out.ref(key + "world-to-camera-token", c.world_to_camera_reference);
        out.ref(key + "camera-to-world-token", c.camera_to_world_reference);
        out.ref(key + "previous-target", c.previous_target); out.ref(key + "twist", c.twist); out.ref(key + "side", c.side);
        out.ref(key + "up-axis", p.up_axis); out.ref(key + "forward-axis", p.forward_axis);
        out.ref(key + "lens", c.lens); out.ref(key + "near-clip", c.near_clip); out.ref(key + "far-factor", c.far_factor);
        out.ref(key + "half-width", c.half_width); out.ref(key + "half-height", c.half_height);
        out.ref(key + "velocity", c.velocity); out.ref(key + "linear-speed", c.linear_speed); out.ref(key + "drift", c.drift);
        out.ref(key + "control-angles", c.control_angles); out.ref(key + "angular-velocity", c.angular_velocity);
        out.ref(key + "angular-acceleration", c.angular_acceleration); out.ref(key + "planes", p.planes);
        out.ref(key + "clip-near", life.clip_near); out.ref(key + "clip-far", life.clip_far); out.ref(key + "base-lens", life.base_lens);
        out.ref(key + "collision-bypass-ticks", c.collision_bypass_ticks); out.ref(key + "stabilized-up", c.stabilized_up);
        out.ref(key + "settle-ticks", c.settle_ticks); out.ref(key + "speed-lift", c.speed_lift); out.ref(key + "height-offset", c.height_offset);
        out.ref(key + "unknown1c8", life.unknown_1c8); out.ref(key + "next-mode", life.next_mode);
        out.ref(key + "pending-follow-setup", c.pending_follow_setup); out.ref(key + "pending-mode", life.pending_mode);
        out.ref(key + "pending-lens", c.pending_lens); out.ref(key + "requested-lens", c.requested_lens);
        const auto record = [&](const char* name, const std::optional<std::uint32_t>& index) {
            const auto prefix = key + name;
            out.value(prefix + "/present", std::uint32_t(index.has_value()));
            if (index) {
                out.ref(prefix + "/index", *index);
                if (*index >= s.camera_bank.records.size()) throw std::runtime_error("native camera observation record index");
                out.value(prefix + "/target-address", reinterpret_cast<std::uint64_t>(&s.camera_bank.records[*index]));
            } else out.value(prefix + "/target-address", std::uint64_t{0});
        };
        record("current-record", life.current_record); record("pending-record", life.pending_record);
        out.value(key + "spline/present", std::uint32_t(p.spline.has_value()));
        if (p.spline) {
            const auto& view = *p.spline;
            camera_descriptor(out, key + "spline/borrowed-descriptor", view.descriptor);
            out.span(key + "spline/storage", view.storage);
            out.ref(key + "spline/point-storage-end", view.point_storage_end);
            // This is raw evidence for identity checks, not an assertion that
            // a descriptor with the same ID has the same storage or contents.
            bool found = false;
            for (std::size_t n = 0; n < descriptors.size(); ++n) if (descriptors[n].id == view.descriptor.id) {
                out.value(key + "spline/canonical-index", static_cast<std::uint64_t>(n));
                out.value(key + "spline/canonical-address", reinterpret_cast<std::uint64_t>(&descriptors[n]));
                found = true; break;
            }
            out.value(key + "spline/canonical-found", std::uint32_t(found));
        }
        // These numerical caches have no camera+200 source-storage identity.
        out.ref(key + "native-only/projection", c.projection);
        out.ref(key + "native-only/camera-to-world-cache", c.camera_to_world_rotation);
        out.ref(key + "native-only/world-to-camera-cache", c.world_to_camera_rotation);
        out.ref(key + "native-only/inverse-depth-scale", c.inverse_depth_scale);
        out.ref(key + "native-only/depth-offset", c.depth_offset);
    }
    out.ref("camera/frame-projection", s.frame_projection); out.ref("camera/frame-world-view", s.frame_world_view);
    out.ref("camera/inverse-depth-scale", s.inverse_depth_scale); out.ref("camera/depth-offset", s.depth_offset);
    out.span("camera/matrix-arena", std::span(s.view_matrices.matrices)); out.ref("camera/shared-identity", s.shared_model_identity);
    out.value("camera/arena-cursor/present", std::uint32_t(s.view_matrices.cursor.has_value()));
    if (s.view_matrices.cursor) {
        out.ref("camera/arena-cursor/byte-offset", *s.view_matrices.cursor);
        if (*s.view_matrices.cursor > sizeof(s.view_matrices.matrices)) throw std::runtime_error("native camera observation arena cursor extent");
        const auto* bytes = reinterpret_cast<const std::uint8_t*>(&s.view_matrices.matrices);
        out.value("camera/arena-cursor/target-address", reinterpret_cast<std::uint64_t>(bytes + *s.view_matrices.cursor));
    } else out.value("camera/arena-cursor/target-address", std::uint64_t{0});
    out.ref("camera/arena-count", s.view_matrices.count);
    const auto& v = s.viewport;
    out.ref("camera/viewport/width", v.width); out.ref("camera/viewport/height", v.height);
    out.ref("camera/viewport/center-pixel-x", v.center_pixel_x); out.ref("camera/viewport/center-pixel-y", v.center_pixel_y);
    out.ref("camera/viewport/center-x", v.center_x); out.ref("camera/viewport/center-y", v.center_y);
    out.ref("camera/viewport/scale-x", v.scale_x); out.ref("camera/viewport/scale-y", v.scale_y);
    out.ref("camera/viewport/right-edge", v.right_edge); out.ref("camera/viewport/bottom-edge", v.bottom_edge);
    out.ref("camera/viewport/min-x", v.min_x); out.ref("camera/viewport/min-y", v.min_y);
    out.ref("camera/viewport/max-x", v.max_x); out.ref("camera/viewport/max-y", v.max_y);
    out.ref("camera/viewport/depth-bits", v.depth_bits); out.ref("camera/viewport/bytes-per-pixel", v.bytes_per_pixel);
    out.ref("camera/viewport/full-width", v.full_width); out.ref("camera/viewport/full-height", v.full_height);
    out.ref("camera/viewport/cropped", v.cropped); out.ref("camera/viewport/selector", v.selector); out.ref("camera/viewport/split-setting", s.split_setting);
    out.ref("camera/viewport/origin-x", v.origin_x); out.ref("camera/viewport/origin-y", v.origin_y);
    out.ref("camera/viewport/clip-virtual", v.clip_virtual); out.ref("camera/viewport/clip-screen", v.clip_screen); out.ref("camera/viewport/clip-expanded", v.clip_expanded);
    out.ref("camera/viewport/applied-selector", v.applied_selector); out.ref("camera/viewport/requested-selector", v.requested_selector);
    out.ref("camera/viewport/ui-scale-x", v.ui_scale_x); out.ref("camera/viewport/ui-scale-y", v.ui_scale_y);
    out.ref("camera/quality/first", s.view_quality.first); out.ref("camera/quality/second", s.view_quality.second);
    out.ref("camera/quality/legacy-driver-restriction", s.view_quality.legacy_driver_restriction);
    out.ref("camera/light/phase-fast", s.light_animation.phase_fast); out.ref("camera/light/phase-slow", s.light_animation.phase_slow);
    out.ref("camera/glare/elements", s.glare.elements);
    out.save(output);
}
} // namespace normal_native_observer
