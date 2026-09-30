#pragma once
#include "sgn/source_allocation_bank.hpp"
#include <array>
#include <limits>
#include <memory>
#include <span>
#include <type_traits>
#include <variant>
#include <vector>
#include <utility>

namespace sgn::retail
{
class RaceSessionPlatform;
struct SourceWorldBufferBinding
{
    std::weak_ptr<RaceSessionPlatform> platform;
    SourceAllocationRef allocation;
    SourceAllocationBank *bank{}; // borrow only; weak platform lock guards its lifetime
    std::uint32_t bytes{};        // whole allocation extent
    std::uint32_t byte_offset{};
    std::uint32_t byte_count{std::numeric_limits<std::uint32_t>::max()};
};
// Factory belongs to race_session; consumers only need the bank and weak guard.
SourceWorldBufferBinding bind_source_world_buffer(std::weak_ptr<RaceSessionPlatform>, SourceAllocationRef,
                                                  std::uint32_t);
// Synchronous checked borrow: locks platform lifetime before touching its bank.
std::span<std::uint8_t> source_world_buffer_bytes(const SourceWorldBufferBinding &);

// One active representation of the existing field: borrowed native array OR
// one real source-bank allocation. This class owns no payload or pointer copy.
template <class T> class SourceWorldSpan
{
    static_assert(std::is_trivially_copyable_v<T> && std::is_standard_layout_v<T>);
    std::variant<std::span<T>, SourceWorldBufferBinding> binding_{std::span<T>{}};

  public:
    SourceWorldSpan() = default;
    SourceWorldSpan(std::span<T> view) : binding_(view) {}
    template <std::size_t N> SourceWorldSpan(std::array<T, N> &view) : binding_(std::span<T>(view)) {}
    SourceWorldSpan(std::vector<T> &view) : binding_(std::span<T>(view)) {}
    static SourceWorldSpan source(std::weak_ptr<RaceSessionPlatform> platform, SourceAllocationRef allocation,
                                  std::uint32_t bytes)
    {
        if (bytes % sizeof(T) != 0)
            throw SourceAllocationError(0x42e430, "source world buffer stride mismatch");
        SourceWorldSpan result;
        result.binding_ = bind_source_world_buffer(std::move(platform), allocation, bytes);
        return result; // publication does not dereference/null-test the returned source pointer
    }
    // Source address arithmetic without dereferencing the allocation. NULL or
    // released ownership remains representable; the later read diagnoses it.
    SourceWorldSpan source_subspan(std::size_t offset, std::size_t count) const
    {
        if (!source_bound())
            throw SourceAllocationError(0x427275, "source slice requires source owner");
        auto region = source_binding();
        const auto extent =
            region.byte_count == std::numeric_limits<std::uint32_t>::max() ? region.bytes : region.byte_count;
        if (offset > extent / sizeof(T) || count > extent / sizeof(T) - offset)
            throw SourceAllocationError(0x427275, "source slice outside retained allocation extent");
        region.byte_offset += static_cast<std::uint32_t>(offset * sizeof(T));
        region.byte_count = static_cast<std::uint32_t>(count * sizeof(T));
        SourceWorldSpan result;
        result.binding_ = std::move(region);
        return result;
    }
    bool source_bound() const noexcept
    {
        return std::holds_alternative<SourceWorldBufferBinding>(binding_);
    }
    const SourceWorldBufferBinding &source_binding() const
    {
        if (!source_bound())
            throw SourceAllocationError(0x42e430, "native buffer has no source identity");
        return std::get<SourceWorldBufferBinding>(binding_);
    }
    bool same_source_allocation(const SourceWorldSpan &other) const noexcept
    {
        const auto *a = std::get_if<SourceWorldBufferBinding>(&binding_);
        const auto *b = std::get_if<SourceWorldBufferBinding>(&other.binding_);
        return a && b && a->allocation == b->allocation && a->bytes == b->bytes && a->byte_offset == b->byte_offset &&
               a->byte_count == b->byte_count && !a->platform.owner_before(b->platform) &&
               !b->platform.owner_before(a->platform);
    }
    std::span<T> view() const
    {
        if (const auto *native = std::get_if<std::span<T>>(&binding_))
            return *native;
        const auto bytes = source_world_buffer_bytes(std::get<SourceWorldBufferBinding>(binding_));
        if (reinterpret_cast<std::uintptr_t>(bytes.data()) % alignof(T) != 0)
            throw SourceAllocationError(0x42e430, "source world buffer alignment mismatch");
        return {reinterpret_cast<T *>(bytes.data()), bytes.size() / sizeof(T)};
    }
    operator std::span<T>() const
    {
        return view();
    }
    std::size_t size() const
    {
        return view().size();
    }
    bool empty() const
    {
        return view().empty();
    }
    T *data() const
    {
        return view().data();
    }
    auto begin() const
    {
        return view().begin();
    }
    auto end() const
    {
        return view().end();
    }
    T &operator[](std::size_t index) const
    {
        const auto bytes = view();
        if (index >= bytes.size())
            throw SourceAllocationError(0x42e430, "source world buffer index outside owner");
        return bytes[index];
    }
    std::span<T> first(std::size_t count) const
    {
        return subspan(0, count);
    }
    std::span<T> subspan(std::size_t offset, std::size_t count = std::dynamic_extent) const
    {
        const auto bytes = view();
        if (offset > bytes.size() || (count != std::dynamic_extent && count > bytes.size() - offset))
            throw SourceAllocationError(0x42e430, "source world buffer subspan outside owner");
        return bytes.subspan(offset, count);
    }
};
} // namespace sgn::retail
