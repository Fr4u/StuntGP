#include "sgn/source_race_ring_render.hpp"
#include "sgn/source_trail_packet_writer.hpp"
#include "sgn/race_session.hpp"
#include <bit>
#include <cstring>
#include <type_traits>
#include "sgn/source_ring_x87.hpp"
#include "sgn/source_screen_transport.hpp"
#include "sgn/host_profiler.hpp"
#include "sgn/host_wide_race_view.hpp"
#include "sgn/host_presentation.hpp"
#include <memory>

extern "C" std::uint32_t sgn_source_ring_nonzero_41f313(const float *) noexcept;
extern "C" std::uint32_t sgn_source_ring_depth_41ee19(const void *) noexcept;
extern "C" void sgn_source_ring_mul_float(const float *, const float *, float *) noexcept;
extern "C" std::uint32_t sgn_source_ring_mul_integer(const float *, const float *) noexcept;
extern "C" std::uint32_t sgn_source_ring_uint_alpha(const std::uint32_t *, const float *) noexcept;
struct SourceRingPointInputs
{
    const void *shape;
    const float *matrix;
    const void *point;
    const float *width;
};
struct SourceRingScreenInputs
{
    const void *profile;
    const void *point;
    const float *scale;
    void *frame;
};
static_assert(sizeof(SourceRingPointInputs) == 32 && offsetof(SourceRingPointInputs, width) == 24);
static_assert(sizeof(SourceRingScreenInputs) == 32 && offsetof(SourceRingScreenInputs, frame) == 24);
extern "C" void sgn_source_ring_world_point_41e792(const SourceRingPointInputs *, void *) noexcept;
extern "C" void sgn_source_ring_screen_head_41ee41(const SourceRingScreenInputs *) noexcept;

namespace sgn::retail
{
namespace
{
using W = std::uint32_t;
using I = std::int32_t;
static_assert(sizeof(contact::effects::ImpactParticle) == 0x440);
static_assert(offsetof(contact::effects::ImpactParticle, reserved) == 0x30);
static_assert(sizeof(TrailRenderProfile) == 0x110 && sizeof(SourceTrailPolygon) == 0xa0);
static_assert(sizeof(VehicleVector) == 16 && std::is_trivially_copyable_v<VehicleVector>);
static_assert(std::is_trivially_copyable_v<TrailRenderProfile>);
// R19 (perf_entities, NEW host representation only): opaque origins are
// immutable metadata {function,instruction,frame_offset}; nothing compares
// their identity ("distinct origins never imply unequal guest bits"). One
// owner-thread cache hands out the same content without a heap allocation per
// ring call. Content, lanes and literal bytes of every produced word are
// exactly those of SourceTransportWord::opaque(function,instruction,offset).
using Origin = SourceOpaqueOrigin; // R19 perf_transport: plain value origin, no allocation
Origin cached_origin(W function, W instruction, I offset)
{
    return Origin(SourceOpaqueWordOrigin{function, instruction, offset});
}
SourceTransportWord opaque_word(const Origin &origin)
{
    SourceTransportWord out{}; // literal bytes0, lanes0..3: as SourceTransportWord::opaque
    for (unsigned i = 0; i < 4; ++i)
    {
        out.bytes[i].origin = origin;
        out.bytes[i].lane = static_cast<std::uint8_t>(i);
    }
    return out;
}
// A register/stack DWORD value inside one LocalFrame: either known bits or the
// whole opaque word whose origin the frame holds at index `origin`.
struct FrameWord
{
    W value{};
    std::int16_t origin{-1};
    static FrameWord known(W v)
    {
        return {v, -1};
    }
};
template <std::size_t Size> class LocalFrame
{
    // Backing bytes have no source value unless a reached writer marks them.
    // Zero initialization is host storage only and can NEVER satisfy a read.
    // tag: unset (never produced, never read), known, or opaque carrying the
    // origin first created for slot `tag` (copies share it, as before).
    static constexpr std::size_t count = Size / 4;
    static constexpr std::int16_t unset = -2, known_tag = -1;
    std::array<W, count> words_{};
    std::array<std::int16_t, count> tag_;
    std::array<Origin, count> origins_{};
    const W function_;
    static std::size_t index(I offset)
    {
        const auto at = static_cast<std::int64_t>(Size) + offset;
        if (at < 0 || at >= static_cast<std::int64_t>(Size) || at % 4)
            throw SourceAllocationError(0x41f2f0, "ring local offset outside actual source frame");
        return static_cast<std::size_t>(at) / 4;
    }

  public:
    explicit LocalFrame(W function) : function_(function)
    {
        tag_.fill(unset);
    }
    void set(I off, W value)
    {
        const auto i = index(off);
        words_[i] = value;
        tag_[i] = known_tag;
    }
    // Opaque copy keeps the old backing DWORD, exactly as before (never read).
    void set(I off, FrameWord value)
    {
        if (value.origin < 0)
            set(off, value.value);
        else
            tag_[index(off)] = value.origin;
    }
    FrameWord transport(I off, W site)
    {
        const auto i = index(off);
        const auto tag = tag_[i];
        if (tag == known_tag)
            return FrameWord::known(words_[i]);
        if (tag != unset)
            return {0, tag};
        origins_[i] = cached_origin(function_, site, off); // first unproduced read names this slot
        tag_[i] = static_cast<std::int16_t>(i);
        return {0, tag_[i]};
    }
    SourceTransportWord source(FrameWord value) const
    {
        if (value.origin < 0)
            return SourceTransportWord::known(value.value);
        return opaque_word(origins_[static_cast<std::size_t>(value.origin)]);
    }
    void *output(I off)
    {
        return &words_[index(off)];
    }
    void *ebp()
    {
        return words_.data() + words_.size();
    }
    void mark(I off, unsigned n)
    {
        for (unsigned i = 0; i < n; ++i)
            tag_[index(off + static_cast<I>(4 * i))] = known_tag;
    }
    W get(I off, W site)
    {
        const auto value = transport(off, site);
        if (value.origin >= 0)
            throw SourceOpaqueRead(site, origins_[static_cast<std::size_t>(value.origin)]);
        return value.value;
    }
};
class Profile
{
    RaceSessionState &w_;
    W base_;

  public:
    Profile(RaceSessionState &w, W type) : w_(w), base_(type * 0x110u) {} // 4160D4..DE, modulo32
    W word(W offset, W site) const
    {
        const W at = base_ + offset;
        auto bytes = std::as_bytes(std::span{w_.trail_profiles.profiles});
        if (at > bytes.size() || 4u > bytes.size() - at)
            throw SourceAllocationError(site, "source trail profile read leaves actual nine-row bank");
        W value;
        std::memcpy(&value, bytes.data() + at, 4);
        return value;
    }
    float real(W offset, W site) const
    {
        return std::bit_cast<float>(word(offset, site));
    }
    const void *prefix(W bytes, W site) const
    {
        const auto all = std::as_bytes(std::span{w_.trail_profiles.profiles});
        if (base_ > all.size() || bytes > all.size() - base_)
            throw SourceAllocationError(site, "source trail profile prefix leaves actual owner");
        return all.data() + base_;
    }
};
class Ring
{
    // Invocation-only bytes of the actual retained particle/vehicle array.
    // The two constructors are private-module bindings, never a public
    // caller-supplied span or a 64-point impact proxy for a smaller ring.
    std::span<const std::byte> bytes_;

  public:
    explicit Ring(const contact::effects::ImpactParticle &p) : bytes_(std::as_bytes(std::span{p.reserved})) {}
    explicit Ring(std::span<const VehicleVector> actual) : bytes_(std::as_bytes(actual)) {}
    const void *point(W index, W bytes, W site) const
    {
        const W at = index << 4u; // literal SHL32; retain the true owner's extent
        if (at > bytes_.size() || bytes > bytes_.size() - at)
            throw SourceAllocationError(site, "reached trail point leaves actual captured ring owner");
        return bytes_.data() + at;
    }
    W word(W index, unsigned lane, W site) const
    {
        const auto *bytes = static_cast<const std::byte *>(point(index, 4u * (lane + 1u), site));
        W value;
        std::memcpy(&value, bytes + 4u * lane, 4);
        return value;
    }
};
void staging(RaceSessionState &w, W offset, const SourceTransportWord &value)
{
    w.trail_staging.store(offset, value, 0x41f2f0);
}
void staging(RaceSessionState &w, W offset, W value)
{
    w.trail_staging.store(offset, value, 0x41f2f0);
}
float multiply(float a, float b)
{
    float value;
    sgn_source_ring_mul_float(&a, &b, &value);
    return value;
}
W alpha(float a, float b)
{
    return sgn_source_ring_mul_integer(&a, &b);
}
bool full(const SourceSpritePacketStorage &pool)
{
    const auto &a = pool.current_headers.source_binding();
    const auto &b = pool.headers.source().source_binding();
    if (a.bank != b.bank || a.allocation != b.allocation || a.bytes != b.bytes || a.byte_offset < b.byte_offset ||
        (a.byte_offset - b.byte_offset) % 8u || a.platform.owner_before(b.platform) ||
        b.platform.owner_before(a.platform))
        throw SourceAllocationError(0x434403, "source trail header cursor needs common actual owner");
    return (a.byte_offset - b.byte_offset) / 8u >= 512u;
}
LightCameraView camera(RaceSessionState &w)
{
    const auto i = w.controls.current;
    if (i >= w.controls.controllers.size())
        throw SourceAllocationError(0x4280a6, "trail current camera leaves actual bank");
    return {w.controls.controllers[i].camera,
            w.camera_lifecycle[i],
            w.camera_presentation[i],
            w.overlays[i],
            &w.view_matrices,
            &w.shared_model_identity};
}
void world_head(RaceSessionState &w, const Profile &p, const Ring &ring, float scale, float opacity)
{
    const auto x = p.real(0x100, 0x41f32d);
    const auto color = p.word(0xfc, 0x41f333);
    staging(w, 0x48, color);
    staging(w, 0x64, color);
    staging(w, 0x80, color);
    staging(w, 0x9c, color);
    const auto capacity = p.word(4, 0x41f354);
    staging(w, 0x10, std::bit_cast<W>(multiply(scale, x))); // 41F357
    const auto y = p.real(0x104, 0x41f360);
    const auto first = ring.word(capacity, 0, 0x41f36b);
    staging(w, 0x14, std::bit_cast<W>(multiply(scale, y)));
    const auto second = ring.word(capacity, 1, 0x41f373);
    staging(w, 0, first);
    const auto third = ring.word(capacity, 2, 0x41f37f);
    const auto z = p.real(0x108, 0x41f388);
    staging(w, 4, second);
    const auto fourth = ring.word(capacity, 3, 0x41f38e);
    const auto head_alpha = p.word(0xf8, 0x41f391);
    staging(w, 8, third);
    staging(w, 12, fourth);
    staging(w, 0x18, std::bit_cast<W>(multiply(scale, z)));
    const auto a = sgn_source_ring_uint_alpha(&head_alpha, &opacity);
    const auto texture = p.word(0xf4, 0x41f3b7), flags = p.word(0xf0, 0x41f3bd);
    staging(w, 0x20, texture);
    staging(w, 0x28, a);
    staging(w, 0x24, flags);
    // NEW HOST (R28): the head billboard moves with the ring owner (interpolation tag, read-only).
    host_presentation::EffectSpriteScope effect(w.sprite_packets, host_presentation::ring_head_effect());
    if (!full(w.sprite_packets))
        (void)append_source_trail_billboard_4343f0(w.sprite_packets, w.trail_staging, camera(w));
}
// 4255E0 overwrites reached outcodes; the whole copied payload retains validity.
template <std::size_t N> void copy_screen(RaceSessionState &w, LocalFrame<N> &frame, I base)
{
    SGN_PROFILE_SCOPE("entities.ring.copy_screen_425838");
    const auto index = w.screen_polygon_count; // 425830 BEFORE publication
    if (w.screen_base.is_null())
        throw SourceAllocationError(0x425838, "source screen base is null");
    if (!w.screen_polygons.source_bound())
        throw SourceAllocationError(0x425838, "source screen allocation required");
    if (const auto *wide = host_wide::race_view())
    {
        // NEW R20 HOST (wide_3d) Hor+: screen-space trail polygons of the 3D view
        //  are clipped (4255E0, 576C04..30) to the widened view instead of the
        //  4:3 frame. Call-local viewport copy; ViewportState is not written.
        auto view = w.viewport;
        for (unsigned i = 0; i < 4; ++i)
        {
            view.clip_screen[i] = wide->clip_screen[i];
            view.clip_expanded[i] = wide->clip_expanded[i];
        }
        (void)append_source_screen_captured_425838(
            w.screen_polygons.source_binding(), index, w.screen_polygon_count,
            [&](W off, W site) { return frame.source(frame.transport(base + static_cast<I>(off), site)); }, view);
        return;
    }
    (void)append_source_screen_captured_425838(
        w.screen_polygons.source_binding(), index, w.screen_polygon_count,
        [&](W off, W site) { return frame.source(frame.transport(base + static_cast<I>(off), site)); }, w.viewport);
}
void screen_head(RaceSessionState &w, const Profile &p, const Ring &ring, W cursor, float scale)
{
    const auto *point = ring.point(cursor, 12, 0x41ee19);
    if (!sgn_source_ring_depth_41ee19(point))
        return;
    LocalFrame<0x114> frame(0x41ee10);
    // Exact fields in the local polygon. Unknown W/outcodes/unused tail are
    // not assigned a source value by the allocator or these publications.
    frame.set(-0x108, 4);
    frame.set(-0x110, p.word(0xf0, 0x41ee4a));
    frame.set(-0x10c, p.word(0xf8, 0x41ee60));
    frame.set(-0x114, p.word(0xf4, 0x41ee6f));
    const auto depth = ring.word(cursor, 2, 0x41ee7b);
    frame.set(-0xfc, depth);
    frame.set(-0xdc, depth);
    frame.set(-0xf4, 0);
    frame.set(-0xf0, 0);
    frame.set(-0xd4, 0x3f800000);
    frame.set(-0xd0, 0);
    const auto color = p.word(0xfc, 0x41eec4);
    frame.set(-0xb4, 0x3f800000);
    frame.set(-0xec, color);
    frame.set(-0xcc, color);
    frame.set(-0xac, color);
    frame.set(-0x8c, color);
    frame.set(-0xb0, 0x3f800000);
    frame.set(-0x94, 0);
    frame.set(-0x90, 0x3f800000);
    frame.set(-0xbc, depth);
    frame.set(-0x9c, depth);
    const SourceRingScreenInputs input{p.prefix(0x10c, 0x41ee44), point, &scale, frame.ebp()};
    sgn_source_ring_screen_head_41ee41(&input);
    for (const I off : {-0x104, -0x100, -0xe4, -0xe0, -0xc4, -0xc0, -0xa4, -0xa0, -4})
        frame.mark(off, 1);
    copy_screen(w, frame, -0x114); // 41EF80 ->425830 ->4255E0
}
using Op = RingX87Op;
void normalize_screen(LocalFrame<0x188> &f, RingX87 &fp)
{
    fp.op(Op::fld32, f.get(-0xc, 0x44eda0));
    fp.op(Op::fld32, f.get(-0x10, 0x44eda3));
    fp.op(Op::fld32, f.get(-0x14, 0x44eda6));
    fp.op(Op::fld0);
    fp.op(Op::mul1);
    fp.op(Op::fld2);
    fp.op(Op::mul3);
    fp.op(Op::addp);
    fp.op(Op::fld3);
    fp.op(Op::mul4);
    fp.op(Op::addp);
    fp.op(Op::pop3);
    fp.op(Op::pop0);
    fp.op(Op::pop0);
    fp.op(Op::fcom32, 0);
    if (fp.op(Op::sw) & 0x4000u)
    {
        fp.op(Op::pop0);
        f.set(-0x28, 0);
        f.set(-0xc, 0);
        f.set(-0x10, 0);
        f.set(-0x14, 0);
        return;
    }
    fp.op(Op::sqrt);
    f.set(-0x28, fp.op(Op::fst32));
    fp.op(Op::fdivr32, 0x3f800000);
    fp.op(Op::fld0);
    fp.op(Op::fmul32, f.get(-0x14, 0x44ede6));
    f.set(-0x14, fp.op(Op::fstp32));
    fp.op(Op::fld0);
    fp.op(Op::fmul32, f.get(-0x10, 0x44edec));
    f.set(-0x10, fp.op(Op::fstp32));
    fp.op(Op::fmul32, f.get(-0xc, 0x44edf2));
    f.set(-0xc, fp.op(Op::fstp32));
}
void screen_trail(RaceSessionState &w, const Profile &p, const Ring &ring, W cursor, W count, float scale,
                  float opacity)
{
    SGN_PROFILE_SCOPE("entities.ring.screen_trail_41ef90");
    if (p.word(0xec, 0x41efa0) == 1)
        screen_head(w, p, ring, cursor, scale);
    LocalFrame<0x188> frame(0x41ef90);
    RingX87 fp;
    frame.set(-0x184, p.word(0x14, 0x41efc8));
    frame.set(-0x188, p.word(0x18, 0x41efcb));
    fp.op(Op::fld32, std::bit_cast<W>(scale));
    frame.set(-0x24, ring.word(cursor, 0, 0x41efe2));
    frame.set(-0x17c, 4);
    const auto first_y = ring.word(cursor, 1, 0x41eff3);
    frame.set(-0x5c, 0);
    fp.op(Op::fmul32, p.word(0x2c, 0x41eff9));
    const auto first_z = ring.word(cursor, 2, 0x41effc);
    frame.set(-0x20, first_y);
    frame.set(-0x1c, first_z);
    frame.set(-0x18, ring.word(cursor, 3, 0x41f005));
    if (p.word(0xc, 0x41f00b) == 0)
    {
        fp.op(Op::pop0);
        return;
    }
    for (W segment = 0;; ++segment)
    {
        if (segment)
            fp.op(Op::fld32, frame.get(-0x60, 0x41f021));
        fp.op(Op::fld32, std::bit_cast<W>(scale));
        fp.op(Op::fmul32, p.word(0x30 + segment * 4u, 0x41f027));
        frame.set(-0x60, fp.op(Op::fstp32));
        fp.op(Op::fld32, std::bit_cast<W>(opacity));
        fp.op(Op::fmul32, p.word(0x6c + segment * 4u, 0x41f030));
        const auto alpha_value = fp.ftol();
        const auto before = frame.get(-0x5c, 0x41f039);
        frame.set(-0x180, alpha_value);
        const auto stride = p.word(8, 0x41f045);
        const auto distance = before + stride;
        frame.set(-0x5c, distance);
        if (distance >= count)
        {
            fp.op(Op::pop0);
            return;
        }
        cursor -= stride;
        if (std::bit_cast<I>(cursor) < 0)
            cursor += p.word(4, 0x41f05f);
        const auto old_x = frame.transport(-0x24, 0x41f065), old_y = frame.transport(-0x20, 0x41f068);
        frame.set(-0x38, old_x);
        frame.set(-0x30, frame.transport(-0x1c, 0x41f06e));
        frame.set(-0x34, old_y);
        const auto old_w = frame.transport(-0x18, 0x41f07f);
        const auto x = ring.word(cursor, 0, 0x41f082);
        frame.set(-0x2c, old_w);
        frame.set(-0x24, x);
        frame.set(-0x20, ring.word(cursor, 1, 0x41f08a));
        const auto z = ring.word(cursor, 2, 0x41f090), rw = ring.word(cursor, 3, 0x41f095);
        frame.set(-0x1c, z);
        frame.set(-0x18, rw);
        FrameWord eax{}, ebx{}, ecx{}, edx{};
        if (segment == 0)
        {
            fp.op(Op::fld32, frame.get(-28, 0x41f0a0)); // 41F0A0
            fp.op(Op::mul1);                            // 41F0A3
            // Bound local pointer only; actual frame passed to child. //41F0A5
            // Bound local pointer only; actual frame passed to child. //41F0A8
            frame.set(-12, segment);                     // 41F0AB
            fp.op(Op::fmul32, 0x42c80000u);              // 41F0AE
            frame.set(-4, fp.op(Op::fstp32));            // 41F0B4
            fp.op(Op::pop0);                             // 41F0B7
            fp.op(Op::fld32, frame.get(-36, 0x41f0b9));  // 41F0B9
            fp.op(Op::fsub32, frame.get(-56, 0x41f0bc)); // 41F0BC
            frame.set(-20, fp.op(Op::fstp32));           // 41F0BF
            fp.op(Op::fld32, frame.get(-32, 0x41f0c2));  // 41F0C2
            fp.op(Op::fsub32, frame.get(-52, 0x41f0c5)); // 41F0C5
            frame.set(-16, fp.op(Op::fstp32));           // 41F0C8
            normalize_screen(frame, fp);                 // 41F0CB
            ecx = frame.transport(-20, 0x41f0d0);        // 41F0D0
            edx = frame.transport(-48, 0x41f0d3);        // 41F0D3
            fp.op(Op::fld32, frame.get(-16, 0x41f0d6));  // 41F0D6
            frame.set(-40, ecx);                         // 41F0D9
            eax = edx;                                   // 41F0DC
            fp.op(Op::fld32, frame.get(-40, 0x41f0de));  // 41F0DE
            fp.op(Op::chs);                              // 41F0E1
            frame.set(-16, fp.op(Op::fstp32));           // 41F0E3
            frame.set(-80, edx);                         // 41F0E6
            frame.set(-64, eax);                         // 41F0E9
            fp.op(Op::fmul32, frame.get(-4, 0x41f0ec));  // 41F0EC
            fp.op(Op::fld32, frame.get(-16, 0x41f0ef));  // 41F0EF
            fp.op(Op::fmul32, frame.get(-4, 0x41f0f2));  // 41F0F2
            frame.set(-16, fp.op(Op::fstp32));           // 41F0F5
            fp.op(Op::fld32, frame.get(-56, 0x41f0f8));  // 41F0F8
            fp.op(Op::sub1);                             // 41F0FB
            frame.set(-88, fp.op(Op::fstp32));           // 41F0FD
            fp.op(Op::fld32, frame.get(-52, 0x41f100));  // 41F100
            fp.op(Op::fsub32, frame.get(-16, 0x41f103)); // 41F103
            frame.set(-84, fp.op(Op::fstp32));           // 41F106
            fp.op(Op::fadd32, frame.get(-56, 0x41f109)); // 41F109
            frame.set(-72, fp.op(Op::fstp32));           // 41F10C
            fp.op(Op::fld32, frame.get(-16, 0x41f10f));  // 41F10F
            fp.op(Op::fadd32, frame.get(-52, 0x41f112)); // 41F112
            frame.set(-68, fp.op(Op::fstp32));           // 41F115
        }
        else
        {
            fp.op(Op::pop0);
        }
        fp.op(Op::fld32, frame.get(-28, 0x41f11c));  // 41F11C
        fp.op(Op::fmul32, frame.get(-96, 0x41f11f)); // 41F11F
        // Bound local pointer only; actual frame passed to child. //41F122
        // Bound local pointer only; actual frame passed to child. //41F125
        frame.set(-12, W{0});                                          // 41F128
        fp.op(Op::fmul32, 0x42c80000u);                                // 41F12F
        frame.set(-4, fp.op(Op::fstp32));                              // 41F135
        fp.op(Op::fld32, frame.get(-36, 0x41f138));                    // 41F138
        fp.op(Op::fsub32, frame.get(-56, 0x41f13b));                   // 41F13B
        frame.set(-20, fp.op(Op::fstp32));                             // 41F13E
        fp.op(Op::fld32, frame.get(-32, 0x41f141));                    // 41F141
        fp.op(Op::fsub32, frame.get(-52, 0x41f144));                   // 41F144
        frame.set(-16, fp.op(Op::fstp32));                             // 41F147
        normalize_screen(frame, fp);                                   // 41F14A
        ecx = frame.transport(-20, 0x41f14f);                          // 41F14F
        eax = frame.transport(-88, 0x41f152);                          // 41F152
        fp.op(Op::fld32, frame.get(-16, 0x41f155));                    // 41F155
        frame.set(-40, ecx);                                           // 41F158
        edx = frame.transport(-80, 0x41f15b);                          // 41F15B
        fp.op(Op::fld32, frame.get(-40, 0x41f15e));                    // 41F15E
        fp.op(Op::chs);                                                // 41F161
        frame.set(-16, fp.op(Op::fstp32));                             // 41F163
        ecx = frame.transport(-84, 0x41f166);                          // 41F166
        ebx = frame.transport(-72, 0x41f169);                          // 41F169
        fp.op(Op::fmul32, frame.get(-4, 0x41f16c));                    // 41F16C
        frame.set(-368, edx);                                          // 41F16F
        edx = FrameWord::known(p.word(0x1c, 0x41f175));                // 41F175
        frame.set(-120, ebx);                                          // 41F178
        ebx = frame.transport(-68, 0x41f17b);                          // 41F17B
        frame.set(-360, edx);                                          // 41F17E
        frame.set(-116, ebx);                                          // 41F184
        frame.set(-20, fp.op(Op::fstp32));                             // 41F187
        fp.op(Op::fld32, frame.get(-16, 0x41f18a));                    // 41F18A
        fp.op(Op::fmul32, frame.get(-4, 0x41f18d));                    // 41F18D
        ebx = frame.transport(-64, 0x41f190);                          // 41F190
        frame.set(-372, ecx);                                          // 41F193
        ecx = frame.transport(-76, 0x41f199);                          // 41F199
        frame.set(-112, ebx);                                          // 41F19C
        ebx = frame.transport(-28, 0x41f19f);                          // 41F19F
        frame.set(-376, eax);                                          // 41F1A2
        frame.set(-16, fp.op(Op::fstp32));                             // 41F1A8
        fp.op(Op::fld32, frame.get(-12, 0x41f1ab));                    // 41F1AB
        fp.op(Op::fmul32, frame.get(-4, 0x41f1ae));                    // 41F1AE
        eax = ecx;                                                     // 41F1B1
        frame.set(-80, ebx);                                           // 41F1B3
        frame.set(-64, ebx);                                           // 41F1B6
        frame.set(-364, eax);                                          // 41F1B9
        eax = FrameWord::known(p.word(0x20, 0x41f1bf));                // 41F1BF
        frame.set(-332, ecx);                                          // 41F1C2
        frame.set(-12, fp.op(Op::fstp32));                             // 41F1C8
        fp.op(Op::fld32, frame.get(-36, 0x41f1cb));                    // 41F1CB
        fp.op(Op::fsub32, frame.get(-20, 0x41f1ce));                   // 41F1CE
        ecx = FrameWord::known(p.word(0x24, 0x41f1d1));                // 41F1D1
        frame.set(-356, eax);                                          // 41F1D4
        eax = FrameWord::known(p.word(0xac + segment * 4u, 0x41f1da)); // 41F1DA
        frame.set(-328, ecx);                                          // 41F1E1
        frame.set(-352, eax);                                          // 41F1E7
        frame.set(-88, fp.op(Op::fstp32));                             // 41F1ED
        fp.op(Op::fld32, frame.get(-32, 0x41f1f0));                    // 41F1F0
        fp.op(Op::fsub32, frame.get(-16, 0x41f1f3));                   // 41F1F3
        edx = frame.transport(-88, 0x41f1f6);                          // 41F1F6
        frame.set(-344, edx);                                          // 41F1F9
        frame.set(-84, fp.op(Op::fstp32));                             // 41F1FF
        fp.op(Op::fld32, frame.get(-20, 0x41f202));                    // 41F202
        fp.op(Op::fadd32, frame.get(-36, 0x41f205));                   // 41F205
        edx = frame.transport(-84, 0x41f208);                          // 41F208
        frame.set(-340, edx);                                          // 41F20B
        edx = ebx;                                                     // 41F211
        // Captured profile color pointer is p+0xB0+4*segment. //41F213
        frame.set(-336, edx);                                          // 41F216
        frame.set(-72, fp.op(Op::fstp32));                             // 41F21C
        fp.op(Op::fld32, frame.get(-16, 0x41f21f));                    // 41F21F
        fp.op(Op::fadd32, frame.get(-32, 0x41f222));                   // 41F222
        edx = FrameWord::known(p.word(0x20, 0x41f225));                // 41F225
        ecx = FrameWord::known(p.word(0xb0 + segment * 4u, 0x41f228)); // 41F228
        frame.set(-324, edx);                                          // 41F22A
        edx = frame.transport(-72, 0x41f230);                          // 41F230
        frame.set(-312, edx);                                          // 41F233
        frame.set(-320, ecx);                                          // 41F239
        frame.set(-68, fp.op(Op::fstp32));                             // 41F23F
        edx = frame.transport(-68, 0x41f242);                          // 41F242
        frame.set(-308, edx);                                          // 41F245
        edx = frame.transport(-64, 0x41f24b);                          // 41F24B
        frame.set(-304, edx);                                          // 41F24E
        edx = frame.transport(-60, 0x41f254);                          // 41F254
        frame.set(-288, ecx);                                          // 41F257
        ecx = frame.transport(-120, 0x41f25d);                         // 41F25D
        frame.set(-300, edx);                                          // 41F260
        edx = FrameWord::known(p.word(0x24, 0x41f266));                // 41F266
        frame.set(-280, ecx);                                          // 41F269
        ecx = frame.transport(-112, 0x41f26f);                         // 41F26F
        frame.set(-296, edx);                                          // 41F272
        edx = FrameWord::known(p.word(0x28, 0x41f278));                // 41F278
        frame.set(-272, ecx);                                          // 41F27B
        ecx = frame.transport(-60, 0x41f281);                          // 41F281
        frame.set(-292, edx);                                          // 41F284
        edx = frame.transport(-116, 0x41f28a);                         // 41F28A
        frame.set(-268, ecx);                                          // 41F28D
        ecx = FrameWord::known(p.word(0x28, 0x41f293));                // 41F293
        frame.set(-276, edx);                                          // 41F296
        edx = FrameWord::known(p.word(0x1c, 0x41f29c));                // 41F29C
        frame.set(-260, ecx);                                          // 41F29F
        // Bound local pointer only; actual frame passed to child. //41F2A5
        frame.set(-264, edx); // 41F2AB
        frame.set(-256, eax); // 41F2B1
        fp.publish();
        try
        {
            copy_screen(w, frame, -0x188);
            fp.recapture();
        }
        catch (...)
        {
            fp.recapture();
            throw;
        }
        const auto limit = p.word(0xc, 0x41f2bc);
        if (segment + 1u >= limit)
            return;
    }
}
void world_segment(RaceSessionState &w, const Profile &p, const Ring &ring, W previous, W next_point, float old_width,
                   float width, W segment, W mode)
{
    SGN_PROFILE_SCOPE("entities.ring.world_segment_41e760");
    const auto identity = p.word(0x10, mode ? 0x41ea2c : 0x41e772);
    const TrailShape *shape = nullptr;
    for (const auto &candidate : w.trail_profiles.shapes)
        if (candidate.identity == identity)
        {
            shape = &candidate;
            break;
        }
    if (!shape)
        throw SourceAllocationError(mode ? 0x41ea4f : 0x41e795,
                                    "source trail shape pointer is not a represented shape owner");
    const auto slot = w.current_slot;
    if (slot >= w.vehicles.physics.size())
        throw SourceAllocationError(mode ? 0x41ea35 : 0x41e778, "source current vehicle leaves actual bank");
    const auto &matrix = w.vehicles.physics[slot].orientation;
    LocalFrame<0x98> frame(mode ? 0x41ea20 : 0x41e760);
    for (unsigned i = 0; i < 4; ++i)
    {
        const auto *old_point = ring.point(previous, 12, mode ? 0x41eabb : 0x41e804);
        const SourceRingPointInputs first{&shape->points[i], matrix.data(), old_point, &old_width};
        const I first_offset = (mode ? -0x98 : -0x58) + static_cast<I>(i * 16);
        sgn_source_ring_world_point_41e792(&first, frame.output(first_offset));
        frame.mark(first_offset, 3);
        const auto *fresh_point = ring.point(next_point, 12, mode ? 0x41eb46 : 0x41e88b);
        const SourceRingPointInputs second{&shape->points[i], matrix.data(), fresh_point, &width};
        const I second_offset = (mode ? -0x40 : -0x98) + static_cast<I>(i * 16);
        sgn_source_ring_world_point_41e792(&second, frame.output(second_offset));
        frame.mark(second_offset, 3);
    }
    // NEW HOST (R28): the newest quad (segment 0) stretches from the moving owner to
    // the older ring point (interpolation tag; both points were read above).
    host_presentation::EffectSpriteScope effect(
        w.sprite_packets,
        host_presentation::ring_segment_effect(segment, ring.point(previous, 12, mode ? 0x41eabb : 0x41e804),
                                               ring.point(next_point, 12, mode ? 0x41eb46 : 0x41e88b)));
    FrameWord eax{}, ecx{}, edx{}, ebx{}, esi{}, edi{};
    // Staging receives the same known bits or the same opaque word as before.
    auto put = [&](W offset, FrameWord value) {
        if (value.origin < 0)
            staging(w, offset, value.value);
        else
            staging(w, offset, frame.source(value));
    };
    if (mode == 0)
    {
        eax = FrameWord::known(p.word(0xac + segment * 4u, 0x41e8ba));            // 41E8BA
        put(0x48, eax);                                                           // 41E8C1
        put(0x64, eax);                                                           // 41E8C6
        eax = FrameWord::known(p.word(0xb0 + segment * 4u, 0x41e8cb));            // 41E8CB
        edx = frame.transport(-88, 0x41e8d2);                                     // 41E8D2
        ecx = frame.transport(-80, 0x41e8d5);                                     // 41E8D5
        put(0x80, eax);                                                           // 41E8D8
        put(0x9c, eax);                                                           // 41E8DD
        eax = frame.transport(-84, 0x41e8e2);                                     // 41E8E2
        put(0x30, edx);                                                           // 41E8E5
        edx = frame.transport(-76, 0x41e8eb);                                     // 41E8EB
        put(0x38, ecx);                                                           // 41E8EE
        ecx = frame.transport(-68, 0x41e8f4);                                     // 41E8F4
        put(0x34, eax);                                                           // 41E8F7
        eax = frame.transport(-72, 0x41e8fc);                                     // 41E8FC
        put(0x3c, edx);                                                           // 41E8FF
        edx = frame.transport(-64, 0x41e905);                                     // 41E905
        put(0x50, ecx);                                                           // 41E908
        ecx = frame.transport(-136, 0x41e90e);                                    // 41E90E
        put(0x4c, eax);                                                           // 41E914
        eax = frame.transport(-60, 0x41e919);                                     // 41E919
        put(0x54, edx);                                                           // 41E91C
        edx = frame.transport(-132, 0x41e922);                                    // 41E922
        put(0x68, ecx);                                                           // 41E928
        ecx = frame.transport(-124, 0x41e92e);                                    // 41E92E
        put(0x58, eax);                                                           // 41E931
        eax = frame.transport(-128, 0x41e936);                                    // 41E936
        put(0x6c, edx);                                                           // 41E939
        edx = frame.transport(-152, 0x41e93f);                                    // 41E93F
        put(0x74, ecx);                                                           // 41E945
        ecx = frame.transport(-144, 0x41e94b);                                    // 41E94B
        put(0x70, eax);                                                           // 41E951
        eax = frame.transport(-148, 0x41e956);                                    // 41E956
        put(0x84, edx);                                                           // 41E95C
        edx = frame.transport(-140, 0x41e962);                                    // 41E962
        put(0x8c, ecx);                                                           // 41E968
        ecx = FrameWord::known(0x571330);                                         // 41E96E
        put(0x88, eax);                                                           // 41E973
        put(0x90, edx);                                                           // 41E978
        (void)append_source_trail_quad_4342e0(w.sprite_packets, w.trail_staging); // 41E97E
        eax = frame.transport(-56, 0x41e983);                                     // 41E983
        ecx = frame.transport(-52, 0x41e986);                                     // 41E986
        edx = frame.transport(-48, 0x41e989);                                     // 41E989
        put(0x30, eax);                                                           // 41E98C
        eax = frame.transport(-44, 0x41e991);                                     // 41E991
        put(0x34, ecx);                                                           // 41E994
        ecx = frame.transport(-40, 0x41e99a);                                     // 41E99A
        put(0x38, edx);                                                           // 41E99D
        edx = frame.transport(-36, 0x41e9a3);                                     // 41E9A3
        put(0x3c, eax);                                                           // 41E9A6
        eax = frame.transport(-32, 0x41e9ab);                                     // 41E9AB
        put(0x4c, ecx);                                                           // 41E9AE
        ecx = frame.transport(-28, 0x41e9b4);                                     // 41E9B4
        put(0x50, edx);                                                           // 41E9B7
        edx = frame.transport(-104, 0x41e9bd);                                    // 41E9BD
        put(0x54, eax);                                                           // 41E9C0
        eax = frame.transport(-100, 0x41e9c5);                                    // 41E9C5
        put(0x58, ecx);                                                           // 41E9C8
        ecx = frame.transport(-96, 0x41e9ce);                                     // 41E9CE
        put(0x68, edx);                                                           // 41E9D1
        edx = frame.transport(-92, 0x41e9d7);                                     // 41E9D7
        put(0x6c, eax);                                                           // 41E9DA
        eax = frame.transport(-120, 0x41e9df);                                    // 41E9DF
        put(0x70, ecx);                                                           // 41E9E2
        ecx = frame.transport(-116, 0x41e9e8);                                    // 41E9E8
        put(0x74, edx);                                                           // 41E9EB
        edx = frame.transport(-112, 0x41e9f1);                                    // 41E9F1
        put(0x84, eax);                                                           // 41E9F4
        eax = frame.transport(-108, 0x41e9f9);                                    // 41E9F9
        put(0x88, ecx);                                                           // 41E9FC
        ecx = FrameWord::known(0x571330);                                         // 41EA02
        put(0x8c, edx);                                                           // 41EA07
        put(0x90, eax);                                                           // 41EA0D
        (void)append_source_trail_quad_4342e0(w.sprite_packets, w.trail_staging); // 41EA12
    }
    else
    {
        esi = frame.transport(-136, 0x41eb71);                                    // 41EB71
        edi = frame.transport(-132, 0x41eb77);                                    // 41EB77
        ebx = frame.transport(-128, 0x41eb7d);                                    // 41EB7D
        eax = FrameWord::known(p.word(0xac + segment * 4u, 0x41eb80));            // 41EB80
        put(0x30, esi);                                                           // 41EB87
        put(0x48, eax);                                                           // 41EB8D
        put(0x64, eax);                                                           // 41EB92
        eax = FrameWord::known(p.word(0xb0 + segment * 4u, 0x41eb97));            // 41EB97
        ecx = frame.transport(-120, 0x41eb9e);                                    // 41EB9E
        edx = frame.transport(-116, 0x41eba1);                                    // 41EBA1
        put(0x80, eax);                                                           // 41EBA4
        put(0x9c, eax);                                                           // 41EBA9
        eax = frame.transport(-124, 0x41ebae);                                    // 41EBAE
        put(0x3c, eax);                                                           // 41EBB1
        eax = frame.transport(-112, 0x41ebb6);                                    // 41EBB6
        put(0x4c, ecx);                                                           // 41EBB9
        ecx = frame.transport(-108, 0x41ebbf);                                    // 41EBBF
        put(0x50, edx);                                                           // 41EBC2
        edx = frame.transport(-32, 0x41ebc8);                                     // 41EBC8
        put(0x54, eax);                                                           // 41EBCB
        eax = frame.transport(-28, 0x41ebd0);                                     // 41EBD0
        put(0x58, ecx);                                                           // 41EBD3
        ecx = frame.transport(-24, 0x41ebd9);                                     // 41EBD9
        put(0x68, edx);                                                           // 41EBDC
        edx = frame.transport(-20, 0x41ebe2);                                     // 41EBE2
        put(0x6c, eax);                                                           // 41EBE5
        eax = frame.transport(-48, 0x41ebea);                                     // 41EBEA
        put(0x70, ecx);                                                           // 41EBED
        ecx = frame.transport(-44, 0x41ebf3);                                     // 41EBF3
        put(0x74, edx);                                                           // 41EBF6
        edx = frame.transport(-40, 0x41ebfc);                                     // 41EBFC
        put(0x84, eax);                                                           // 41EBFF
        eax = frame.transport(-36, 0x41ec04);                                     // 41EC04
        put(0x88, ecx);                                                           // 41EC07
        ecx = FrameWord::known(0x571330);                                         // 41EC0D
        put(0x34, edi);                                                           // 41EC12
        put(0x38, ebx);                                                           // 41EC18
        put(0x8c, edx);                                                           // 41EC1E
        put(0x90, eax);                                                           // 41EC24
        (void)append_source_trail_quad_4342e0(w.sprite_packets, w.trail_staging); // 41EC29
        ecx = frame.transport(-152, 0x41ec2e);                                    // 41EC2E
        edx = frame.transport(-148, 0x41ec34);                                    // 41EC34
        eax = frame.transport(-144, 0x41ec3a);                                    // 41EC3A
        put(0x30, ecx);                                                           // 41EC40
        ecx = frame.transport(-140, 0x41ec46);                                    // 41EC46
        put(0x34, edx);                                                           // 41EC4C
        edx = frame.transport(-104, 0x41ec52);                                    // 41EC52
        put(0x3c, ecx);                                                           // 41EC55
        ecx = frame.transport(-96, 0x41ec5b);                                     // 41EC5B
        put(0x38, eax);                                                           // 41EC5E
        eax = frame.transport(-100, 0x41ec63);                                    // 41EC63
        put(0x54, ecx);                                                           // 41EC66
        ecx = frame.transport(-12, 0x41ec6c);                                     // 41EC6C
        put(0x4c, edx);                                                           // 41EC6F
        edx = frame.transport(-92, 0x41ec75);                                     // 41EC75
        put(0x50, eax);                                                           // 41EC78
        eax = frame.transport(-16, 0x41ec7d);                                     // 41EC7D
        put(0x6c, ecx);                                                           // 41EC80
        ecx = frame.transport(-64, 0x41ec86);                                     // 41EC86
        put(0x58, edx);                                                           // 41EC89
        edx = frame.transport(-8, 0x41ec8f);                                      // 41EC8F
        put(0x68, eax);                                                           // 41EC92
        eax = frame.transport(-4, 0x41ec97);                                      // 41EC97
        put(0x84, ecx);                                                           // 41EC9A
        ecx = frame.transport(-52, 0x41eca0);                                     // 41ECA0
        put(0x70, edx);                                                           // 41ECA3
        edx = frame.transport(-60, 0x41eca9);                                     // 41ECA9
        put(0x74, eax);                                                           // 41ECAC
        eax = frame.transport(-56, 0x41ecb1);                                     // 41ECB1
        put(0x90, ecx);                                                           // 41ECB4
        ecx = FrameWord::known(0x571330);                                         // 41ECBA
        put(0x88, edx);                                                           // 41ECBF
        put(0x8c, eax);                                                           // 41ECC5
        (void)append_source_trail_quad_4342e0(w.sprite_packets, w.trail_staging); // 41ECCA
        edx = frame.transport(-152, 0x41eccf);                                    // 41ECCF
        eax = frame.transport(-148, 0x41ecd5);                                    // 41ECD5
        ecx = frame.transport(-144, 0x41ecdb);                                    // 41ECDB
        put(0x30, edx);                                                           // 41ECE1
        put(0x34, eax);                                                           // 41ECE7
        put(0x38, ecx);                                                           // 41ECEC
        edx = frame.transport(-140, 0x41ecf2);                                    // 41ECF2
        ecx = frame.transport(-48, 0x41ecf8);                                     // 41ECF8
        eax = frame.transport(-124, 0x41ecfb);                                    // 41ECFB
        put(0x3c, edx);                                                           // 41ECFE
        edx = frame.transport(-44, 0x41ed04);                                     // 41ED04
        put(0x68, ecx);                                                           // 41ED07
        ecx = frame.transport(-36, 0x41ed0d);                                     // 41ED0D
        put(0x58, eax);                                                           // 41ED10
        eax = frame.transport(-40, 0x41ed15);                                     // 41ED15
        put(0x6c, edx);                                                           // 41ED18
        edx = frame.transport(-64, 0x41ed1e);                                     // 41ED1E
        put(0x74, ecx);                                                           // 41ED21
        ecx = frame.transport(-56, 0x41ed27);                                     // 41ED27
        put(0x70, eax);                                                           // 41ED2A
        eax = frame.transport(-60, 0x41ed2f);                                     // 41ED2F
        put(0x84, edx);                                                           // 41ED32
        edx = frame.transport(-52, 0x41ed38);                                     // 41ED38
        put(0x8c, ecx);                                                           // 41ED3B
        ecx = FrameWord::known(0x571330);                                         // 41ED41
        put(0x4c, esi);                                                           // 41ED46
        put(0x50, edi);                                                           // 41ED4C
        put(0x54, ebx);                                                           // 41ED52
        put(0x88, eax);                                                           // 41ED58
        put(0x90, edx);                                                           // 41ED5D
        (void)append_source_trail_quad_4342e0(w.sprite_packets, w.trail_staging); // 41ED63
        eax = frame.transport(-104, 0x41ed68);                                    // 41ED68
        ecx = frame.transport(-100, 0x41ed6b);                                    // 41ED6B
        edx = frame.transport(-96, 0x41ed6e);                                     // 41ED6E
        put(0x30, eax);                                                           // 41ED71
        eax = frame.transport(-92, 0x41ed76);                                     // 41ED76
        put(0x34, ecx);                                                           // 41ED79
        ecx = frame.transport(-120, 0x41ed7f);                                    // 41ED7F
        put(0x38, edx);                                                           // 41ED82
        edx = frame.transport(-116, 0x41ed88);                                    // 41ED88
        put(0x3c, eax);                                                           // 41ED8B
        eax = frame.transport(-112, 0x41ed90);                                    // 41ED90
        put(0x4c, ecx);                                                           // 41ED93
        ecx = frame.transport(-108, 0x41ed99);                                    // 41ED99
        put(0x50, edx);                                                           // 41ED9C
        edx = frame.transport(-32, 0x41eda2);                                     // 41EDA2
        put(0x54, eax);                                                           // 41EDA5
        eax = frame.transport(-28, 0x41edaa);                                     // 41EDAA
        put(0x58, ecx);                                                           // 41EDAD
        ecx = frame.transport(-24, 0x41edb3);                                     // 41EDB3
        put(0x68, edx);                                                           // 41EDB6
        edx = frame.transport(-20, 0x41edbc);                                     // 41EDBC
        put(0x6c, eax);                                                           // 41EDBF
        eax = frame.transport(-16, 0x41edc4);                                     // 41EDC4
        put(0x70, ecx);                                                           // 41EDC7
        ecx = frame.transport(-12, 0x41edcd);                                     // 41EDCD
        put(0x74, edx);                                                           // 41EDD0
        edx = frame.transport(-8, 0x41edd6);                                      // 41EDD6
        put(0x84, eax);                                                           // 41EDD9
        eax = frame.transport(-4, 0x41edde);                                      // 41EDDE
        put(0x88, ecx);                                                           // 41EDE1
        ecx = FrameWord::known(0x571330);                                         // 41EDE7
        put(0x8c, edx);                                                           // 41EDEC
        put(0x90, eax);                                                           // 41EDF2
        (void)append_source_trail_quad_4342e0(w.sprite_packets, w.trail_staging); // 41EDF7
    }
}
void uv(RaceSessionState &w, const Profile &p, W mode)
{
    if (mode == 1)
    {
        const auto u0 = p.word(0x1c, 0x41f4f9), v0 = p.word(0x20, 0x41f4ff);
        staging(w, 0x40, u0);
        const auto u1 = p.word(0x24, 0x41f507);
        staging(w, 0x44, v0);
        staging(w, 0x5c, u1);
        staging(w, 0x60, v0);
        const auto v1 = p.word(0x28, 0x41f51b);
        staging(w, 0x78, u1);
        const auto last_u = p.word(0x1c, 0x41f523);
        staging(w, 0x7c, v1);
        staging(w, 0x94, last_u);
        staging(w, 0x98, v1);
    }
    else
    {
        const auto v0 = p.word(0x20, 0x41f553), u0 = p.word(0x1c, 0x41f559);
        staging(w, 0x44, v0);
        staging(w, 0x40, u0);
        const auto u1 = p.word(0x24, 0x41f567);
        staging(w, 0x5c, u1);
        staging(w, 0x60, v0);
        const auto v1 = p.word(0x28, 0x41f575);
        staging(w, 0x78, u1);
        const auto last_u = p.word(0x1c, 0x41f57e);
        staging(w, 0x7c, v1);
        staging(w, 0x94, last_u);
        staging(w, 0x98, v1);
    }
}
void append_ring_body(RaceSessionState &w, const Profile &p, const Ring &ring, W cursor, W count, float scale,
                      float opacity)
{
    SGN_PROFILE_SCOPE("entities.ring.body_41f2f0");
    if (p.word(0xec, 0x41f302) == 1)
    {
        const auto z = p.real(0x108, 0x41f313);
        if (sgn_source_ring_nonzero_41f313(&z))
            world_head(w, p, ring, scale, opacity);
    }
    if (w.split_setting != 0)
        return; // 41F3DE..E5 after actual head output
    if (p.word(0x10, 0x41f3eb) == 0)
    {
        screen_trail(w, p, ring, cursor, count, scale, opacity);
        return;
    }
    const auto texture = p.word(0x18, 0x41f417), first_width_bits = p.word(0x2c, 0x41f41d),
               flags = p.word(0x14, 0x41f420);
    const auto first_width = std::bit_cast<float>(first_width_bits);
    staging(w, 0x20, texture);
    staging(w, 0x24, flags);
    staging(w, 0x2c, 4);
    float previous_width = multiply(scale, first_width);
    if (count <= 1)
        return;
    const auto stride = p.word(8, 0x41f44c);
    W length = 1;
    if (count > stride)
    {
        const auto capacity = p.word(4, 0x41f45c);
        if (stride == 0)
            throw SourceAllocationError(count < capacity ? 0x41f463 : 0x41f46a, "source trail DIV by zero");
        length = count / stride;
        if (count < capacity)
            ++length;
    }
    W previous = cursor;
    auto running = cursor;
    bool first = true;
    if (std::bit_cast<I>(length) <= 0)
        return;
    for (W segment = 0; segment < length; ++segment)
    {
        const auto width = multiply(scale, p.real(0x30 + segment * 4u, 0x41f499));
        const auto old_width = previous_width;
        previous_width = width;
        const auto a = alpha(opacity, p.real(0x6c + segment * 4u, 0x41f4a8));
        const auto capacity = p.word(4, 0x41f4af);
        staging(w, 0x28, a);
        running -= p.word(8, 0x41f4ba);
        auto selected = running;
        if (count < capacity)
        {
            if (std::bit_cast<I>(running) < 1)
                selected = 1;
        }
        else if (std::bit_cast<I>(running) < 0)
            selected = capacity + running;
        const auto old_point = previous;
        previous = selected;
        const auto mode = p.word(0, 0x41f4e6);
        if (mode > 1)
            continue; // literal branch before UV/shape/ring dereference
        if (first)
        {
            first = false;
            uv(w, p, mode);
        }
        world_segment(w, p, ring, old_point, selected, old_width, width, segment, mode);
    }
}
struct VehicleCallsite
{
    W site;
    SourceVehicleRingKind kind;
    W side, profile;
};
constexpr std::array<VehicleCallsite, 7> vehicle_calls = {{{0x414d6a, SourceVehicleRingKind::speed, 0, 0x474c08},
                                                           {0x414ea6, SourceVehicleRingKind::effect, 0, 0x474af8},
                                                           {0x414ee2, SourceVehicleRingKind::effect, 1, 0x474af8},
                                                           {0x415093, SourceVehicleRingKind::pair_a, 0, 0x474d18},
                                                           {0x4150bb, SourceVehicleRingKind::pair_a, 1, 0x474d18},
                                                           {0x415475, SourceVehicleRingKind::pair_b, 0, 0x474e28},
                                                           {0x4154a8, SourceVehicleRingKind::pair_b, 1, 0x474e28}}};
} // namespace
void append_source_impact_ring_41f2f0(RaceSessionPlatform &platform, const contact::effects::ImpactParticle &particle,
                                      W type, W cursor, W count, float scale, float opacity)
{
    platform.require_owner_thread();
    if (!platform.source_construction_only_entry())
        throw SourceAllocationError(0x41f2f0, "source ring render requires actual platform");
    auto &w = platform.state();
    // R19: same predicate as the former scan for an element whose address equals
    //&particle, as one range/stride test over the actual32-element array.
    const auto &bank = w.impact_particles.particles;
    const auto at = reinterpret_cast<std::uintptr_t>(&particle), first = reinterpret_cast<std::uintptr_t>(bank.data());
    const bool owned =
        at >= first && at - first < sizeof(bank) && (at - first) % sizeof(contact::effects::ImpactParticle) == 0;
    if (!owned)
        throw SourceAllocationError(0x41f2f0, "source impact ring is not owned by this platform");
    const Profile p(w, type);
    const Ring ring(particle);
    host_presentation::RingOwnerScope presentation_owner(w, particle); // NEW HOST (R28) interpolation tag
    append_ring_body(w, p, ring, cursor, count, scale, opacity);
}
void SourceVehicleRingBinding::append_41f2f0(RaceSessionPlatform &platform, W site,
                                             const SourceVehicleRingCapture &captured)
{
    platform.require_owner_thread();
    if (!platform.source_construction_only_entry())
        throw SourceAllocationError(site, "vehicle ring requires actual source platform");
    bool matched = false;
    for (const auto &call : vehicle_calls)
    {
        if (call.site == site && call.kind == captured.kind && call.side == captured.side &&
            call.profile == captured.profile_address)
        {
            matched = true;
            break;
        }
    }
    if (!matched)
        throw SourceAllocationError(site, "vehicle ring callsite and captured owner/profile disagree");
    // Only owner identity and extent are borrowed here; not one ring/profile
    // word is read before41F302. No count/cursor/float normalization or gates.
    const auto actual = borrow_source_vehicle_ring_41f2f0(platform, captured);
    const Ring ring(actual);
    auto &w = platform.state();
    const Profile p(w, (captured.profile_address - 0x474af8u) / 0x110u);
    host_presentation::RingOwnerScope presentation_owner(captured.vehicle); // NEW HOST (R28) interpolation tag
    append_ring_body(w, p, ring, captured.cursor, captured.count, captured.scale, captured.alpha);
}
} // namespace sgn::retail
