#include "sgn/source_sprite_allocations.hpp"
#include "sgn/race_session.hpp"
namespace sgn::retail
{
SourceSpriteAllocationsResult allocate_source_sprite_buffers_434090(const std::weak_ptr<RaceSessionPlatform> &borrow)
{
    const auto platform = borrow.lock();
    if (!platform)
        throw SourceAllocationError(0x434090, "sprite allocation platform expired");
    platform->require_owner_thread();
    if (!platform->source_construction_only_entry())
        throw SourceAllocationError(0x434090, "sprite allocation requires source world");
    auto &sprites = platform->state().sprite_packets;
    auto &bank = platform->source_allocations();
    static_assert(sizeof(CameraVector) == 16 && sizeof(SourceSpriteAttribute) == 16);
    const auto headers = bank.allocate_42e8d0(0x1000, 0); // 43409A
    sprites.headers.bind_source(
        SourceWorldSpan<std::array<std::uint8_t, 8>>::source(platform, headers, 0x1000));              // 4340A6
    const auto indices = bank.allocate_42e8d0(0x1000, 0);                                              // 4340AB
    sprites.indices.bind_source(SourceWorldSpan<std::uint16_t>::source(platform, indices, 0x1000));    // 4340B7
    const auto positions = bank.allocate_42e8d0(0x8000, 0);                                            // 4340BC
    sprites.positions.bind_source(SourceWorldSpan<CameraVector>::source(platform, positions, 0x8000)); // 4340C8
    const auto attributes = bank.allocate_42e8d0(0x8000, 0);                                           // 4340CD
    sprites.attributes.bind_source(
        SourceWorldSpan<SourceSpriteAttribute>::source(platform, attributes, 0x8000));             // 4340D9
    const auto part = bank.allocate_42e8d0(0x40, 0);                                               // 4340DE
    sprites.source_part = SourceWorldSpan<std::uint8_t>::source(platform, part, 0x40);             // 4340EA
    const auto descriptor = bank.allocate_42e8d0(0x68, 0);                                         // 4340EF
    sprites.source_descriptor = SourceWorldSpan<std::uint8_t>::source(platform, descriptor, 0x68); // 4340F4
    return {}; // 4340F9 scene publication is next, no premature ready state
}
} // namespace sgn::retail
