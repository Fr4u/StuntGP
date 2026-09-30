#pragma once
#include "sgn/assets.hpp"
#include <cstdint>
#include <cstring>
#include <memory>
#include <span>
#include <thread>
#include <type_traits>
namespace sgn::retail
{
// NEW checked transport for a source pointer into the same world's PE static
// objects. A missing represented byte range is NOT a null source pointer.
// No payload owner, bank allocation, source-address truncation or global mirror.
class SourceSceneStaticPointer
{
    std::weak_ptr<void> owner_;
    std::thread::id thread_;
    std::uint32_t address_{};
    std::span<const std::uint8_t> bytes_;

  public:
    static SourceSceneStaticPointer bind(const std::shared_ptr<void> &owner, std::uint32_t address,
                                         std::span<const std::uint8_t> bytes)
    {
        if (!owner || address == 0)
            throw FormatError("static source pointer requires live owner and nonnull address");
        SourceSceneStaticPointer result;
        result.owner_ = owner;
        result.thread_ = std::this_thread::get_id();
        result.address_ = address;
        result.bytes_ = bytes;
        return result;
    }
    std::uint32_t source_address() const
    {
        check();
        return address_;
    }
    void check() const
    {
        if (owner_.expired() || thread_ != std::this_thread::get_id())
            throw FormatError("static scene pointer expired or used from another thread");
    }
    template <class T> T read(std::size_t offset = 0) const
    {
        static_assert(std::is_trivially_copyable_v<T>);
        const auto owner = owner_.lock();
        if (!owner || thread_ != std::this_thread::get_id())
            throw FormatError("static scene pointer read lacks its live owner/thread");
        if (offset > bytes_.size() || sizeof(T) > bytes_.size() - offset)
            throw FormatError("source static pointer reached unrepresented bytes");
        T result;
        std::memcpy(&result, bytes_.data() + offset, sizeof(T));
        return result;
    }
    bool operator==(const SourceSceneStaticPointer &other) const noexcept
    {
        return address_ == other.address_ && bytes_.data() == other.bytes_.data() &&
               bytes_.size() == other.bytes_.size() && !owner_.owner_before(other.owner_) &&
               !other.owner_.owner_before(owner_);
    }
};
} // namespace sgn::retail
