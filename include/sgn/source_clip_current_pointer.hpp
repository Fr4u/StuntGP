#pragma once
#include "sgn/source_world_span.hpp"
#include <optional>
namespace sgn::retail
{
// Replaces an existing relative-count field. It is either its native preview
// count OR the actual source current-pointer identity, never both live states.
class SourceClipCurrentPointer
{
    std::variant<std::uint32_t, SourceWorldBufferBinding> current_{std::uint32_t{0}};

  public:
    SourceClipCurrentPointer() = default;
    SourceClipCurrentPointer(std::uint32_t value) : current_(value) {}
    template <class T> static SourceClipCurrentPointer base(const SourceWorldSpan<T> &owner)
    {
        SourceClipCurrentPointer result;
        if (owner.source_bound())
            result.current_ = owner.source_binding();
        return result; // native count0 or source pointer copy; no payload dereference
    }
    bool source_bound() const noexcept
    {
        return current_.index() == 1;
    }
    const SourceWorldBufferBinding &source_pointer() const
    {
        if (!source_bound())
            throw SourceAllocationError(0x427240, "native count has no source current pointer");
        return std::get<1>(current_);
    }
    template <class T> std::uint32_t count_from(const SourceWorldSpan<T> &owner) const
    {
        if (!source_bound())
        {
            if (owner.source_bound())
                throw SourceAllocationError(0x427240, "source current pointer has not been initialized");
            return std::get<0>(current_);
        }
        if (!owner.source_bound())
            throw SourceAllocationError(0x427240, "source current pointer has native base");
        const auto &a = source_pointer();
        const auto &b = owner.source_binding();
        if (!(a.allocation == b.allocation) || a.bytes != b.bytes || a.platform.owner_before(b.platform) ||
            b.platform.owner_before(a.platform))
            throw SourceAllocationError(0x427240, "clip pointer subtraction across different allocations");
        if (a.byte_offset < b.byte_offset || (a.byte_offset - b.byte_offset) % sizeof(T) != 0)
            throw SourceAllocationError(0x427240, "clip pointer subtraction outside typed domain");
        return static_cast<std::uint32_t>((a.byte_offset - b.byte_offset) / sizeof(T));
    }
    template <class T> SourceClipCurrentPointer advanced(std::uint32_t elements) const
    {
        SourceClipCurrentPointer result = *this;
        if (!source_bound())
        {
            const auto value = std::get<0>(current_);
            if (elements > std::numeric_limits<std::uint32_t>::max() - value)
                throw SourceAllocationError(0x427770, "native clip count overflow");
            result.current_ = value + elements;
            return result;
        }
        auto pointer = source_pointer();
        const auto remaining = pointer.byte_count == std::numeric_limits<std::uint32_t>::max()
                                   ? pointer.bytes - pointer.byte_offset
                                   : pointer.byte_count;
        if (elements > remaining / sizeof(T))
            throw SourceAllocationError(0x427770, "clip current advance outside allocation");
        const auto bytes = static_cast<std::uint32_t>(elements * sizeof(T));
        pointer.byte_offset += bytes;
        pointer.byte_count = remaining - bytes;
        result.current_ = std::move(pointer);
        return result;
    }
    // Legacy native-only arithmetic cannot silently replace source pointers.
    operator std::uint32_t() const
    {
        if (source_bound())
            throw SourceAllocationError(0x427240, "source clip cursor requires an explicit base");
        return std::get<0>(current_);
    }
    SourceClipCurrentPointer &operator=(std::uint32_t value)
    {
        if (source_bound())
            throw SourceAllocationError(0x427240, "native count store cannot replace source pointer");
        current_ = value;
        return *this;
    }
    std::uint32_t operator++(int)
    {
        const auto value = static_cast<std::uint32_t>(*this);
        current_ = value + 1u;
        return value;
    }
};
} // namespace sgn::retail
