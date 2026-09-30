#pragma once
#include "sgn/source_allocation_bank.hpp"
#include <cstring>
#include <limits>
#include <type_traits>

namespace sgn::retail
{
// NEW HOST pointer into the existing bank. No payload/platform ownership.
// Binding does not read a pointee: source may publish a null/expired pointer.
// A reached read locks its same platform and checks the allocation generation.
class SourceSceneAllocationPointer
{
    std::weak_ptr<void> owner_;
    SourceAllocationBank *bank_{};
    std::thread::id thread_;
    SourceAddressLocation location_;

  public:
    static SourceSceneAllocationPointer bind(const std::shared_ptr<void> &owner, SourceAllocationBank &bank,
                                             SourceAddressLocation location)
    {
        if (!owner)
            throw SourceAllocationError(0x42d970, "source light pointer has no platform owner");
        SourceSceneAllocationPointer pointer;
        pointer.owner_ = owner;
        pointer.bank_ = &bank;
        pointer.thread_ = std::this_thread::get_id();
        pointer.location_ = location;
        return pointer;
    }
    SourceAddressLocation require_location(const std::shared_ptr<void> &owner, SourceAllocationBank &bank) const
    {
        const auto actual = owner_.lock();
        if (!actual || thread_ != std::this_thread::get_id() || bank_ != &bank || owner_.owner_before(owner) ||
            owner.owner_before(owner_))
            throw SourceAllocationError(0x42b490, "light pointer belongs to another/expired platform or bank");
        return location_; // metadata only, preserve pointer without dereferencing
    }
    template <class T> T read(std::uint32_t offset, std::uint32_t site) const
    {
        static_assert(std::is_trivially_copyable_v<T>);
        const auto owner = owner_.lock();
        if (!owner || thread_ != std::this_thread::get_id())
            throw SourceAllocationError(site, "source allocation pointer owner expired or thread differs");
        if (offset > std::numeric_limits<std::uint32_t>::max() - location_.offset)
            throw SourceAllocationError(site, "source allocation pointer offset overflow");
        const auto position = location_.offset + offset;
        const auto bytes = bank_->bytes(location_.allocation);
        if (position > bytes.size() || sizeof(T) > bytes.size() - position)
            throw SourceAllocationError(site, "source allocation pointer leaves its actual payload");
        T value;
        std::memcpy(&value, bytes.data() + position, sizeof(T));
        return value;
    }
    bool operator==(const SourceSceneAllocationPointer &other) const noexcept
    {
        return bank_ == other.bank_ && location_.allocation == other.location_.allocation &&
               location_.offset == other.location_.offset && !owner_.owner_before(other.owner_) &&
               !other.owner_.owner_before(owner_);
    }
};
} // namespace sgn::retail
