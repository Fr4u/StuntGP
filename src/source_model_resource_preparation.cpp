#include "sgn/source_model_resource_preparation.hpp"
#include "sgn/race_session.hpp"
#include "sgn/scene_resources.hpp"
#include "sgn/source_pmd_material_base.hpp"
#include <array>
#include <cstddef>
#include <cstring>
#include <limits>
#include <optional>
#include <span>
#include <variant>

extern "C" void sgn_source_light_strength_42e3bd(void *, const std::uint32_t *) noexcept;

namespace sgn::retail
{
namespace
{
using Word = std::uint32_t;
SourceAddressLocation add(SourceAddressLocation at, Word offset, Word site)
{
    if (offset > std::numeric_limits<Word>::max() - at.offset)
        throw SourceAllocationError(site, "source model preparation offset leaves host domain");
    at.offset += offset;
    return at;
}
Word part_offset(Word part, Word site)
{
    if (part > std::numeric_limits<Word>::max() / 64u)
        throw SourceAllocationError(site, "source model part stride leaves host domain");
    return part * 64u;
}
std::span<std::uint8_t> bytes(SourceAllocationBank &bank, SourceAddressLocation at, Word count, Word site)
{
    const auto owner = bank.bytes(at.allocation);
    if (at.offset > owner.size() || count > owner.size() - at.offset)
        throw SourceAllocationError(site, "source model preparation reaches outside actual allocation");
    return owner.subspan(at.offset, count);
}
template <class T> T read(SourceAllocationBank &bank, SourceAddressLocation at, Word site)
{
    const auto source = bytes(bank, at, static_cast<Word>(sizeof(T)), site);
    T value;
    std::memcpy(&value, source.data(), sizeof(T));
    return value;
}
void write_color(SourceAllocationBank &bank, SourceAddressLocation at, std::uint8_t value, Word site)
{
    auto destination = bytes(bank, at, 1, site);
    // These recovered identity-valued byte writes must not become a host no-op.
    *static_cast<volatile std::uint8_t *>(destination.data()) = value;
}
std::uint8_t scale_color(std::uint8_t value) noexcept
{
    // 42E2B0..2C8 (and three repeated lanes): signed magic division is exact
    // for this nonnegative 0..65025 domain. Preserve the reached stores below.
    return static_cast<std::uint8_t>((Word(value) * 255u) / 255u);
}
bool null_descriptor(const SceneResourceRecord &record)
{
    if (std::holds_alternative<std::monostate>(record.descriptor))
        return true;
    if (const auto *strip = std::get_if<VehicleStripGeometry *>(&record.descriptor))
        return *strip == nullptr;
    if (const auto *strip = std::get_if<SceneStripBinding>(&record.descriptor))
        return !strip->source_descriptor;
    return false;
}
struct ModelBorrow
{
    SourceAllocationBank &bank;
    SourceAddressLocation descriptor;
    std::optional<SourcePmdModelView> pmd;
    Word word(Word offset, Word site) const
    {
        return read<Word>(bank, add(descriptor, offset, site), site);
    }
    SourceAddressLocation pointer(Word value, Word site) const
    {
        if (!pmd)
            return bank.decode_address(value);
        if (value == 0)
            throw SourceAllocationError(site, "null relocated PMD preparation pointer");
        return {descriptor.allocation, value};
    }
    SourceAddressLocation part(Word value, Word index, Word site) const
    {
        auto at = add(pointer(value, site), part_offset(index, site), site);
        if (pmd)
        {
            // Existing immutable relocation history; no logical part-count read.
            const auto checked = pmd->captured_base_part_42b055(at.offset).location();
            at = {checked.allocation, checked.offset};
        }
        return at;
    }
    bool visited(Word lod, Word index) const
    {
        if (pmd)
            return source_pmd_material_part_visited_42b8b0(*pmd, lod, index);
        // Same child on generated source allocations, whose pointers are bank
        // transport tokens rather than PMD-relative offsets. Do not reinterpret.
        if (lod > (std::numeric_limits<Word>::max() - 0x48u) / 4u)
            throw SourceAllocationError(0x42b8b9, "source generated LOD offset overflow");
        const auto raw = word(0x48u + lod * 4u, 0x42b8b9); // even for part0
        if (index == 0)
            return false; // no pointed access in this source branch
        const auto current = part(raw, index, 0x42b8c8);
        const auto attributes = read<Word>(bank, add(current, 0x34, 0x42b8c8), 0x42b8c8);
        for (Word previous = 0; previous < index; ++previous)
        {
            const auto earlier = part(raw, previous, 0x42b8ce);
            if (attributes == read<Word>(bank, add(earlier, 0x34, 0x42b8ce), 0x42b8ce) &&
                read<std::uint16_t>(bank, add(earlier, 0x24, 0x42b8d3), 0x42b8d3) != 0)
                return true;
        }
        return false;
    }
};
ModelBorrow borrow_model(RaceSessionPlatform &platform, const SceneResourceRecord &record)
{
    auto &scene = platform.source_scene_resources();
    auto &bank = platform.source_allocations();
    if (const auto *strip = std::get_if<SceneStripBinding>(&record.descriptor))
    {
        if (strip->owner != &platform.state().strip)
            throw SourceAllocationError(0x42e253, "source strip material owner belongs to another world");
        return {bank, {strip->source_descriptor, 0}, std::nullopt};
    }
    if (const auto *binding = std::get_if<ScenePmdBinding>(&record.descriptor))
    {
        const auto model = scene.models().source_at(binding->handle); // native mirror rejects
        const auto descriptor = model.descriptor().location();
        return {bank, {descriptor.allocation, descriptor.offset}, model};
    }
    if (const auto *sprite = std::get_if<SceneSpriteBinding>(&record.descriptor))
    {
        auto &world = platform.state();
        scene.require_sprite_binding(world.model_draws, world.sprite_packets);
        if (!sprite->source_descriptor)
            throw SourceAllocationError(0x42e253, "42E240 requires the actual source sprite descriptor");
        return {bank, {sprite->source_descriptor, 0}, std::nullopt};
    }
    throw SourceAllocationError(
        0x42e253, "42E240 requires source descriptor storage; native strip/NULL descriptor is not reconstructed here");
}
void prepare_model(RaceSessionPlatform &platform, const SceneResourceRecord &record)
{
    const auto model = borrow_model(platform, record);
    if (model.word(0x58, 0x42e253) + 1u == 0)
        return; // INC wrap, no part access
    Word lod = 0;
    do
    {
        auto limit = model.word(0x24, 0x42e265);
        Word index = 0;
        if (limit != 0)
            do
            {
                if (!model.visited(lod, index))
                {
                    if (lod > (std::numeric_limits<Word>::max() - 0x48u) / 4u)
                        throw SourceAllocationError(0x42e291, "source LOD pointer offset overflow");
                    const auto raw = model.word(0x48u + lod * 4u, 0x42e291); // fresh after42B8B0
                    const auto part = model.part(raw, index, 0x42e293);
                    const auto coordinates = read<Word>(model.bank, add(part, 0x2c, 0x42e293), 0x42e293);
                    auto count = read<std::uint16_t>(model.bank, add(part, 0x26, 0x42e299), 0x42e299);
                    if (count != 0)
                    {
                        auto corner = model.pointer(coordinates, 0x42e2aa);
                        do
                        {
                            // Source reads the NEXT byte before storing the prior
                            // lane: +15,+14,store15,+13,store14,+12,store13,store12.
                            const auto a = read<std::uint8_t>(model.bank, add(corner, 15, 0x42e2aa), 0x42e2aa);
                            const auto b = read<std::uint8_t>(model.bank, add(corner, 14, 0x42e2cc), 0x42e2cc);
                            write_color(model.bank, add(corner, 15, 0x42e2cf), scale_color(a), 0x42e2cf);
                            const auto c = read<std::uint8_t>(model.bank, add(corner, 13, 0x42e2ee), 0x42e2ee);
                            write_color(model.bank, add(corner, 14, 0x42e2f1), scale_color(b), 0x42e2f1);
                            const auto d = read<std::uint8_t>(model.bank, add(corner, 12, 0x42e310), 0x42e310);
                            write_color(model.bank, add(corner, 13, 0x42e313), scale_color(c), 0x42e313);
                            --count;
                            write_color(model.bank, add(corner, 12, 0x42e331), scale_color(d), 0x42e331);
                            if (count != 0)
                                corner = add(corner, 16, 0x42e2ad);
                        } while (count != 0);
                    }
                }
                limit = model.word(0x24, 0x42e340); // before incrementing the part index
                ++index;
            } while (index < limit);
        const auto end = model.word(0x58, 0x42e358) + 1u;
        ++lod;
        if (lod >= end)
            break;
    } while (true);
}
} // namespace

void prepare_source_model_colors_42e240(RaceSessionPlatform &platform, Word slot)
{
    platform.require_owner_thread();
    auto &scene = platform.source_scene_resources();
    const auto records = scene.source_records();
    if (slot >= records.size())
        throw SourceAllocationError(0x42e253, "source model registry slot outside represented owner");
    prepare_model(platform, records[slot]);
}
void prepare_source_model_resources_42e380(const std::shared_ptr<RaceSessionPlatform> &platform)
{
    if (!platform)
        throw SourceAllocationError(0x42e380, "source model preparation needs its retained platform");
    platform->require_owner_thread();
    auto &world = platform->state();
    auto &scene = platform->source_scene_resources();
    Word slot = world.outer.device.initialized_6244f8 == 1 ? 2u : 0u;
    for (;; ++slot)
    {
        if (slot == 16)
        {
            if (scene.material_registry_terminal_636518() == 0)
                break;
            throw SourceAllocationError(0x42e39b, "42E380 reached nonnull unrepresented registry terminal descriptor");
        }
        const auto &record = scene.source_records()[slot];
        if (null_descriptor(record))
            break;
        prepare_model(*platform, record);
    }
    const auto &track = scene.models().source_loading_state().track;
    auto count = track.light_count;   // 62449C captured once,42E3AA
    const auto lights = track.lights; // 62447C captured even when count0,42E3B0
    if (count == 0)
        return;
    // Pinned immutable PE FLOAT literals, not rounded reciprocal/double values.
    constexpr std::array<Word, 2> factors{0x437f0000u, 0x3b808081u}; // 46E1E4,46E614
    auto &bank = platform->source_allocations();
    if (const auto *pmd = std::get_if<SourcePmdLocation>(&lights))
    {
        auto current = add({pmd->allocation, pmd->offset}, 8, 0x42e3ba);
        do
        {
            auto strength = bytes(bank, current, 4, 0x42e3bd);
            sgn_source_light_strength_42e3bd(strength.data(), factors.data());
            --count;
            if (count != 0)
                current = add(current, 0x90, 0x42e3c5);
        } while (count != 0);
        return;
    }
    const auto &pointer = std::get<SourceSceneStaticPointer>(lights);
    auto &actual = world.lights.source_menu_light_589628;
    static_assert(offsetof(RetailLightRecord, strength) == 8);
    const auto expected = SourceSceneStaticPointer::bind(
        platform, 0x589628, {reinterpret_cast<const std::uint8_t *>(&actual), sizeof(actual)});
    if (!(pointer == expected))
        throw SourceAllocationError(0x42e3bd, "light strength pointer does not name this world's actual static light");
    pointer.check();
    sgn_source_light_strength_42e3bd(&actual.strength, factors.data());
    if (--count != 0)
        throw SourceAllocationError(
            0x42e3bd, "42E380 reached beyond the single represented static light; first strength store retained");
}
} // namespace sgn::retail
