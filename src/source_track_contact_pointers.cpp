#include "sgn/source_track_contact_pointers.hpp"
#include "sgn/race_session.hpp"
#include "sgn/scene_resources.hpp"

namespace sgn::retail
{
void initialize_source_track_contact_pointers_401d40(RaceSessionPlatform &platform)
{
    platform.require_owner_thread();
    if (!platform.source_construction_only_entry())
        throw SourceAllocationError(0x401d40, "track contact pointers require the actual source platform");
    auto &world = platform.state();
    if (world.scene_mode == 2)
        return; // 401D40..47; no track/model/payload access
    auto &scene = platform.source_scene_resources();
    const auto handle = scene.model_handle(3); // 401D49, current636448
    if (!handle)
        throw SourceAllocationError(0x401d4e, "track contact publication reached a null PMD descriptor");
    const auto model = scene.models().source_at(*handle);
    const auto part_reference = model.descriptor().field<std::uint32_t>(0x44).value(); // 401D4E
    // Use actual42ECA0 relocation history. A logical part count changed since
    // load cannot redirect the captured pointer to another part or suppress it.
    const auto part = model.captured_base_part_42b055(part_reference);
    const auto location = part.location();
    const auto record = model.bytes().subview(location.offset, 0x40);
    const auto attributes = record.field<std::uint32_t>(0x34).value();  // 401D51
    const auto coordinates = record.field<std::uint32_t>(0x2c).value(); // 401D54
    const auto vertices = record.field<std::uint32_t>(0x30).value();    // 401D57
    const auto pointer = [&](std::uint32_t offset) {
        return offset == 0 ? SourceAddressLocation{} : SourceAddressLocation{location.allocation, offset};
    };
    auto &target = world.contact_cast.internals;
    target.track_attributes_498238 = pointer(attributes);   // 401D5A
    target.track_coordinates_498230 = pointer(coordinates); // 401D60
    target.track_vertices_498234 = pointer(vertices);       // 401D66
}
} // namespace sgn::retail
