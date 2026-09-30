#include "sgn/source_world_span.hpp"
#include "sgn/race_session_platform.hpp"
namespace sgn::retail
{
SourceWorldBufferBinding bind_source_world_buffer(std::weak_ptr<RaceSessionPlatform> borrow,
                                                  SourceAllocationRef allocation, std::uint32_t bytes)
{
    const auto platform = borrow.lock();
    if (!platform)
        throw SourceAllocationError(0x42e430, "source world binding platform expired");
    platform->require_owner_thread();
    return {std::move(borrow), allocation, &platform->source_allocations(), bytes};
}
} // namespace sgn::retail
