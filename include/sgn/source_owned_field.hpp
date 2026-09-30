#pragma once
#include "sgn/source_world_span.hpp"
#include <cstring>
namespace sgn::retail
{
// One native value OR a live field in the canonical source allocation.
// Bytes are copied only for a synchronous read/write; no retained value mirror.
template <class T> class SourceOwnedField
{
    static_assert(std::is_trivially_copyable_v<T>);
    std::variant<T, SourceWorldSpan<std::uint8_t>> value_;

  public:
    SourceOwnedField(T value = {}) : value_(value) {}
    void bind_source(const SourceWorldSpan<std::uint8_t> &bytes, std::uint32_t offset)
    {
        value_ = bytes.source_subspan(offset, sizeof(T));
    }
    operator T() const
    {
        if (const auto *v = std::get_if<T>(&value_))
            return *v;
        const auto bytes = std::get<1>(value_).view();
        T v;
        std::memcpy(&v, bytes.data(), sizeof(T));
        return v;
    }
    SourceOwnedField &operator=(T v)
    {
        if (auto *native = std::get_if<T>(&value_))
            *native = v;
        else
        {
            const auto bytes = std::get<1>(value_).view();
            std::memcpy(bytes.data(), &v, sizeof(T));
        }
        return *this;
    }
    SourceOwnedField &operator++()
    {
        return *this = T(T(*this) + 1);
    }
};
// Source descriptor+0C is canonical. Native queued transforms are not encoded
// by this initializer; reject that later producer until its transport is ported.
template <class T> class SourceOwnedOuterPointer
{
    std::variant<const T *, SourceWorldSpan<std::uint8_t>> value_{static_cast<const T *>(nullptr)};

  public:
    void bind_source(const SourceWorldSpan<std::uint8_t> &bytes)
    {
        value_ = bytes.source_subspan(0xc, 4);
    }
    operator const T *() const
    {
        if (const auto *native = std::get_if<0>(&value_))
            return *native;
        std::uint32_t word;
        const auto bytes = std::get<1>(value_).view();
        std::memcpy(&word, bytes.data(), 4);
        if (word)
            throw SourceAllocationError(0x42af30, "source sprite outer transform transport is not implemented");
        return nullptr;
    }
    SourceOwnedOuterPointer &operator=(const T *pointer)
    {
        if (auto *native = std::get_if<0>(&value_))
            *native = pointer;
        else
        {
            if (pointer)
                throw SourceAllocationError(0x42af30, "native sprite pose cannot overwrite source descriptor");
            const std::uint32_t zero = 0;
            const auto bytes = std::get<1>(value_).view();
            std::memcpy(bytes.data(), &zero, 4);
        }
        return *this;
    }
};
} // namespace sgn::retail
