#pragma once
#include <cstdint>
#include <type_traits>

namespace sgn::retail
{
// Windows x64 listener source word. The actual pointer object is permanent:
// native void** stores and numeric source operations always use the same cell.
// Integer tokens are never dereferenced or treated as ownership evidence here.
// Integer/pointer representation is the qualified host contract, not portable
// C++ serialization. No reference counting or destructor publication occurs.
class SourceListenerValue
{
    void *cell_{};

  public:
    constexpr SourceListenerValue() noexcept = default;
    SourceListenerValue(std::uint64_t value) noexcept
    {
        *this = value;
    }
    SourceListenerValue &operator=(std::uint64_t value) noexcept
    {
        cell_ = value == 0 ? nullptr : reinterpret_cast<void *>(static_cast<std::uintptr_t>(value));
        return *this;
    }
    operator std::uint64_t() const noexcept
    {
        return cell_ == nullptr ? 0 : static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(cell_));
    }
    // Borrowed only during a synchronous call. The enclosing source owner must
    // stay alive and at the same address until that call returns or unwinds.
    void **native_address() & noexcept
    {
        return &cell_;
    }
};
static_assert(sizeof(void *) == sizeof(std::uint64_t), "Listener native storage requires the qualified 64-bit host");
static_assert(sizeof(std::uintptr_t) == sizeof(std::uint64_t));
static_assert(sizeof(SourceListenerValue) == sizeof(std::uint64_t));
static_assert(alignof(SourceListenerValue) == alignof(std::uint64_t));
static_assert(std::is_trivially_copyable_v<SourceListenerValue>);
static_assert(std::is_standard_layout_v<SourceListenerValue>);
} // namespace sgn::retail
