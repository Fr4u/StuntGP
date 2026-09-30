#include "sgn/source_sprite_scene.hpp"
#include "sgn/source_sprite_allocations.hpp"
#include "sgn/race_session.hpp"
#include "sgn/scene_resources.hpp"
#include <cstring>
namespace sgn::retail
{
namespace
{
std::shared_ptr<RaceSessionPlatform> require_platform(const std::weak_ptr<RaceSessionPlatform> &borrow)
{
    const auto p = borrow.lock();
    if (!p)
        throw SourceAllocationError(0x434090, "sprite platform expired");
    p->require_owner_thread();
    if (!p->source_construction_only_entry())
        throw SourceAllocationError(0x434090, "sprite entry requires source world");
    return p;
}
template <class T> void write(const SourceWorldSpan<std::uint8_t> &memory, std::size_t offset, T value)
{
    const auto b = memory.subspan(offset, sizeof(T));
    std::memcpy(b.data(), &value, sizeof(T));
}
void w16(const SourceWorldSpan<std::uint8_t> &b, std::size_t o, std::uint16_t v)
{
    write(b, o, v);
}
void w32(const SourceWorldSpan<std::uint8_t> &b, std::size_t o, std::uint32_t v)
{
    write(b, o, v);
}
template <class T> std::uint32_t token(SourceAllocationBank &bank, const SourceWorldSpan<T> &span)
{
    const auto &b = span.source_binding();
    return bank.encode_address(b.allocation, b.byte_offset);
}
} // namespace
void reset_source_sprite_cursors_434030(const std::weak_ptr<RaceSessionPlatform> &borrow)
{
    const auto platform = require_platform(borrow);
    auto &world = platform->state();
    auto &s = world.sprite_packets;
    auto &m = world.model_draws;
    reset_source_sprite_cursors_434030(m, s);
}
void initialize_source_sprite_allocations_434090(const std::weak_ptr<RaceSessionPlatform> &borrow)
{
    const auto platform = require_platform(borrow);
    auto &world = platform->state();
    auto &s = world.sprite_packets;
    auto &bank = platform->source_allocations();
    (void)allocate_source_sprite_buffers_434090(platform);
    platform->source_scene_resources().bind_sprite_4340f9(world.model_draws, s); // 4340F9 even for NULL descriptor
    reset_source_sprite_cursors_434030(platform);                                // 4340FE
    // 43410B..434179: only XYZ, preserve the other four DWORDs of each PE record.
    constexpr std::uint32_t x[]{0xc2480000, 0x42480000, 0x42480000, 0xc2480000};
    constexpr std::uint32_t y[]{0x42480000, 0x42480000, 0xc2480000, 0xc2480000};
    for (unsigned i = 0; i < 4; ++i)
    {
        s.source_rectangle[i][0] = x[i];
        s.source_rectangle[i][1] = y[i];
        s.source_rectangle[i][2] = 0;
    }
    for (std::uint16_t i = 0; i < 0x800; ++i)
        s.current_indices[i] = i; // 434185..434191
    for (unsigned i = 0; i < 0x800; ++i)
    { // 4341A9..4341BB: preserve position W and remaining attribute fields
        s.positions[i].x = 0;
        s.positions[i].y = 0;
        s.current_attributes[i].retained = 0;
        s.positions[i].z = 0;
    }
    for (unsigned i = 0; i < 0x200; ++i)
    {
        s.current_headers[i][4] = 0;
        s.current_headers[i][5] = 0;
    } // 4341CB
    const auto part = s.current_part;
    for (unsigned o = 0; o < 0x40; o += 4)
        w32(part, o, 0); // 4341E9 REP STOSD
    w16(part, 0, 1);
    w16(part, 2, 0);
    w32(part, 8, 0x4788c0); // 4341F0..4341FF
    for (unsigned o = 0x20; o <= 0x28; o += 2)
        w16(part, o, 0);                                 // 434206..434216
    w32(part, 0x2c, token(bank, s.attributes.source())); // 43421A
    w32(part, 0x30, token(bank, s.positions.source()));  // 43421D
    w32(part, 0x34, token(bank, s.headers.source()));    // 434220
    w32(part, 0x10, 0);
    w32(part, 0x14, 0);
    w32(part, 0x18, 0); // 434223..434229
    const auto descriptor = s.current_descriptor;
    for (unsigned o = 0; o < 0x68; o += 4)
        w32(descriptor, o, 0); // 43423B REP STOSD
    w16(descriptor, 2, 1);     // 434248
    w32(descriptor, 0xc, 0);
    w32(descriptor, 0x10, 0);
    w32(descriptor, 0x14, 0);
    w32(descriptor, 0x18, 0);
    w16(descriptor, 0x20, 0);
    w16(descriptor, 0x22, 0);
    w32(descriptor, 0x24, 1);
    // Static guest literals name existing world owners; allocated pointers use
    // the SAME bank's native-width identity token codec, never host truncation.
    w32(descriptor, 0x28, 0x4788b0);
    w32(descriptor, 0x2c, token(bank, s.headers.source()));
    w32(descriptor, 0x30, token(bank, s.attributes.source()));
    w32(descriptor, 0x34, token(bank, s.positions.source()));
    w32(descriptor, 0x38, token(bank, s.indices.source()));
    w32(descriptor, 0x3c, 0x4788c0);
    w32(descriptor, 0x40, 0x4788a8);
    for (unsigned o = 0x44; o <= 0x54; o += 4)
        w32(descriptor, o, token(bank, s.source_part));
    w32(descriptor, 0x58, 0);
    w32(descriptor, 0x5c, 0);
    w32(descriptor, 0x60, 0x5da230);
}
void initialize_source_sprite_scene_4342c0(const std::weak_ptr<RaceSessionPlatform> &borrow)
{
    const auto platform = require_platform(borrow);
    initialize_source_sprite_allocations_434090(platform); // 4342C0
    reset_source_sprite_cursors_434030(platform);          // 4342C5, not elided
    auto &s = platform->state().sprite_packets;            // 4342CA/4342CF ->44EE10(5DA1E0)
    s.right = {1, 0, 0, 0};
    s.up = {0, 1, 0, 0};
    s.forward = {0, 0, 1, 0};
    s.matrix_last_row = {0, 0, 0, 1};
}
} // namespace sgn::retail
