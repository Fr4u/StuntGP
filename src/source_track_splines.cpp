#include "sgn/track_spline_source.hpp"
#include "sgn/borrowed_material_name.hpp"
#include "sgn/contact_rail_state.hpp"
#include "sgn/race_session.hpp"
#include "sgn/scene_resources.hpp"
#include "sgn/source_application_literals.hpp"
#include "sgn/vehicle_ai_automatic.hpp"
#include <cstddef>
#include <cstdint>

// NEW R18 (DESIGN A1): the single source world's track spline owner. Every
// member reads the live 624474/624498 track globals and the [636448] PMD in
// the platform's one allocation bank; identities are published, never views.
// 4192F0/418B10/46DD90 moved here unchanged from source_vehicle_rail.cpp.
namespace sgn::retail
{
namespace
{
using W = std::uint32_t;
[[noreturn]] void gap(W site, const char *reason)
{
    throw SourceAllocationError(site, reason);
}
std::shared_ptr<RaceSessionPlatform> admit(const std::weak_ptr<RaceSessionPlatform> &weak, W site)
{
    auto platform = weak.lock();
    if (!platform)
        gap(site, "track spline platform expired");
    platform->require_owner_thread();
    return platform;
}

// 418B10 with 46DD90 against PE 474674 "Trk_TubeGlass".
std::uint8_t fold_46ddba(std::uint8_t byte) noexcept
{
    const auto displaced = static_cast<std::uint8_t>(byte - 0x41u);
    return static_cast<std::uint8_t>(displaced + (displaced < 0x1au ? 0x20u : 0u) + 0x41u);
}
std::int32_t compare_tube_label_46dd90(const SourceApplicationLiteralRef &app, const BorrowedMaterialName &label,
                                       std::size_t cursor)
{
    const bool non_c = app.read_compare_locale_46dda2() != 0; // 46DDA2
    W literal = 0x474674;                                     // 418B3B PUSH, second argument (ESI)
    std::uint8_t previous = 0xff;
    while (previous != 0)
    {
        auto right = app.read_level_kind_byte_46dd90(literal, non_c ? 0x46ddf0u : 0x46ddb0u);
        ++literal;
        auto left = label.byte(cursor);
        ++cursor; // 46DDB3/46DDF3 first argument (EDI)
        previous = right;
        if (left == right)
            continue;
        if (non_c)
            throw SourceLiteralReadError(0x46ddfc, left,
                                         "non-C locale mismatch requires unreconstructed 46AA6C/NLS lowercase child");
        right = fold_46ddba(right);
        left = fold_46ddba(left);
        previous = left;
        if (left == right)
            continue;
        return left < right ? -1 : 1;
    }
    return 0;
}
// Complete 418B10 (418B10..418B60). `rail` is the platform's sole owner of 474670.
void find_tube_texture_418b10(RaceSessionPlatform &p, RailState &rail)
{
    auto &scene = p.source_scene_resources();
    const auto handle = scene.model_handle(3); // 418B10 [636448]
    if (!handle)
        gap(0x418b19, "418B10 reached a null track PMD descriptor");
    const SourcePmdMaterialNames names(scene.models().source_at(*handle)); // 418B19 [EAX+60]
    rail.tube_glass_texture.reset();                                       // 418B1C 474670=FFFFFFFF
    const auto app = p.source_application_literals();
    for (W index = 0;; ++index)
    {                                       // EDI
        const auto label = names.at(index); // 418B26/418B53
        if (!label)
            return; // 418B2A/418B5C
        std::size_t cursor = 0;
        while (label.byte(cursor) != 0x5c)
            ++cursor;                                               // 418B2C..418B38: no terminator test in source
        if (compare_tube_label_46dd90(app, label, cursor + 1) == 0) // 418B3A..418B4B
            rail.tube_glass_texture = index;                        // 418B4D
    }
}
} // namespace

CameraSplineBinding SourceTrackSplines::find(std::uint32_t id) const
{
    CameraSplineBinding result; // 428800 returns NULL (empty) when absent
    result.bind_source(find_source_camera_spline_428800(platform_, id));
    return result;
}
CameraSplineBinding SourceTrackSplines::first() const
{
    // [624474] itself: descriptor 0 of the live track spline table (the reset
    // spline, 4183A5/418D34). An empty table has no descriptor 0 to name.
    const auto platform = admit(platform_, 0x418d34);
    const auto &track = platform->source_scene_resources().models().source_loading_state().track;
    CameraSplineBinding result;
    if (track.spline_count != 0 && track.splines.allocation)
        result.bind_source(SourceCameraSplineRef(platform_, track.splines.allocation, track.splines.offset));
    return result;
}
// Complete 4192F0 (4192F0..4192FF) including its tail JMP 418B10.
void SourceTrackSplines::initialize_rail(RailState &rail) const
{
    const auto platform = admit(platform_, 0x4192f5);
    if (&rail != &platform->state().rail)
        gap(0x4192fa, "4192F0 writes only this platform's sole 4D5C9C/474670 owner");
    rail.spline.bind_source(
        find_source_camera_spline_428800(platform_, 2)); // 4192F0 MOV ECX,2; 4192F5 CALL; 4192FA ->4D5C9C
    find_tube_texture_418b10(*platform, rail);           // 4192FF JMP 418B10
}
// 405260 route publication (40526E..405306). The caller owns the 49A164 gate,
// 49A16C..174 and the per-vehicle clears (40530B..).
void SourceTrackSplines::publish_ai_routes(AiRouteBindings &routes) const
{
    (void)admit(platform_, 0x40529b);
    routes.bind_source(0, find_source_camera_spline_428800(platform_, 3)); // 40526E ECX=3, 40529B CALL, 4052A5 ->49A158
    routes.bind_source(1, find_source_camera_spline_428800(platform_, 4)); // 4052A0 ECX=4, 4052AA CALL, 4052B4 ->49A15C
    routes.bind_source(2, find_source_camera_spline_428800(platform_, 5)); // 4052AF ECX=5, 4052B9 CALL, 4052BE ->49A160
    // 4052C3..405306: each NULL route independently re-looks-up ID1 in order
    //(4052CE/4052D3->49A158, 4052E5/4052EA->49A15C, 4052FC/405301->49A160);
    // the result may itself stay NULL, exactly as the source stores EAX.
    for (unsigned i = 0; i < 3; ++i)
        if (!routes.source(i))
            routes.bind_source(i, find_source_camera_spline_428800(platform_, 1));
}
// 418D34..418D40: MOV EAX,[624474]; MOV ECX,[EAX+24]. Absent beyond the track
// file, as in the decoded form (CHALL1 ends before it).
std::optional<std::uint32_t> SourceTrackSplines::reset_spacing_word() const
{
    const auto platform = admit(platform_, 0x418d34);
    auto &scene = platform->source_scene_resources();
    const auto &models = scene.models();
    const auto &track = models.source_loading_state().track;
    if (!track.splines.allocation)
        gap(0x418d34, "418D34 reads a NULL [624474]");
    const auto handle = scene.model_handle(3);
    if (!handle)
        gap(0x418d40, "[624474]+24 requires the live [636448] track PMD");
    const auto file = models.source_at(*handle).bytes();
    const auto location = file.location();
    if (!(location.allocation == track.splines.allocation) || track.splines.offset < location.offset)
        gap(0x418d40, "[624474] is not inside the live [636448] track PMD");
    const auto relative = std::uint64_t(track.splines.offset - location.offset) + 0x24u;
    const auto size = file.size();
    if (relative > size || size - relative < 4)
        return std::nullopt;
    return file.field<std::uint32_t>(static_cast<W>(relative)).value(0x418d40);
}
} // namespace sgn::retail
