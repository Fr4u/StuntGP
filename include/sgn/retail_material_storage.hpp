#pragma once
#include "sgn/retail_polygon_submission.hpp"
#include <limits>

namespace sgn::retail
{
// Sole owner of source6227D4..6243B7 (255 records, seven DWORDs each).
// The contiguous render projection and remaining words partition that storage;
// there is no second mutable record copy. New host layout/lifetime checks.
class PhysicalMaterialStorage final
{
    std::array<PolygonSubmissionMaterial, 255> render_{};
    std::array<std::uint32_t, 255> word08_{};
    std::array<std::array<std::uint32_t, 3>, 255> other_{};
    std::uint32_t cursor_{};     // Sole normalized6243B8 index when active.
    bool active_{};              // Source null is distinct from an active base pointer.
    std::uint64_t generation_{}; // Host reset lifetime, not a source counter.
    static void require(bool ok, const char *why)
    {
        if (!ok)
            throw FormatError(why);
    }
    void reset(bool active)
    {
        require(generation_ != std::numeric_limits<std::uint64_t>::max(), "physical material generation exhausted");
        render_ = {};
        word08_ = {};
        other_ = {};
        cursor_ = 0;
        active_ = active;
        ++generation_;
    }

  public:
    static constexpr std::uint32_t capacity = 255;
    PhysicalMaterialStorage() = default;
    PhysicalMaterialStorage(const PhysicalMaterialStorage &) = delete;
    PhysicalMaterialStorage &operator=(const PhysicalMaterialStorage &) = delete;
    PhysicalMaterialStorage(PhysicalMaterialStorage &&) = delete;
    PhysicalMaterialStorage &operator=(PhysicalMaterialStorage &&) = delete;
    // 42E5DE..42E5FA: full clear, then null.42C4A8..42C4B4: full clear, then base.
    // Neither operation owns or changes publication6243BC or ordinary58271C.
    void reset_scene_42e510()
    {
        reset(false);
    }
    void reset_loading_42c480()
    {
        reset(true);
    }
    bool active() const noexcept
    {
        return active_;
    }
    std::uint64_t generation() const noexcept
    {
        return generation_;
    }
    std::uint32_t size() const
    {
        require(cursor_ <= capacity, "physical material cursor outside owner");
        require(active_ || cursor_ == 0, "inactive physical material cursor changed");
        return active_ ? cursor_ : 0;
    }
    bool empty() const
    {
        return size() == 0;
    }
    std::uint32_t source_cursor() const
    {
        return active_ ? 0x6227d4u + 28u * size() : 0u;
    }
    std::uint32_t next_index() const
    {
        require(active_, "physical material cursor is not initialized");
        require(size() < capacity, "physical material next-free record outside owner");
        return cursor_;
    }
    std::uint32_t &active_cursor() &
    {
        require(active_, "physical material cursor is not initialized");
        (void)size();
        return cursor_;
    }
    std::uint32_t &active_cursor() && = delete;
    PolygonSubmissionMaterial &record(std::size_t slot) &
    {
        require(slot < capacity, "physical material slot outside owner");
        return render_[slot];
    }
    const PolygonSubmissionMaterial &record(std::size_t slot) const &
    {
        require(slot < capacity, "physical material slot outside owner");
        return render_[slot];
    }
    PolygonSubmissionMaterial &record(std::size_t) && = delete;
    const PolygonSubmissionMaterial &record(std::size_t) const && = delete;
    std::uint32_t &source_word(std::size_t slot, std::uint32_t byte_offset) &
    {
        require(slot < capacity, "physical material slot outside owner");
        switch (byte_offset)
        {
        case 0:
            return render_[slot].primary_texture;
        case 4:
            return render_[slot].alternate_texture;
        case 8:
            return word08_[slot];
        case 12:
            return other_[slot][0];
        case 16:
            return other_[slot][1];
        case 20:
            return render_[slot].motion_index;
        case 24:
            return other_[slot][2];
        default:
            throw FormatError("physical material word offset outside source record");
        }
    }
    std::uint32_t source_word(std::size_t slot, std::uint32_t byte_offset) const &
    {
        require(slot < capacity, "physical material slot outside owner");
        switch (byte_offset)
        {
        case 0:
            return render_[slot].primary_texture;
        case 4:
            return render_[slot].alternate_texture;
        case 8:
            return word08_[slot];
        case 12:
            return other_[slot][0];
        case 16:
            return other_[slot][1];
        case 20:
            return render_[slot].motion_index;
        case 24:
            return other_[slot][2];
        default:
            throw FormatError("physical material word offset outside source record");
        }
    }
    std::uint32_t &source_word(std::size_t, std::uint32_t) && = delete;
    std::uint16_t dimension(std::size_t slot) const
    {
        return static_cast<std::uint16_t>(source_word(slot, 8));
    }
    void set_dimension(std::size_t slot, std::uint16_t dimension)
    {
        auto &word = source_word(slot, 8);
        word = (word & 0xffff0000u) | dimension;
    }
    void prepare_next(std::uint32_t motion)
    {
        const auto slot = next_index();
        other_[slot][0] = 0;
        render_[slot].motion_index = motion; // 42BF60/42C0A0
    }
    void complete_next(std::uint32_t primary, std::uint32_t alternate, std::uint16_t dimension)
    {
        const auto slot = next_index();
        render_[slot].primary_texture = primary;
        render_[slot].alternate_texture = alternate;
        set_dimension(slot, dimension);
        ++cursor_; // 4303A0/430730 successful physical completion.
    }
    std::span<PolygonSubmissionMaterial> active_materials() &
    {
        return {render_.data(), size()};
    }
    std::span<const PolygonSubmissionMaterial> active_materials() const &
    {
        return {render_.data(), size()};
    }
    std::span<PolygonSubmissionMaterial> active_materials() && = delete;
    std::span<const PolygonSubmissionMaterial> active_materials() const && = delete;
    PolygonSubmissionMaterial &operator[](std::size_t slot) &
    {
        require(slot < size(), "physical material index outside active table");
        return render_[slot];
    }
    const PolygonSubmissionMaterial &operator[](std::size_t slot) const &
    {
        require(slot < size(), "physical material index outside active table");
        return render_[slot];
    }
    PolygonSubmissionMaterial &back() &
    {
        require(!empty(), "physical material table is empty");
        return render_[size() - 1];
    }
    const PolygonSubmissionMaterial &back() const &
    {
        require(!empty(), "physical material table is empty");
        return render_[size() - 1];
    }
    auto begin() &
    {
        return active_materials().begin();
    }
    auto end() &
    {
        return active_materials().end();
    }
    auto begin() const &
    {
        return active_materials().begin();
    }
    auto end() const &
    {
        return active_materials().end();
    }
};
static_assert(sizeof(PolygonSubmissionMaterial) == 12);
} // namespace sgn::retail
