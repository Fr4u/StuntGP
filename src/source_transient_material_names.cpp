#include "sgn/source_application_literals.hpp"
namespace sgn::retail
{
std::span<const std::string *const> SourceApplicationLiteralRef::transient_material_names_441180() const &
{
    const auto identity = identity_.lock();
    if (!identity)
        throw SourceLiteralReadError(0x441180, 0x480134, "expired App transient material names");
    if (std::this_thread::get_id() != identity->thread)
        throw SourceLiteralReadError(0x441180, 0x480134, "App transient material names belong to another thread");
    // 441180 returns the ADDRESS of the list, not its first DWORD. Preserve one
    // addressable null slot so the existing consumer reads its actual terminator.
    return {&identity->owner->transient_material_terminal_480134_, 1};
}
} // namespace sgn::retail
