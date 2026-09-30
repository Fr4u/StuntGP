#pragma once
#include "sgn/model_resources.hpp"
#include <utility>
#include <variant>

namespace sgn::retail
{
// Independent source pointer words, not ownership of model/geometry payloads.
// Native bindings borrow the existing ModelResource; source bindings retain
// only checked weak PMD views. Clearing one word never changes the other.
class SourceTrackModelPointer
{
    struct Source
    {
        SourcePmdModelView owner;
        SourcePmdByteView descriptor;
    };
    std::variant<ModelResource *, Source> value_{static_cast<ModelResource *>(nullptr)};

  public:
    SourceTrackModelPointer() = default;
    SourceTrackModelPointer(ModelResource *value) noexcept : value_(value) {}
    static SourceTrackModelPointer source(SourcePmdModelView value)
    {
        // Capture the actual descriptor, not a promise to re-read the PMD
        // header's descriptor offset on the next access to61C52C.
        const auto descriptor = value.descriptor();
        return captured_source(value, descriptor);
    }
    static SourceTrackModelPointer captured_source(SourcePmdModelView owner, SourcePmdByteView descriptor)
    {
        if (owner.allocation() != descriptor.location().allocation)
            throw FormatError("captured source descriptor belongs to another PMD");
        SourceTrackModelPointer result;
        result.value_.emplace<1>(Source{owner, descriptor});
        return result;
    }
    bool source_bound() const noexcept
    {
        return value_.index() == 1;
    }
    bool is_null() const noexcept
    {
        return !source_bound() && std::get<0>(value_) == nullptr;
    }
    explicit operator bool() const noexcept
    {
        return !is_null();
    }
    void clear() noexcept
    {
        value_.emplace<0>(nullptr);
    }
    ModelResource *native_model() const
    {
        if (source_bound())
            throw FormatError("61C52C contains a source PMD borrow, not a native model");
        return std::get<0>(value_);
    }
    SourcePmdByteView source_descriptor() const
    {
        if (!source_bound())
            throw FormatError("61C52C has no source PMD binding");
        const auto value = std::get<1>(value_).descriptor;
        (void)value.size();
        return value;
    }
    // Metadata only: paired with the independently captured descriptor;
    // no header+4C reread and no copied geometry or guessed scene slot.
    SourcePmdModelView source_model_owner() const
    {
        if (!source_bound())
            throw FormatError("61C52C has no source PMD owner");
        const auto owner = std::get<1>(value_).owner;
        owner.check();
        return owner;
    }
    bool operator==(const ModelResource *value) const noexcept
    {
        return !source_bound() && std::get<0>(value_) == value;
    }
};

class SourceTrackPartPointer
{
  public:
    // Stable identity within the retained native owner, not copied part data.
    struct Native
    {
        ModelResource *owner{};
        unsigned lod{}, part{};
    };

  private:
    // Keep the captured descriptor+44 DWORD. Resolving against a subsequent
    // descriptor+44 value would change the independent61C530 publication.
    struct Source
    {
        SourcePmdModelView owner;
        std::uint32_t captured_reference;
    };
    std::variant<Native, Source> value_{Native{}};

  public:
    SourceTrackPartPointer() = default;
    static SourceTrackPartPointer native(ModelResource &owner, unsigned lod, unsigned part) noexcept
    {
        SourceTrackPartPointer result;
        result.value_.emplace<0>(Native{&owner, lod, part});
        return result;
    }
    static SourceTrackPartPointer source(SourcePmdModelView owner, std::uint32_t captured_reference)
    {
        SourceTrackPartPointer result;
        result.value_.emplace<1>(Source{std::move(owner), captured_reference});
        return result;
    }
    bool source_bound() const noexcept
    {
        return value_.index() == 1;
    }
    bool is_null() const noexcept
    {
        return source_bound() ? std::get<1>(value_).captured_reference == 0 : std::get<0>(value_).owner == nullptr;
    }
    explicit operator bool() const noexcept
    {
        return !is_null();
    }
    void clear() noexcept
    {
        value_.emplace<0>(Native{});
    }
    Native native_part() const
    {
        if (source_bound())
            throw FormatError("61C530 contains a source PMD borrow, not a native part");
        return std::get<0>(value_);
    }
    SourcePmdLocation source_location() const
    {
        if (!source_bound())
            throw FormatError("61C530 has no source PMD binding");
        const auto &value = std::get<1>(value_);
        if (value.captured_reference == 0)
            return {};
        return {value.owner.allocation(), value.captured_reference};
    }
    SourcePmdPartView source_part() const
    {
        if (!source_bound() || is_null())
            throw FormatError("61C530 has no nonnull source PMD part");
        const auto &value = std::get<1>(value_);
        // Existing resolver validates actual42ECA0 relocation history and bank
        // generation at dereference. Publication itself does not dereference.
        return value.owner.captured_base_part_42b055(value.captured_reference);
    }
};
} // namespace sgn::retail
