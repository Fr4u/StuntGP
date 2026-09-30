#include "sgn/race_session.hpp"
#include "sgn/model_resources.hpp"
#include <variant>
#include <cstring>
#include <limits>

extern "C" void sgn_source_light_record_42d756(void *) noexcept;
extern "C" void sgn_source_light_ambient_42d7e0(void *) noexcept;

namespace sgn::retail
{
namespace
{
using LightPointer = std::variant<SourcePmdLocation, SourceSceneStaticPointer>;
std::uint32_t add(std::uint32_t left, std::uint32_t right, std::uint32_t site)
{
    if (right > std::numeric_limits<std::uint32_t>::max() - left)
        throw SourceAllocationError(site, "source light pointer arithmetic exceeds represented allocation");
    return left + right;
}
template <class T>
T read(const std::shared_ptr<RaceSessionPlatform> &platform, const LightPointer &pointer, std::uint32_t offset,
       std::uint32_t site)
{
    if (const auto *pmd = std::get_if<SourcePmdLocation>(&pointer))
        return SourceSceneAllocationPointer::bind(platform, platform->source_allocations(),
                                                  {pmd->allocation, pmd->offset})
            .read<T>(offset, site);
    return std::get<SourceSceneStaticPointer>(pointer).read<T>(offset);
}
std::span<std::uint8_t> light_record(const std::shared_ptr<RaceSessionPlatform> &platform, const LightPointer &pointer,
                                     std::uint32_t offset)
{
    if (const auto *pmd = std::get_if<SourcePmdLocation>(&pointer))
    {
        const auto position = add(pmd->offset, offset, 0x42d756);
        auto bytes = platform->source_allocations().bytes(pmd->allocation);
        // Host admission for the numeric leaf: one complete original144-byte
        // light record in the SAME allocation. No copied light vector/owner.
        if (position > bytes.size() || 0x90u > bytes.size() - position)
            throw SourceAllocationError(0x42d756, "source light record exceeds actual PMD allocation");
        return bytes.subspan(position, 0x90);
    }
    auto &actual = platform->state().lights.source_menu_light_589628;
    auto bytes = std::span<std::uint8_t>{reinterpret_cast<std::uint8_t *>(&actual), sizeof(actual)};
    const auto expected = SourceSceneStaticPointer::bind(platform, 0x589628, bytes);
    if (offset != 0 || std::get<SourceSceneStaticPointer>(pointer) != expected)
        throw SourceAllocationError(0x42d756, "source light does not name the represented static record");
    return bytes;
}
void publish_selected(const std::shared_ptr<RaceSessionPlatform> &platform, const LightPointer &base,
                      std::uint32_t offset)
{
    auto &selected = platform->state().lights.selected;
    if (const auto *pmd = std::get_if<SourcePmdLocation>(&base))
    {
        // 42D996/42D99F only store the pointer. Count0 must not dereference it
        // or invent a null value from an empty host vector.
        selected = SourceSceneAllocationPointer::bind(platform, platform->source_allocations(),
                                                      {pmd->allocation, add(pmd->offset, offset, 0x42d99f)});
    }
    else
    {
        if (offset != 0)
            throw SourceAllocationError(0x42d99f, "selected light leaves represented static object");
        selected = std::get<SourceSceneStaticPointer>(base);
    }
}
} // namespace
void ModelResources::initialize_source_menu_lights_42d700(const std::shared_ptr<RaceSessionPlatform> &platform)
{
    if (!platform)
        throw FormatError("42D700 needs its source world");
    platform->require_owner_thread();
    if (&platform->source_scene_resources().models() != this || !platform->source_construction_only_entry())
        throw FormatError("42D700 requires the same source model/global owner");
    auto &world = platform->state();
    auto &track = source_loading_.track;
    source_loading_.requests.check_thread(0x42d700);
    if (world.scene_mode == 2)
    {
        world.lights.selected = std::optional<std::uint32_t>{}; // 42D712:589E04=NULL
        const auto bytes = [](const auto &object) {
            return std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t *>(&object), sizeof(object));
        };
        track.lights =
            SourceSceneStaticPointer::bind(platform, 0x589628, bytes(world.lights.source_menu_light_589628)); // 42D718
        track.light_count = 0;                  // 42D722, physical static record remains present
        world.light_selection.active_count = 0; // 42D727
        track.ambient =
            SourceSceneStaticPointer::bind(platform, 0x5898d0, bytes(world.lights.authored_ambient)); // 42D72D
        track.texture_motion = SourceSceneStaticPointer::bind(platform, 0x5898f8, {});                // 42D737
    }
    const auto count = track.light_count; // 42D743 (scene!=2), otherwise actual preceding EAX=0
    const auto lights = track.lights;     // 42D749: one captured base for this loop
    std::uint32_t offset = 0;
    for (std::uint32_t remaining = count; remaining != 0; --remaining)
    {
        auto record = light_record(platform, lights, offset);
        sgn_source_light_record_42d756(record.data()); // same actual fields, original x87/FTOL stores
        // 42D7A8 also increments after the final record. Host overflow has no
        // guest-address equivalent and is rejected without rolling back stores.
        offset = add(offset, 0x90, 0x42d7a8);
    }
    const auto ambient = track.ambient; // 42D7B1, after all reached light writes
    auto *destination = reinterpret_cast<std::uint8_t *>(&world.ambient_light);
    for (std::uint32_t word = 0; word < 10; ++word)
    { // 42D7C1: actual sequential ten-DWORD copy
        const auto value = read<std::uint32_t>(platform, ambient, word * 4, 0x42d7c1);
        std::memcpy(destination + word * 4, &value, 4);
    }
    sgn_source_light_ambient_42d7e0(&world.ambient_light); // 42D7C3, original spills and store order
    const auto selection_count = track.light_count;        // 42D970, captured before base
    const auto selection_base = track.lights;              // 42D977
    std::uint32_t selected_offset = 0;
    for (std::uint32_t i = 0; i < selection_count; ++i)
    {
        if (read<std::uint32_t>(platform, selection_base, add(selected_offset, 0x50, 0x42d985), 0x42d985) == 3)
        {
            publish_selected(platform, selection_base, selected_offset);
            return; // 42D99F
        }
        selected_offset = add(selected_offset, 0x90, 0x42d98b);
    }
    publish_selected(platform, selection_base, 0); // 42D996, including count0 and no matching type
}
} // namespace sgn::retail
