#include "sgn/session_world_materials.hpp"
#include "sgn/race_session.hpp"
#include <bit>

namespace sgn::retail
{
void SessionWorldMaterials::publish_source_count_42e772(const MaterialTextureBackend &backend)
{
    const auto platform = require_registry_live(); // retain this source world during the borrow
    (void)source_material_state_for(*scene_, backend);
    if (!fonts_loaded_)
        throw FormatError("source42E772 requires the preceding428FA0 to have returned");
    // 42E753 reads6243B8, the physical pointer after fonts.58271C counts only
    // ordinary material uploads and cannot replace this input.
    const std::uint32_t cursor = state_.names.materials.source_cursor();
    const auto difference = std::bit_cast<std::int32_t>(cursor - std::uint32_t{0x6227d4});
    // 42E759..42E770 implement signed division by28, truncated toward zero.
    // Unsigned subtraction above preserves the x86 DWORD subtraction first.
    const auto count = difference / std::int32_t{28};
    state_.published_material_count = std::bit_cast<std::uint32_t>(count); // 42E772, sole6243BC
    published_ = true; // host publication bookkeeping; source_registry_only_ remains true
    // No fonts/material reload, pointer advance, ticket creation or later42E430.
}
} // namespace sgn::retail
