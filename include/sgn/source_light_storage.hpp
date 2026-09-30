#pragma once
#include "sgn/source_world_span.hpp"
namespace sgn::retail
{
// Host storage representation: existing vector OR the sole source-bank payload.
class SourceLightStorage
{
    std::variant<std::vector<std::uint8_t>, SourceWorldSpan<std::uint8_t>> storage_;

  public:
    using value_type = std::uint8_t;
    bool source_bound() const noexcept
    {
        return storage_.index() == 1;
    }
    const SourceWorldBufferBinding &source_binding() const
    {
        if (!source_bound())
            throw SourceAllocationError(0x42b478, "light workspace is not source-backed");
        return std::get<1>(storage_).source_binding();
    }
    void bind_source(SourceWorldSpan<std::uint8_t> value)
    {
        if (!value.source_bound())
            throw SourceAllocationError(0x42e508, "source light binding required");
        storage_.emplace<1>(std::move(value));
    }
    void resize(std::size_t size)
    {
        if (source_bound())
            throw SourceAllocationError(0x42e508, "cannot resize source light allocation");
        std::get<0>(storage_).resize(size);
    }
    bool is_null() const
    {
        return source_bound() ? !static_cast<bool>(std::get<1>(storage_).source_binding().allocation)
                              : std::get<0>(storage_).data() == nullptr;
    }
    std::span<std::uint8_t> view()
    {
        if (source_bound())
            return std::get<1>(storage_).view();
        return std::get<0>(storage_);
    }
    std::span<const std::uint8_t> view() const
    {
        if (source_bound())
            return std::get<1>(storage_).view();
        return std::get<0>(storage_);
    }
    auto data()
    {
        return view().data();
    }
    auto data() const
    {
        return view().data();
    }
    auto begin()
    {
        return view().begin();
    }
    auto begin() const
    {
        return view().begin();
    }
    auto end()
    {
        return view().end();
    }
    auto end() const
    {
        return view().end();
    }
    auto size() const
    {
        return view().size();
    }
    operator std::span<std::uint8_t>()
    {
        return view();
    }
    operator std::span<const std::uint8_t>() const
    {
        return view();
    }
};
} // namespace sgn::retail
