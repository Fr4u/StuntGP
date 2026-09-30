#pragma once
#include "sgn/source_owned_array.hpp"
#include "sgn/source_owned_field.hpp"
#include "sgn/retail_lighting.hpp"

namespace sgn::retail
{
struct LightViewAnimation
{
    std::uint32_t phase_fast{}, phase_slow{}; // 589DF8/589DFC
};
// 624530 owns the source allocation;624574 is its working cursor. Native code
// uses a borrowed buffer and offset, not a copied 32-bit executable pointer.
struct LightVertexCursor
{
    std::span<std::uint8_t> buffer;
    std::uint32_t offset{};
};
struct GlareElement
{
    std::uint32_t texture{};
    float displacement{}, scale{};
    std::uint32_t step{}, phase{}, color{};
};
static_assert(sizeof(GlareElement) == 24);
struct GlareState
{
    std::array<GlareElement, 7> elements;
}; // 477B88, shared across views
GlareState initial_glare_state(); // literal pinned PE words; not a per-frame reset
// The source reads sine[phase] BEFORE applying &8191. In particular its PE
// second phase is 0x6000, reading the live word at4A25D8 outside the sine table.
// The caller must borrow such adjacent source-owned values, never assume zero.
struct GlareAdjacentSample
{
    std::uint32_t index;
    const float &value;
};
struct SourceSpriteRecord
{
    std::uint32_t flags{};
    std::uint16_t texture{}, alpha{};
    std::array<std::uint32_t, 4> colors{};
    CameraVector position{};
    std::array<float, 3> scale{};
    std::array<std::uint32_t, 5> opaque_34_44{};
    CameraVector up{};
    std::array<std::uint32_t, 4> opaque_58_64{};
};
static_assert(sizeof(SourceSpriteRecord) == 0x68 && offsetof(SourceSpriteRecord, up) == 0x48);
struct SourceSpriteAttribute
{
    std::uint32_t u{}, v{};
    std::uint16_t vertex{}, retained{};
    std::uint32_t color{};
};
static_assert(sizeof(SourceSpriteAttribute) == 16);
struct SourceSpritePacketStorage
{
    // Exact source allocations434090. Written ranges only; W and padding stay.
    SourceOwnedArray<std::array<std::uint8_t, 8>, 512> headers;
    SourceOwnedArray<std::uint16_t, 2048> indices;
    SourceOwnedArray<CameraVector, 2048> positions;
    SourceOwnedArray<SourceSpriteAttribute, 2048> attributes;
    // Actual allocations5DA244/248; no decoded source-part/descriptor copy.
    // Native preview has no raw part/descriptor. Source count/flag projections
    // below borrow these bytes; source current pointers are explicit aliases.
    SourceWorldSpan<std::uint8_t> source_part, source_descriptor;
    std::uint32_t header_bytes{}, index_count{}, position_count{}, attribute_count{};
    SourceOwnedField<std::uint16_t> polygons{}, vertices{}, corners{}; // part24/26/28
    // Canonical working pointer publications 5DA24C..5DA260. Legacy scalar
    // cursors above are inactive in a source world; typed producers reject it.
    SourceWorldSpan<std::array<std::uint8_t, 8>> current_headers;
    SourceWorldSpan<std::uint16_t> current_indices;
    SourceWorldSpan<CameraVector> current_positions;
    SourceWorldSpan<SourceSpriteAttribute> current_attributes;
    SourceWorldSpan<std::uint8_t> current_part, current_descriptor;
    // PE478910, four 28-byte records. Initial UV/padding/color bytes are retained.
    std::array<std::array<std::uint32_t, 7>, 4> source_rectangle{
        {{{0xc2480000, 0x42480000, 0, 0, 0, 0, 0x01ffffff}},
         {{0x42480000, 0x42480000, 0, 0, 0x3f7f7cee, 0, 0x01ffffff}},
         {{0x42480000, 0xc2480000, 0, 0, 0x3f7f7cee, 0x3f7f7cee, 0x01ffffff}},
         {{0xc2480000, 0xc2480000, 0, 0, 0, 0x3f7f7cee, 0x01ffffff}}}};
    CameraVector right{}, up{}, forward{}; // 5DA1E0/5DA1F0/5DA200, shared sprite scratch
    CameraVector matrix_last_row{};        // 5DA210..5DA21F, fourth row of the SAME matrix
};
struct SourceSpritePacketView
{
    std::span<const std::array<std::uint8_t, 8>> headers;
    std::span<const std::uint16_t> indices;
    std::span<const CameraVector> positions;
    std::span<const SourceSpriteAttribute> attributes;
};
SourceSpritePacketView sprite_packet_view(const SourceSpritePacketStorage &);
std::uint32_t classify_sprite(const SourceSpriteRecord &, LightCameraView);                          // complete4280A0
bool append_sprite_packet(SourceSpritePacketStorage &, const SourceSpriteRecord &, LightCameraView); // complete434590
// Actual434590, including4280A0, real current pointers and captured camera+50.
// Caller keeps the same pool/camera/arena/identity owners alive and stable.
// The input sprite must not alias the output buffers or shared sprite axes.
// Part/header/XYZ/UV writes survive a later guard; no preview counter updates.
bool append_source_sprite_packet_434590(SourceSpritePacketStorage &, const SourceSpriteRecord &, LightCameraView);
// Complete42DCF0 including42D9B0 and44EB70. Copy624530 is an existing borrowed
// buffer. Optional rotation audit observes61C3A4, never becomes a runtime owner.
void prepare_animated_light_view(PmdLightBank &, LightViewSelection &, LightFadeState &, LightViewAnimation &,
                                 LightCameraView, LightVertexCursor &, std::uint32_t split_setting, LightGammaDevice *,
                                 CameraMatrix *rotation_audit = nullptr);
// Complete42DD80 including actual434590/4280A0 packet writes. No draw callback.
void append_light_glare(const RetailLightRecord &, LightFadeState &, const LightViewAnimation &, GlareState &,
                        SourceSpritePacketStorage &, LightCameraView,
                        std::span<const GlareAdjacentSample> adjacent_samples);
} // namespace sgn::retail
