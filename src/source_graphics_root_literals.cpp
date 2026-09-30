#include "sgn/source_application_literals.hpp"
namespace sgn::retail
{
std::uint32_t SourceApplicationLiteralRef::read_model_memory_budget_42e3e0() const
{
    constexpr std::uint32_t site = 0x42e3e0, address = 0x477c30;
    const auto identity = identity_.lock();
    if (!identity)
        throw SourceLiteralReadError(site, address, "expired graphics-root App owner");
    if (std::this_thread::get_id() != identity->thread)
        throw SourceLiteralReadError(site, address, "graphics-root App belongs to another thread");
    return identity->owner->model_memory_budget_477c30_; // SAME DWORD read by444EA2.
}
std::span<std::uint8_t, 16> SourceApplicationLiteralData::mutable_graphics_root_format_477c34() &
{
    require_owner_thread();
    return graphics_root_format_477c34_;
}
} // namespace sgn::retail
