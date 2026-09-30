#pragma once
// Private CODE-ONLY adapter. This exports current native owners; it does not
// run camera logic, initialize Session, or qualify a normal-parent execution.
#include "normal_session_model_export.hpp"

namespace normal_session_owner {
namespace camera_export_detail {
inline constexpr std::uint32_t controllers = 0x498248, controller_stride = 0x74c;
inline constexpr std::uint32_t records = 0x4722d8;
static_assert(std::endian::native == std::endian::little);
static_assert(sizeof(CameraVector) == 16 && sizeof(CameraMatrix) == 64);
static_assert(sizeof(AuthoredCamera) == 64);

template<class T>void field(Image& out, NativeAddresses& addresses,
    const std::string& name, std::uint32_t at, const T& value) {
    static_assert(std::is_trivially_copyable_v<T>);
    addresses.array(name, std::span(&value, 1), at);
    out.create_live(name, at, std::span(&value, 1));
}
inline void pointer(Image& out, const std::string& name, std::uint32_t at,
    std::uint32_t target, std::uint32_t extent) {
    require(target || extent == 0, "null camera pointer has a nonempty target");
    if (target) out.alias(name + "/target", target, extent);
    const auto bytes = std::span(reinterpret_cast<const std::uint8_t*>(&target), 4);
    out.create_encoded(name, at, bytes, {{0, 4, Origin::relocation, name}});
}
inline std::uint32_t matrix_target(const RaceSessionState& s,
    const NativeAddresses& addresses, std::uint32_t token) {
    if (!token) return 0;
    const CameraMatrix* actual = nullptr;
    if (token == 1) actual = &s.shared_model_identity;
    else {
        require(token >= 2 && (token - 2) % 64 == 0 &&
            (token - 2) / 64 < s.view_matrices.matrices.size(), "camera matrix token outside real owner");
        actual = &s.view_matrices.matrices[(token - 2) / 64];
    }
    const auto target = addresses.address(actual, sizeof(*actual));
    require(target == matrix_address(token), "camera matrix alias differs from model arena identity");
    return target;
}
inline std::uint32_t record_target(const CameraBank& bank,
    const NativeAddresses& addresses, std::optional<std::uint32_t> index) {
    if (!index) return 0;
    // Physical slots remain real owners even when the live count is zero.
    require(*index < bank.records.size(), "camera record index outside physical bank");
    return addresses.address(&bank.records[*index], sizeof(AuthoredCamera));
}
inline bool same_descriptor(const SplineDescriptor& a, const SplineDescriptor& b) {
    return a.id == b.id && a.point_index == b.point_index && a.point_count == b.point_count &&
        a.opaque == b.opaque && std::bit_cast<std::uint32_t>(a.length) == std::bit_cast<std::uint32_t>(b.length) &&
        a.point_offset == b.point_offset;
}

// 42EE50: the loaded PMD remains the physical owner. Its descriptor +4 words
// become point pointers at42EE8A..42EE9C; native SplineSet retains indices.
// The descriptor vector and point-storage subspan supply actual native values.
// They overlay only their corresponding authored sections, never a second PMD.
struct SplinesExport {
    const SplineSet& set;
    std::span<const SplineDescriptor> descriptors;
    std::span<const std::uint8_t> storage;
    std::size_t point_end{};
    std::uint32_t base{}, descriptor_offset{}, point_offset{}, point_bytes{}, owner_id{};

    SplinesExport(RaceSession& session, Image& out, NativeAddresses& addresses, const ModelsExport& models)
        : set(session.splines()), descriptors(set.descriptors()) {
        const auto track = session.track_pmd();
        const auto model = session.scene_resources().model_handle(3);
        require(model.has_value(), "camera spline has no actual track model");
        base = models.guest_base_for(&session.models().at(*model));
        require(track.size() >= 328 && track.size() < 0x400000, "camera spline PMD extent");
        const auto owner = out.reference("camera/spline-pmd", base, narrow(track.size()));
        const auto& allocation = out.allocation(owner.owner_id);
        require(owner.owner_offset == 0 && allocation.bytes.size() == track.size(), "camera spline PMD physical extent differs");
        require(std::equal(track.begin(), track.begin() + 328, allocation.bytes.begin()), "camera spline PMD header differs from model owner");
        owner_id = owner.owner_id;
        descriptor_offset = add(328, word(track, 0x78));
        point_offset = add(328, word(track, 0x74));
        const auto descriptor_bytes = word(track, 0x10c);
        point_bytes = word(track, 0x108);
        require(descriptor_bytes % 28 == 0 && descriptor_bytes / 28 == descriptors.size() && point_bytes % 16 == 0,
            "camera canonical spline section shape differs");
        require(descriptor_offset <= track.size() && descriptor_bytes <= track.size() - descriptor_offset &&
            point_offset <= track.size() && point_bytes <= track.size() - point_offset, "camera spline section outside PMD");
        point_end = std::size_t(point_offset) + point_bytes;
        require(!descriptor_bytes || !point_bytes || point_end <= descriptor_offset ||
            std::uint64_t(descriptor_offset) + descriptor_bytes <= point_offset, "camera spline sections overlap");
        // Without a public canonical storage view, an empty set cannot provide
        // an observed point allocation. Do not replace it with authored bytes.
        require(!descriptors.empty(), "camera codec needs a canonical SplineSet storage view; empty-set owner is unrepresented");
        const auto canonical = set.find(descriptors.front().id);
        require(canonical.has_value(), "canonical first spline descriptor cannot be found");
        storage = canonical->storage;
        require(storage.size() == track.size() && canonical->point_storage_end == point_end &&
            std::equal(track.begin(), track.end(), storage.begin()), "canonical SplineSet does not borrow its actual track bytes");
        require(std::equal(track.begin() + descriptor_offset, track.begin() + descriptor_offset + descriptor_bytes,
            allocation.bytes.begin() + descriptor_offset) &&
            std::equal(track.begin() + point_offset, track.begin() + point_end, allocation.bytes.begin() + point_offset),
            "spline source sections differ or were already overlaid");
        addresses.array("camera/spline-descriptors", descriptors, add(base, descriptor_offset), 28);
        const auto points = storage.subspan(point_offset, point_bytes);
        addresses.array("camera/spline-points", points, add(base, point_offset));
        if (!points.empty()) out.write(owner_id, point_offset, points.data(), points.size(), Origin::live, "camera/spline-points");
        out.alias_in("camera/spline-descriptor-owner", owner_id, descriptor_offset, descriptor_bytes);
        out.alias_in("camera/spline-point-owner", owner_id, point_offset, point_bytes);
        for (std::size_t i = 0; i < descriptors.size(); ++i) {
            const auto& d = descriptors[i];
            const auto at = add(descriptor_offset, i * 28);
            const auto name = "camera/spline/" + std::to_string(i);
            require(d.id == word(track, at) && d.point_index == word(track, at + 4) && d.point_count == word(track, at + 8) &&
                d.opaque[0] == word(track, at + 12) && d.opaque[1] == word(track, at + 16) && d.opaque[2] == word(track, at + 20) &&
                std::bit_cast<std::uint32_t>(d.length) == word(track, at + 24), "canonical spline descriptor differs from original PMD");
            require(d.point_count && (std::uint64_t(d.point_index) + d.point_count) * 16 <= point_bytes &&
                d.point_offset == std::uint64_t(point_offset) + std::uint64_t(d.point_index) * 16, "canonical spline point range differs");
            const auto target = addresses.address(storage.data() + d.point_offset, std::size_t(d.point_count) * 16);
            require(target == add(base, d.point_offset), "canonical spline point identity differs");
            out.live(owner_id, at, d.id, name + "/id");
            out.pointer(owner_id, at + 4, target, name + "/points");
            out.live(owner_id, at + 8, d.point_count, name + "/count");
            out.live(owner_id, at + 12, d.opaque, name + "/opaque");
            out.live(owner_id, at + 24, d.length, name + "/length");
            out.alias_in(name + "/point-view", owner_id, narrow(d.point_offset), narrow(std::size_t(d.point_count) * 16));
        }
        // Loaded-Session lifetime view, not independently writable globals:
        //42EEA5/42EEAC (and42EEB7/42EEBE for an empty source table) publish
        // this exact descriptor base and count.42E674/42E6AA clear them before
        // loading; that pre-load/reset domain is outside this adapter.
        const auto descriptor_root = addresses.address(descriptors.data(), descriptors.size_bytes());
        pointer(out, "camera/spline-root", 0x624474, descriptor_root, descriptor_bytes);
        const auto descriptor_count = narrow(descriptors.size());
        out.create_live("camera/spline-count", 0x624498, std::span(&descriptor_count, 1));
    }
    std::uint32_t target(const std::optional<SplineView>& view, const NativeAddresses& addresses) const {
        if (!view) return 0;
        const auto canonical = set.find(view->descriptor.id);
        require(canonical && view->storage.data() == storage.data() && view->storage.size() == storage.size() &&
            view->point_storage_end == point_end && same_descriptor(view->descriptor, canonical->descriptor),
            "camera spline is not the actual canonical borrowed view");
        for (std::size_t i = 0; i < descriptors.size(); ++i) if (descriptors[i].id == view->descriptor.id) {
            require(same_descriptor(descriptors[i], view->descriptor), "camera spline descriptor identity differs");
            const auto result = addresses.address(&descriptors[i], sizeof(SplineDescriptor));
            require(result == add(base, descriptor_offset + i * 28), "camera spline descriptor alias differs");
            return result;
        }
        throw std::runtime_error("camera spline descriptor has no canonical native owner");
    }
};
} // namespace camera_export_detail

// Preconditions: ModelsExport::snapshot registered the real PMD/matrix owners;
// the vehicle exporter registered each referenced49A1F8+i*7724 prefix. This is
// the live post-load, nonempty SplineSet domain; it cannot encode pre-load or
// unload/reset spline roots/counts. Missing
// roots cause an exception. No vehicle bytes, controller holes, or diagnostic
// +200 camera fields are invented. Discard the output Image after any exception.
inline void export_camera_owners(RaceSession& session, Image& out,
    NativeAddresses& addresses, const ModelsExport& models) {
    using namespace camera_export_detail;
    const auto& s = session.state();
    require(s.controls.current < s.controls.controllers.size(), "selected camera controller outside physical bank");
    SplinesExport splines(session, out, addresses, models);
    addresses.array("camera/records", std::span(s.camera_bank.records), records);
    out.create_live("camera/records", records, std::span(s.camera_bank.records));
    for (std::size_t i = 0; i < s.controls.controllers.size(); ++i) {
        const auto& controller = s.controls.controllers[i];
        const auto& c = controller.camera;
        const auto& life = s.camera_lifecycle[i];
        const auto& p = s.camera_presentation[i];
        const auto base = add(controllers, i * controller_stride);
        const auto key = "camera/controller/" + std::to_string(i) + "/";
        const auto live = [&](const char* name, std::uint32_t offset, const auto& value) {
            field(out, addresses, key + name, add(base, offset), value);
        };
        const auto ptr = [&](const char* name, std::uint32_t offset, std::uint32_t target, std::uint32_t extent) {
            pointer(out, key + name, add(base, offset), target, target ? extent : 0);
        };
        live("active", 0x000, controller.active);
        live("mode", 0x004, c.mode); live("position", 0x008, c.position);
        live("scale", 0x018, life.scale); live("target", 0x028, c.target); live("up", 0x038, c.up);
        live("transform-word48", 0x048, life.transform_word_48);
        ptr("world-to-camera", 0x04c, matrix_target(s, addresses, c.world_to_camera_reference), 64);
        ptr("camera-to-world", 0x050, matrix_target(s, addresses, c.camera_to_world_reference), 64);
        live("transform-word54", 0x054, life.transform_word);
        live("previous-target", 0x068, c.previous_target); live("twist", 0x078, c.twist); live("side", 0x07c, c.side);
        live("up-axis", 0x08c, p.up_axis); live("forward-axis", 0x09c, p.forward_axis);
        require(controller.vehicle < s.vehicles.physics.size(), "camera vehicle slot outside physical owners");
        ptr("vehicle", 0x0ac, add(0x49a1f8, std::size_t(controller.vehicle) * 0x7724), 4);
        live("lens", 0x0b0, c.lens); live("near-clip", 0x0b4, c.near_clip); live("far-factor", 0x0b8, c.far_factor);
        live("half-width", 0x0bc, c.half_width); live("half-height", 0x0c0, c.half_height);
        live("velocity", 0x0c4, c.velocity); live("linear-speed", 0x0d4, c.linear_speed); live("drift", 0x0d8, c.drift);
        live("control-angles", 0x0e8, c.control_angles); live("angular-velocity", 0x0f4, c.angular_velocity);
        live("angular-acceleration", 0x104, c.angular_acceleration); live("planes", 0x118, p.planes);
        live("clip-near", 0x198, life.clip_near); live("clip-far", 0x19c, life.clip_far); live("base-lens", 0x1a0, life.base_lens);
        live("collision-bypass-ticks", 0x1a4, c.collision_bypass_ticks); ptr("spline", 0x1a8, splines.target(p.spline, addresses), 28);
        live("stabilized-up", 0x1ac, c.stabilized_up); live("settle-ticks", 0x1bc, c.settle_ticks);
        live("speed-lift", 0x1c0, c.speed_lift); live("height-offset", 0x1c4, c.height_offset); live("unknown1c8", 0x1c8, life.unknown_1c8);
        ptr("current-record", 0x1cc, record_target(s.camera_bank, addresses, life.current_record), 64);
        live("next-mode", 0x1d0, life.next_mode); live("pending-follow-setup", 0x1d4, c.pending_follow_setup);
        ptr("pending-record", 0x1d8, record_target(s.camera_bank, addresses, life.pending_record), 64);
        live("pending-mode", 0x1dc, life.pending_mode); live("pending-lens", 0x1e0, c.pending_lens); live("requested-lens", 0x1e4, c.requested_lens);
    }
    const auto selected = addresses.address(&s.controls.controllers[s.controls.current].active, 4);
    pointer(out, "camera/current-controller", 0x4722d0, selected, 4);
    const auto global = [&](const char* name, std::uint32_t at, const auto& value) {
        field(out, addresses, std::string("camera/") + name, at, value);
    };
    global("path-speed", 0x499f88, s.camera_session.path_speed); global("path-parameter", 0x499f8c, s.camera_session.path_parameter);
    global("record-count", 0x499f90, s.camera_bank.count); global("authored-cycle", 0x499f94, s.camera_session.authored_cycle_index);
    global("selected-preset", 0x60f020, s.selected_camera_preset);
    global("frame-projection", 0x61c3e8, s.frame_projection); global("frame-world-view", 0x61c594, s.frame_world_view);
    global("inverse-depth-scale", 0x61c534, s.inverse_depth_scale); global("depth-offset", 0x61c538, s.depth_offset);
    // Explicit alias_in avoids the adjacent6227CC cursor word being confused
    // with an arena one-past pointer. Count is observed independently.
    std::uint32_t cursor = 0;
    if (s.view_matrices.cursor) {
        const auto offset = *s.view_matrices.cursor;
        require(offset % 64 == 0 && offset / 64 <= s.view_matrices.matrices.size(), "camera arena cursor outside owner");
        const auto arena = out.reference("camera/arena", 0x61c7cc, narrow(sizeof(s.view_matrices.matrices)));
        require(arena.owner_offset == 0 && out.allocation(arena.owner_id).bytes.size() == sizeof(s.view_matrices.matrices), "camera arena physical extent differs");
        cursor = add(0x61c7cc, offset);
        out.alias_in("camera/arena-cursor-target", arena.owner_id, offset, 0);
        // Resolve the last in-bounds matrix, then add64 for an exhausted arena;
        // NativeAddresses may have an adjacent host owner at its end as well.
        const auto count = s.view_matrices.matrices.size();
        const auto index = std::size_t(offset / 64);
        const auto checked = addresses.address(&s.view_matrices.matrices[index < count ? index : count - 1], 64);
        require(add(checked, index == count ? 64 : 0) == cursor, "camera arena cursor native identity differs");
    }
    const auto cursor_bytes = std::span(reinterpret_cast<const std::uint8_t*>(&cursor), 4);
    out.create_encoded("camera/arena-cursor", 0x6227cc, cursor_bytes, {{0, 4, Origin::relocation, "camera/arena-cursor"}});
    global("arena-count", 0x6227d0, s.view_matrices.count);
    const auto& v = s.viewport;
    global("viewport/width", 0x576b80, v.width); global("viewport/height", 0x576b84, v.height);
    global("viewport/center-pixel-x", 0x576b8c, v.center_pixel_x); global("viewport/center-pixel-y", 0x576b90, v.center_pixel_y);
    global("viewport/center-x", 0x576b94, v.center_x); global("viewport/center-y", 0x576b98, v.center_y);
    global("viewport/scale-x", 0x576b9c, v.scale_x); global("viewport/scale-y", 0x576ba0, v.scale_y);
    global("viewport/right-edge", 0x576ba4, v.right_edge); global("viewport/bottom-edge", 0x576ba8, v.bottom_edge);
    global("viewport/min-x", 0x576bac, v.min_x); global("viewport/min-y", 0x576bb0, v.min_y);
    global("viewport/max-x", 0x576bb4, v.max_x); global("viewport/max-y", 0x576bb8, v.max_y);
    global("viewport/depth-bits", 0x576bbc, v.depth_bits); global("viewport/bytes-per-pixel", 0x576bc0, v.bytes_per_pixel);
    global("viewport/full-width", 0x576bc4, v.full_width); global("viewport/full-height", 0x576bc8, v.full_height);
    global("viewport/cropped", 0x576bd8, v.cropped); global("viewport/selector", 0x576bdc, v.selector);
    global("viewport/split-setting", 0x576be0, s.split_setting); global("viewport/origin-x", 0x576be4, v.origin_x); global("viewport/origin-y", 0x576be8, v.origin_y);
    global("viewport/clip-virtual", 0x576c04, v.clip_virtual); global("viewport/clip-screen", 0x576c14, v.clip_screen); global("viewport/clip-expanded", 0x576c24, v.clip_expanded);
    global("viewport/applied-selector", 0x624988, v.applied_selector); global("viewport/requested-selector", 0x62498c, v.requested_selector);
    global("viewport/ui-scale-x", 0x624990, v.ui_scale_x); global("viewport/ui-scale-y", 0x624994, v.ui_scale_y);
    global("quality/first", 0x624980, s.view_quality.first); global("quality/second", 0x624984, s.view_quality.second);
    global("quality/legacy-driver-restriction", 0x589e08, s.view_quality.legacy_driver_restriction);
    global("light/phase-fast", 0x589df8, s.light_animation.phase_fast); global("light/phase-slow", 0x589dfc, s.light_animation.phase_slow);
    global("glare/elements", 0x477b88, s.glare.elements);
}
} // namespace normal_session_owner
