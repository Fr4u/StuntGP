#pragma once
#include "sgn/source_scene_static_pointer.hpp"
#include "sgn/source_scene_allocation_pointer.hpp"
#include <array>
#include <optional>
#include <variant>
#include <vector>

namespace sgn::retail
{
using SourceSelectedLight = std::variant<std::monostate, SourceSceneStaticPointer, SourceSceneAllocationPointer>;
// One representation of624534..624573. Native compatibility methods keep
// their existing optional-index reference types, but reject a source list.
// Source slots contain weak pointers to actual records, never copied lights.
class LightSelectionStorage
{
    using Native = std::vector<std::optional<std::uint32_t>>;
    using Source = std::array<SourceSelectedLight, 16>;
    std::variant<Native, Source> storage_;
    Native &native()
    {
        if (source_bound())
            throw FormatError("source light pointer list is not a native index list");
        return std::get<0>(storage_);
    }
    const Native &native() const
    {
        if (source_bound())
            throw FormatError("source light pointer list is not a native index list");
        return std::get<0>(storage_);
    }

  public:
    LightSelectionStorage() = default;
    LightSelectionStorage(std::initializer_list<std::optional<std::uint32_t>> values) : storage_(Native(values)) {}
    bool source_bound() const noexcept
    {
        return storage_.index() == 1;
    }
    // NEW representation admission of the untouched PE-zero static bank.
    // An already written native list is never converted, cleared or guessed.
    void bind_initial_source()
    {
        if (source_bound())
            return;
        if (!native().empty())
            throw FormatError("cannot replace written native light list with source pointers");
        storage_.emplace<1>();
    }
    SourceSelectedLight source_at(std::uint32_t index) const
    {
        if (!source_bound() || index >= 16)
            throw SourceAllocationError(0x42b490, "source light pointer outside represented16-word bank");
        return std::get<1>(storage_)[index];
    }
    void publish_source(std::uint32_t index, SourceSelectedLight pointer)
    {
        if (!source_bound() || index >= 16)
            throw SourceAllocationError(0x42dcad, "source light list write reaches independent624574");
        std::get<1>(storage_)[index] = std::move(pointer);
    }
    std::size_t size() const noexcept
    {
        return source_bound() ? 16 : std::get<0>(storage_).size();
    }
    bool empty() const
    {
        return native().empty();
    }
    void resize(std::size_t count)
    {
        native().resize(count);
    }
    void clear()
    {
        native().clear();
    }
    auto &operator[](std::size_t index)
    {
        return native()[index];
    }
    const auto &operator[](std::size_t index) const
    {
        return native()[index];
    }
    auto &at(std::size_t index)
    {
        return native().at(index);
    }
    const auto &at(std::size_t index) const
    {
        return native().at(index);
    }
    template <class... T> auto &emplace_back(T &&...values)
    {
        return native().emplace_back(std::forward<T>(values)...);
    }
    void push_back(std::optional<std::uint32_t> value)
    {
        native().push_back(value);
    }
    auto begin()
    {
        return native().begin();
    }
    auto end()
    {
        return native().end();
    }
    auto begin() const
    {
        return native().begin();
    }
    auto end() const
    {
        return native().end();
    }
};
} // namespace sgn::retail
