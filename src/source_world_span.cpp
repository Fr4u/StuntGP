#include "sgn/source_world_span.hpp"

namespace sgn::retail
{
std::span<std::uint8_t> source_world_buffer_bytes(const SourceWorldBufferBinding &binding)
{
    const auto platform = binding.platform.lock();
    if (!platform)
        throw SourceAllocationError(0x42e430, "source world buffer outlived its platform");
    // Null allocation is a retained source pointer, not an empty successful array.
    if (!binding.bank)
        throw SourceAllocationError(0x42e430, "source world buffer has no bank binding");
    auto bytes = binding.bank->bytes(binding.allocation); // same bank checks its owner thread
    if (bytes.size() != binding.bytes)
        throw SourceAllocationError(0x42e430, "source world buffer allocation extent changed");
    const auto count =
        binding.byte_count == std::numeric_limits<std::uint32_t>::max() ? binding.bytes : binding.byte_count;
    if (binding.byte_offset > bytes.size() || count > bytes.size() - binding.byte_offset)
        throw SourceAllocationError(0x427275, "source world region outside allocation");
    return bytes.subspan(binding.byte_offset, count);
}
} // namespace sgn::retail
