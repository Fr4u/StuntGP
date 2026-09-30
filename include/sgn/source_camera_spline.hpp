#pragma once
#include "sgn/source_allocation_bank.hpp"
#include "sgn/retail_spline.hpp"
#include <memory>
#include <optional>
#include <variant>
namespace sgn::retail
{
class RaceSessionPlatform;
// Captured624474+28*index identity. No descriptor copy, model-header reread,
// payload span, or lifetime extension. Each reached read checks the bank generation.
class SourceCameraSplineRef final
{
    std::weak_ptr<RaceSessionPlatform> platform_;
    SourceAllocationRef allocation_;
    std::uint32_t offset_{};

  public:
    SourceCameraSplineRef(std::weak_ptr<RaceSessionPlatform> p, SourceAllocationRef a, std::uint32_t o)
        : platform_(p), allocation_(a), offset_(o)
    {
    }
    std::uint32_t read_word(std::uint64_t relative, std::uint32_t site) const;
    // PMD descriptor+4 is a normalized pointer within this same allocation.
    // Constructing its identity reads no payload. Null fails only when reached.
    SourceCameraSplineRef at_normalized_offset(std::uint32_t offset) const noexcept
    {
        return SourceCameraSplineRef(platform_, offset ? allocation_ : SourceAllocationRef{}, offset);
    }
    const SourceAllocationRef &allocation() const noexcept
    {
        return allocation_;
    }
    std::uint32_t offset() const noexcept
    {
        return offset_;
    }
    // NEW R18 (DESIGN A1): the weak owner for call-scoped reader resolution
    // (spline_resolution.hpp). Neither locks nor extends the platform lifetime.
    const std::weak_ptr<RaceSessionPlatform> &platform() const noexcept
    {
        return platform_;
    }
};
std::optional<SourceCameraSplineRef> find_source_camera_spline_428800(const std::weak_ptr<RaceSessionPlatform> &,
                                                                      std::uint32_t id);
// One source pointer field with disjoint native/source representations.
// Existing native consumers may borrow only the native alternative. A source
// binding never turns into an unchecked SplineView or a fabricated null.
class CameraSplineBinding final
{
    std::variant<std::optional<SplineView>, SourceCameraSplineRef> value_;

  public:
    CameraSplineBinding() = default;
    CameraSplineBinding(std::optional<SplineView> value) : value_(value) {}
    CameraSplineBinding(SplineView value) : value_(std::optional<SplineView>{value}) {}
    CameraSplineBinding &operator=(std::optional<SplineView> value)
    {
        value_ = value;
        return *this;
    }
    CameraSplineBinding &operator=(SplineView value)
    {
        value_ = std::optional<SplineView>{value};
        return *this;
    }
    void bind_source(std::optional<SourceCameraSplineRef> value)
    {
        if (value)
            value_ = *value;
        else
            reset();
    }
    void reset() noexcept
    {
        value_ = std::optional<SplineView>{};
    }
    bool has_value() const noexcept
    {
        return value_.index() == 1 || std::get<0>(value_).has_value();
    }
    explicit operator bool() const noexcept
    {
        return has_value();
    }
    const std::optional<SplineView> &native() const
    {
        if (value_.index() != 0)
            throw SourceAllocationError(0x428800, "source camera spline requires its live allocation receiver");
        return std::get<0>(value_);
    }
    operator const std::optional<SplineView> &() const
    {
        return native();
    }
    const SplineView &operator*() const
    {
        return native().value();
    }
    const SplineView *operator->() const
    {
        return &native().value();
    }
    const SourceCameraSplineRef *source() const noexcept
    {
        return std::get_if<1>(&value_);
    }
};
} // namespace sgn::retail
