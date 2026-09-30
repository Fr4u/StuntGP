#include "sgn/source_scene_uv.hpp"
#include "sgn/race_session.hpp"
#include "sgn/race_session_platform.hpp"
#include "sgn/material_d3d12.hpp"
#include <cstring>
#include <limits>

extern "C" void sgn_source_uv_coordinate_4310a0(void *) noexcept;

namespace sgn::retail
{
namespace
{
using W = std::uint32_t;
SourceAddressLocation add(SourceAddressLocation at, std::uint64_t amount, W site)
{
    if (amount > std::numeric_limits<W>::max() - at.offset)
        throw SourceAllocationError(site, "source UV offset leaves the admitted address domain");
    at.offset += static_cast<W>(amount);
    return at;
}
std::span<std::uint8_t> reached(SourceAllocationBank &bank, SourceAddressLocation at, W count, W site)
{
    const auto bytes = bank.bytes(at.allocation);
    if (at.offset > bytes.size() || count > bytes.size() - at.offset)
        throw SourceAllocationError(site, "reached UV field is outside its actual allocation");
    return bytes.subspan(at.offset, count);
}
template <class T> T read(SourceAllocationBank &bank, SourceAddressLocation at, W site)
{
    const auto bytes = reached(bank, at, static_cast<W>(sizeof(T)), site);
    T value;
    std::memcpy(&value, bytes.data(), sizeof(T));
    return value;
}
struct Model
{
    SourceAddressLocation descriptor;
    bool token_pointers{}; // generated sprite/strip tokens, PMD-relative offsets
    W word(SourceAllocationBank &bank, W offset, W site) const
    {
        return read<W>(bank, add(descriptor, offset, site), site);
    }
    SourceAddressLocation pointer(SourceAllocationBank &bank, SourceAddressLocation owner, W offset, W site) const
    {
        const auto value = read<W>(bank, add(owner, offset, site), site);
        if (value == 0)
            return {}; // do not turn NULL into the allocation's byte0
        if (token_pointers)
            return bank.decode_address(value);
        return {owner.allocation, value};
    }
};
bool already_42b8b0(SourceAllocationBank &bank, const Model &model, W lod, W part)
{
    // 42B8B9 reads even for part0. No pointed payload is read for that part.
    const auto table_offset = std::uint64_t(0x48) + std::uint64_t(lod) * 4u;
    if (table_offset > std::numeric_limits<W>::max())
        throw SourceAllocationError(0x42b8b9, "UV LOD field overflow");
    const auto base = model.pointer(bank, model.descriptor, static_cast<W>(table_offset), 0x42b8b9);
    const auto current = add(base, std::uint64_t(part) * 64u, 0x42b8c2);
    if (part == 0)
        return false;
    const auto headers = read<W>(bank, add(current, 0x34, 0x42b8c8), 0x42b8c8);
    for (W i = 0; i < part; ++i)
    {
        const auto previous = add(base, std::uint64_t(i) * 64u, 0x42b8ce);
        if (headers == read<W>(bank, add(previous, 0x34, 0x42b8ce), 0x42b8ce) &&
            read<std::uint16_t>(bank, add(previous, 0x24, 0x42b8d3), 0x42b8d3) != 0)
            return true;
    }
    return false;
}
void model_uv_4311c0(SourceAllocationBank &bank, const Model &model, const MaterialTransparencyStorage &transparency)
{
    if (model.word(bank, 0x58, 0x4311d2) + 1u == 0)
        return;
    W lod = 0;
    do
    {
        W part = 0;
        auto part_count = model.word(bank, 0x24, 0x4311e3);
        while (part < part_count)
        {
            if (!already_42b8b0(bank, model, lod, part))
            {
                const auto lod_field = std::uint64_t(0x48) + std::uint64_t(lod) * 4u;
                if (lod_field > std::numeric_limits<W>::max())
                    throw SourceAllocationError(0x43120a, "UV LOD field overflow");
                const auto base = model.pointer(bank, model.descriptor, static_cast<W>(lod_field), 0x43120a);
                const auto current = add(base, std::uint64_t(part) * 64u, 0x431213);
                auto count = read<std::uint16_t>(bank, add(current, 0x24, 0x43120e), 0x43120e);
                auto header = model.pointer(bank, current, 0x34, 0x431217); // also count0
                while (count != 0)
                {
                    const auto material = read<std::uint8_t>(bank, add(header, 1, 0x431227), 0x431227);
                    if (material >= transparency.bytes.size())
                        throw SourceAllocationError(0x43122a,
                                                    "UV material byte reaches outside the actual255-byte owner");
                    if (transparency.bytes[material] != 0)
                    {
                        const auto indices = model.pointer(bank, model.descriptor, 0x38, 0x431234);
                        const auto first = read<std::uint16_t>(bank, add(header, 6, 0x431239), 0x431239);
                        auto index = add(indices, std::uint64_t(first) * 2u, 0x43123d);
                        auto corners = read<std::uint8_t>(bank, header, 0x431242);
                        if (corners != 0)
                        {
                            const auto coordinates = model.pointer(bank, current, 0x2c, 0x431248);
                            do
                            {
                                const auto coordinate = read<std::uint16_t>(bank, index, 0x43124f);
                                fix_source_uv_coordinates_4310a0(
                                    bank, add(coordinates, std::uint64_t(coordinate) * 16u, 0x431255));
                                index = add(index, 2, 0x43125c);
                                --corners;
                            } while (corners != 0);
                        }
                    }
                    header = add(header, 8, 0x43126e);
                    --count;
                }
            }
            part_count = model.word(bank, 0x24, 0x431283);
            ++part;
        }
        const auto maximum = model.word(bank, 0x58, 0x43129b);
        ++lod;
        if (lod >= maximum + 1u)
            break;
    } while (true);
}
} // namespace
void fix_source_uv_coordinates_4310a0(SourceAllocationBank &bank, SourceAddressLocation coordinate)
{
    for (W lane = 0; lane < 2; ++lane)
    {
        const W site = lane == 0 ? 0x4310a3u : 0x431122u;
        const auto bytes = reached(bank, add(coordinate, lane * 4u, site), 4, site);
        W bits;
        std::memcpy(&bits, bytes.data(), 4);
        if ((bits & 0x7f800000u) == 0x7f800000u)
            throw SourceAllocationError(site, "nonfinite UV requires the original CRT exceptional dispatcher");
        // Actual borrowed field, not a temporary float. Source may add then
        // subtract, with the original binary32 store visible between the two.
        sgn_source_uv_coordinate_4310a0(bytes.data());
    }
}
void fix_source_scene_uv_4312c0(RaceSessionPlatform &platform, const MaterialTransparencyStorage &transparency)
{
    platform.require_owner_thread();
    auto &world = platform.state();
    if (world.boot_config.uv_fix == 0)
        return;
    auto &scene = platform.source_scene_resources();
    auto &bank = platform.source_allocations();
    W slot = world.outer.device.initialized_6244f8 == 1 ? 2u : 0u;
    for (;; ++slot)
    {
        const auto records = scene.source_records();
        if (slot == records.size())
        {
            if (scene.material_registry_terminal_636518() == 0)
                return;
            throw SourceAllocationError(0x4312e4, "UV traversal reached an unrepresented nonnull registry terminal");
        }
        if (slot > records.size())
            throw SourceAllocationError(0x4312e4, "UV registry outside actual owner");
        const auto &descriptor = records[slot].descriptor;
        if (std::holds_alternative<std::monostate>(descriptor))
            return;
        if (const auto *strip = std::get_if<VehicleStripGeometry *>(&descriptor))
        {
            if (*strip == nullptr)
                return;
            throw SourceAllocationError(0x4312e4,
                                        "UV traversal requires a source descriptor, not a native strip mirror");
        }
        Model model;
        if (const auto *pmd = std::get_if<ScenePmdBinding>(&descriptor))
        {
            const auto location = scene.models().source_at(pmd->handle).descriptor().location();
            model.descriptor = {location.allocation, location.offset};
        }
        else if (const auto *strip = std::get_if<SceneStripBinding>(&descriptor))
        {
            if (strip->owner != &world.strip)
                throw SourceAllocationError(0x4312e4, "source strip UV owner belongs to another world");
            if (!strip->source_descriptor)
                return;
            model.descriptor = {strip->source_descriptor, 0};
            model.token_pointers = true;
        }
        else if (const auto *sprite = std::get_if<SceneSpriteBinding>(&descriptor))
        {
            scene.require_sprite_binding(world.model_draws, world.sprite_packets);
            if (!sprite->source_descriptor)
                throw SourceAllocationError(0x4312e4, "UV sprite lacks its actual source descriptor");
            model.descriptor = {sprite->source_descriptor, 0};
            model.token_pointers = true;
        }
        else
            throw SourceAllocationError(0x4312e4, "unrepresented UV descriptor kind");
        model_uv_4311c0(bank, model, transparency);
    }
}
} // namespace sgn::retail
