#pragma once
#if !defined(SGN_CAMERA_RECOVERY_RECEIVING) || SGN_CAMERA_RECOVERY_RECEIVING != 1
#error This fieldwise witness requires the explicit receiving-only build definition.
#endif
#include "sgn/race_session.hpp"
#include "sgn/front_recipe_bundle.hpp"
#include <bit>
#include <concepts>
#include <limits>
#include <set>
#include <type_traits>

namespace sgn::diagnostics
{
// NEW native same-process diagnostic, not a retail memory image or oracle.
// Every field owns encoded bytes. No material frame, COM owner or borrowed span
// survives capture, and no source camera or resource-loading function is called.
struct CameraRecoveryStamp
{
    std::uint64_t host_clock_tick{}, completed_tick{}, driver_calls{}, camera_begin_calls{}, source_captures{};
};
struct CameraRecoveryField
{
    std::string name;
    std::vector<std::uint8_t> bytes;
    bool operator==(const CameraRecoveryField &) const = default;
};
struct CameraRecoverySnapshot
{
    std::vector<CameraRecoveryField> fields;
    bool operator==(const CameraRecoverySnapshot &) const = default;
};
namespace camera_witness_detail
{
template <std::integral T> void append(std::vector<std::uint8_t> &out, T value)
{
    if constexpr (std::same_as<T, bool>)
        out.push_back(static_cast<std::uint8_t>(value ? 1 : 0));
    else
    {
        using U = std::make_unsigned_t<T>;
        const auto bits = static_cast<U>(value);
        for (std::size_t n = 0; n < sizeof(T); ++n)
            out.push_back(static_cast<std::uint8_t>((bits >> (n * 8)) & U{255}));
    }
}
inline void append(std::vector<std::uint8_t> &out, const float &value)
{
    static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559);
    append(out, std::bit_cast<std::uint32_t>(value));
}
inline void append(std::vector<std::uint8_t> &out, const retail::CameraVector &value)
{
    append(out, value.x);
    append(out, value.y);
    append(out, value.z);
    append(out, value.w);
}
template <class T, std::size_t N> void append(std::vector<std::uint8_t> &out, const std::array<T, N> &values)
{
    for (const auto &value : values)
        append(out, value);
}
class Fields
{
    CameraRecoverySnapshot snapshot_;
    std::set<std::string> names_;
    std::size_t payload_{};
    void commit(CameraRecoveryField field)
    {
        constexpr std::size_t limit = 64u * 1024u * 1024u;
        if (field.name.empty() || field.name.size() > 512 || snapshot_.fields.size() >= 2048 ||
            field.bytes.size() > limit || payload_ > limit - field.bytes.size() || !names_.insert(field.name).second)
            throw std::runtime_error("camera witness field identity or extent is invalid");
        payload_ += field.bytes.size();
        snapshot_.fields.push_back(std::move(field));
    }

  public:
    template <class T> void add(std::string name, const T &value)
    {
        CameraRecoveryField field{std::move(name), {}};
        append(field.bytes, value);
        commit(std::move(field));
    }
    void bytes(std::string name, std::span<const std::uint8_t> data)
    {
        if (data.size() > 16u * 1024u * 1024u || (!data.empty() && !data.data()))
            throw std::runtime_error("camera witness borrowed byte extent invalid");
        std::vector<std::uint8_t> copy;
        if (!data.empty())
            copy.assign(data.begin(), data.end());
        commit({std::move(name), std::move(copy)});
    }
    template <class T> void address(std::string name, const T *pointer)
    {
        static_assert(sizeof(std::uintptr_t) <= sizeof(std::uint64_t));
        add(std::move(name), static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(pointer)));
    }
    void optional(std::string name, const std::optional<std::uint32_t> &value)
    {
        add(name + "/present", value.has_value());
        if (value)
            add(name + "/value", *value);
    }
    CameraRecoverySnapshot finish() &&
    {
        return std::move(snapshot_);
    }
};
inline void descriptor(Fields &out, const std::string &name, const retail::SplineDescriptor &d)
{
    out.add(name + "/id", d.id);
    out.add(name + "/point-index", d.point_index);
    out.add(name + "/point-count", d.point_count);
    out.add(name + "/opaque", d.opaque);
    out.add(name + "/length", d.length);
    out.add(name + "/point-offset", static_cast<std::uint64_t>(d.point_offset));
}
} // namespace camera_witness_detail
inline CameraRecoverySnapshot capture_camera_recovery_state(const retail::RaceSessionState &s,
                                                            retail::RaceSessionReadiness readiness,
                                                            const CameraRecoveryStamp &stamp,
                                                            const std::optional<CapturedPolygonFrontRecipe> &front)
{
    using namespace camera_witness_detail;
    Fields out;
    out.add("front/present", front.has_value());
    if (front)
    {
        if (!front->submission)
            throw std::invalid_argument("camera witness front snapshot lacks submission");
        out.add("front/gamma-u16le", front->gamma);
        out.add("front/source-tick", front->submission->source_tick);
        out.add("front/width", front->width);
        out.add("front/height", front->height);
        out.add("front/commands", static_cast<std::uint64_t>(front->submission->commands));
        out.add("front/events", static_cast<std::uint64_t>(front->submission->events));
        out.address("front/logical-device-address", front->submission->device.get());
        out.add("front/segment-count", static_cast<std::uint64_t>(front->submission->segments.size()));
        for (std::size_t i = 0; i < front->submission->segments.size(); ++i)
        {
            const auto key = "front/segment/" + std::to_string(i) + "/";
            const auto &frame = front->submission->segments[i].device_frame;
            out.address(key + "frame-address", frame.get());
            if (frame)
            {
                out.add(key + "sequence-before", frame->initial.operation_sequence);
                out.add(key + "sequence-after", frame->final.operation_sequence);
            }
        }
        // Physical image IDs and new GPU texture owners are deliberately absent:
        // they are recreated. Their exact per-endpoint bundles remain separately
        // bound by recovery.json; no pixel equality is inferred from this witness.
    }
    out.add("host/clock-tick", stamp.host_clock_tick);
    out.add("host/completed-tick", stamp.completed_tick);
    out.add("host/driver-calls", stamp.driver_calls);
    out.add("host/camera-begin-calls", stamp.camera_begin_calls);
    out.add("host/source-captures", stamp.source_captures);
    out.add("source/simulation-tick", s.simulation_tick);
    out.add("source/frame-pause", s.frame_pause);
    out.add("session/readiness", static_cast<std::uint32_t>(readiness));
    out.address("session/state-address", &s);
    out.add("camera/current-controller", s.controls.current);
    out.add("camera/selected-preset", s.selected_camera_preset);
    out.add("camera/authored-cycle", s.camera_session.authored_cycle_index);
    out.add("camera/path-speed", s.camera_session.path_speed);
    out.add("camera/path-parameter", s.camera_session.path_parameter);
    out.add("camera/bank-count", s.camera_bank.count);
    out.address("camera/bank-address", s.camera_bank.records.data());
    for (std::size_t i = 0; i < s.camera_bank.records.size(); ++i)
    {
        const auto key = "camera/bank/" + std::to_string(i);
        const auto &r = s.camera_bank.records[i];
        out.add(key + "/position", r.position);
        out.add(key + "/target", r.target);
        out.add(key + "/up", r.up);
        out.add(key + "/lens", r.lens);
        out.add(key + "/tracking", r.tracking);
        out.add(key + "/twist", r.twist);
        out.add(key + "/opaque", r.opaque);
    }
    for (std::size_t i = 0; i < s.controls.controllers.size(); ++i)
    {
        const auto key = "camera/controller/" + std::to_string(i) + "/";
        const auto &controller = s.controls.controllers[i];
        const auto &c = controller.camera;
        const auto &life = s.camera_lifecycle[i];
        const auto &p = s.camera_presentation[i];
        out.address(key + "address", &controller);
        out.add(key + "active", controller.active);
        out.add(key + "vehicle", controller.vehicle);
        out.add(key + "mode", c.mode);
        out.add(key + "position", c.position);
        out.add(key + "target", c.target);
        out.add(key + "previous-target", c.previous_target);
        out.add(key + "up", c.up);
        out.add(key + "side", c.side);
        out.add(key + "velocity", c.velocity);
        out.add(key + "drift", c.drift);
        out.add(key + "stabilized-up", c.stabilized_up);
        out.add(key + "twist", c.twist);
        out.add(key + "lens", c.lens);
        out.add(key + "near-clip", c.near_clip);
        out.add(key + "far-factor", c.far_factor);
        out.add(key + "half-width", c.half_width);
        out.add(key + "half-height", c.half_height);
        out.add(key + "collision-bypass-ticks", c.collision_bypass_ticks);
        out.add(key + "settle-ticks", c.settle_ticks);
        out.add(key + "pending-lens", c.pending_lens);
        out.add(key + "speed-lift", c.speed_lift);
        out.add(key + "height-offset", c.height_offset);
        out.add(key + "requested-lens", c.requested_lens);
        out.add(key + "native-only/projection", c.projection);
        out.add(key + "native-only/camera-to-world-cache", c.camera_to_world_rotation);
        out.add(key + "native-only/world-to-camera-cache", c.world_to_camera_rotation);
        out.add(key + "camera-to-world-reference", c.camera_to_world_reference);
        out.add(key + "world-to-camera-reference", c.world_to_camera_reference);
        out.add(key + "native-only/inverse-depth-scale", c.inverse_depth_scale);
        out.add(key + "native-only/depth-offset", c.depth_offset);
        out.add(key + "linear-speed", c.linear_speed);
        out.add(key + "control-angles", c.control_angles);
        out.add(key + "angular-velocity", c.angular_velocity);
        out.add(key + "angular-acceleration", c.angular_acceleration);
        out.add(key + "pending-follow-setup", c.pending_follow_setup);
        out.add(key + "lifecycle/scale", life.scale);
        out.add(key + "lifecycle/transform48", life.transform_word_48);
        out.add(key + "lifecycle/transform54", life.transform_word);
        out.add(key + "lifecycle/clip-near", life.clip_near);
        out.add(key + "lifecycle/clip-far", life.clip_far);
        out.add(key + "lifecycle/base-lens", life.base_lens);
        out.add(key + "lifecycle/unknown1c8", life.unknown_1c8);
        out.add(key + "lifecycle/next-mode", life.next_mode);
        out.optional(key + "lifecycle/current-record", life.current_record);
        out.optional(key + "lifecycle/pending-record", life.pending_record);
        out.add(key + "lifecycle/pending-mode", life.pending_mode);
        out.add(key + "presentation/up-axis", p.up_axis);
        out.add(key + "presentation/forward-axis", p.forward_axis);
        out.add(key + "presentation/planes", p.planes);
        out.add(key + "spline/present", p.spline.has_value());
        if (p.spline)
        {
            const auto &v = *p.spline;
            descriptor(out, key + "spline/descriptor", v.descriptor);
            out.address(key + "spline/storage-address", v.storage.data());
            out.add(key + "spline/storage-size", static_cast<std::uint64_t>(v.storage.size()));
            out.add(key + "spline/point-storage-end", static_cast<std::uint64_t>(v.point_storage_end));
            const bool available = readiness == retail::RaceSessionReadiness::preview_ready;
            out.add(key + "spline/backing-observed", available);
            // Never call session.splines()/track_pmd() or materialize resources.
            // A source-only attachment records the borrowed identity/extent but
            // does not dereference its unavailable resource storage.
            if (available)
                out.bytes(key + "spline/backing", v.storage);
        }
    }
    out.add("camera/frame-projection", s.frame_projection);
    out.add("camera/frame-world-view", s.frame_world_view);
    out.add("camera/inverse-depth-scale", s.inverse_depth_scale);
    out.add("camera/depth-offset", s.depth_offset);
    out.add("camera/matrix-arena", s.view_matrices.matrices);
    out.address("camera/matrix-arena-address", s.view_matrices.matrices.data());
    out.optional("camera/matrix-cursor", s.view_matrices.cursor);
    out.add("camera/matrix-count", s.view_matrices.count);
    out.add("camera/shared-model-identity", s.shared_model_identity);
    out.address("camera/shared-model-identity-address", &s.shared_model_identity);
    const auto &v = s.viewport;
    out.add("viewport/width", v.width);
    out.add("viewport/height", v.height);
    out.add("viewport/center-pixel-x", v.center_pixel_x);
    out.add("viewport/center-pixel-y", v.center_pixel_y);
    out.add("viewport/center-x", v.center_x);
    out.add("viewport/center-y", v.center_y);
    out.add("viewport/scale-x", v.scale_x);
    out.add("viewport/scale-y", v.scale_y);
    out.add("viewport/right-edge", v.right_edge);
    out.add("viewport/bottom-edge", v.bottom_edge);
    out.add("viewport/min-x", v.min_x);
    out.add("viewport/min-y", v.min_y);
    out.add("viewport/max-x", v.max_x);
    out.add("viewport/max-y", v.max_y);
    out.add("viewport/depth-bits", v.depth_bits);
    out.add("viewport/bytes-per-pixel", v.bytes_per_pixel);
    out.add("viewport/full-width", v.full_width);
    out.add("viewport/full-height", v.full_height);
    out.add("viewport/cropped", v.cropped);
    out.add("viewport/selector", v.selector);
    out.add("viewport/split-setting", s.split_setting);
    out.add("viewport/origin-x", v.origin_x);
    out.add("viewport/origin-y", v.origin_y);
    out.add("viewport/clip-virtual", v.clip_virtual);
    out.add("viewport/clip-screen", v.clip_screen);
    out.add("viewport/clip-expanded", v.clip_expanded);
    out.add("viewport/applied-selector", v.applied_selector);
    out.add("viewport/requested-selector", v.requested_selector);
    out.add("viewport/ui-scale-x", v.ui_scale_x);
    out.add("viewport/ui-scale-y", v.ui_scale_y);
    out.add("quality/first", s.view_quality.first);
    out.add("quality/second", s.view_quality.second);
    out.add("quality/restriction", s.view_quality.legacy_driver_restriction);
    out.add("light/phase-fast", s.light_animation.phase_fast);
    out.add("light/phase-slow", s.light_animation.phase_slow);
    for (std::size_t i = 0; i < s.glare.elements.size(); ++i)
    {
        const auto &e = s.glare.elements[i];
        const auto key = "glare/" + std::to_string(i) + "/";
        out.add(key + "texture", e.texture);
        out.add(key + "displacement", e.displacement);
        out.add(key + "scale", e.scale);
        out.add(key + "step", e.step);
        out.add(key + "phase", e.phase);
        out.add(key + "color", e.color);
    }
    return std::move(out).finish();
}
inline std::vector<std::uint8_t> encode_camera_recovery_snapshot(const CameraRecoverySnapshot &snapshot)
{
    using camera_witness_detail::append;
    std::vector<std::uint8_t> bytes{'S', 'G', 'N', 'C', 'W', 'I', 'T', 0};
    append(bytes, std::uint32_t{1});
    if (snapshot.fields.size() > 2048)
        throw std::length_error("camera witness field count");
    append(bytes, static_cast<std::uint32_t>(snapshot.fields.size()));
    std::set<std::string> names;
    for (const auto &field : snapshot.fields)
    {
        if (field.name.empty() || field.name.size() > 512 || field.bytes.size() > 64u * 1024u * 1024u ||
            !names.insert(field.name).second)
            throw std::runtime_error("invalid camera witness field");
        append(bytes, static_cast<std::uint32_t>(field.name.size()));
        append(bytes, static_cast<std::uint64_t>(field.bytes.size()));
        bytes.insert(bytes.end(), field.name.begin(), field.name.end());
        bytes.insert(bytes.end(), field.bytes.begin(), field.bytes.end());
    }
    constexpr std::array<std::uint8_t, 8> footer{'E', 'N', 'D', 'C', 'W', 'I', 'T', 0};
    bytes.insert(bytes.end(), footer.begin(), footer.end());
    return bytes;
}
// One interval directory. Before is saved before suspension; after and the
// completion record are saved immediately after successful rebind, before ticks.
// Captured metadata can fail the explicit receiving run, never alter source state.
class CameraRecoveryInterval
{
    std::filesystem::path directory_;
    CameraRecoverySnapshot before_;
    WorldJoinFingerprint before_hash_;
    WorldJoinFingerprint front_before_hash_;
    std::uint64_t serial_{};
    bool begun_{}, finished_{};

  public:
    CameraRecoveryInterval(std::filesystem::path directory, std::uint64_t serial)
        : directory_(std::move(directory)), serial_(serial)
    {
        front_bundle_detail::fresh_directory(directory_);
    }
    const std::filesystem::path &directory() const noexcept
    {
        return directory_;
    }
    void begin(CameraRecoverySnapshot before)
    {
        if (begun_ || finished_)
            throw std::runtime_error("camera recovery interval already begun");
        before_ = std::move(before);
        front_before_hash_ = fingerprint_world_join_file(directory_ / "front-before" / "front.json");
        before_hash_ =
            front_bundle_detail::write_bytes(directory_ / "before.bin", encode_camera_recovery_snapshot(before_));
        begun_ = true;
    }
    void finish(const CameraRecoverySnapshot &after)
    {
        if (!begun_ || finished_)
            throw std::runtime_error("camera recovery interval not begun or already completed");
        const auto front_after_hash = fingerprint_world_join_file(directory_ / "front-after" / "front.json");
        const auto after_hash =
            front_bundle_detail::write_bytes(directory_ / "after.bin", encode_camera_recovery_snapshot(after));
        const bool equal = before_ == after;
        std::ostringstream record;
        record.imbue(std::locale::classic());
        record
            << "{\"schema\":\"sgn.camera-recovery-witness.v1\",\"complete\":true,\"same_process\":true,\"process_id\":"
            << GetCurrentProcessId() << ",\"recovery_serial\":" << serial_ << ",\"before\":";
        front_bundle_detail::fingerprint_json(record, "before.bin", before_hash_);
        record << ",\"after\":";
        front_bundle_detail::fingerprint_json(record, "after.bin", after_hash);
        record << ",\"front_before\":";
        front_bundle_detail::fingerprint_json(record, "front-before/front.json", front_before_hash_);
        record << ",\"front_after\":";
        front_bundle_detail::fingerprint_json(record, "front-after/front.json", front_after_hash);
        record << ",\"equal\":" << (equal ? "true" : "false") << ",\"retail_oracle_qualified\":false}\n";
        (void)front_bundle_detail::write_text(directory_ / "recovery.json", record.str());
        finished_ = true;
        if (!equal)
            throw std::runtime_error("camera fields changed within the fixed-extent recovery interval");
    }
};
} // namespace sgn::diagnostics
