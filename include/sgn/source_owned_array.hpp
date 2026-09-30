#pragma once
#include "sgn/source_world_span.hpp"
namespace sgn::retail
{
// Existing native array OR source-bank payload; never two active payloads.
template <class T, std::size_t N> class SourceOwnedArray
{
    std::variant<std::array<T, N>, SourceWorldSpan<T>> storage_;

  public:
    using value_type = T;
    void bind_source(SourceWorldSpan<T> source)
    {
        if (!source.source_bound())
            throw SourceAllocationError(0x434090, "source array requires source binding");
        storage_.template emplace<1>(std::move(source));
    }
    bool source_bound() const noexcept
    {
        return storage_.index() == 1;
    }
    const SourceWorldSpan<T> &source() const
    {
        if (!source_bound())
            throw SourceAllocationError(0x434090, "native array has no source binding");
        return std::get<1>(storage_);
    }
    std::span<T> view()
    {
        if (auto *native = std::get_if<0>(&storage_))
            return *native;
        auto values = std::get<1>(storage_).view();
        if (values.size() != N)
            throw SourceAllocationError(0x434090, "source array extent mismatch");
        return values;
    }
    std::span<const T> view() const
    {
        if (const auto *native = std::get_if<0>(&storage_))
            return *native;
        auto values = std::get<1>(storage_).view();
        if (values.size() != N)
            throw SourceAllocationError(0x434090, "source array extent mismatch");
        return values;
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
    T &operator[](std::size_t i)
    {
        if (i >= N)
            throw SourceAllocationError(0x434090, "source array index");
        return view()[i];
    }
    const T &operator[](std::size_t i) const
    {
        if (i >= N)
            throw SourceAllocationError(0x434090, "source array index");
        return view()[i];
    }
    void fill(const T &value)
    {
        for (auto &item : view())
            item = value;
    }
    operator std::span<T>()
    {
        return view();
    }
    operator std::span<const T>() const
    {
        return view();
    }
};
} // namespace sgn::retail
