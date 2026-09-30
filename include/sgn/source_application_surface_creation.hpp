#pragma once
#include "sgn/source_application_display_mode.hpp"
#include "sgn/source_application_surface_registry.hpp"

namespace sgn::retail
{
// The original passes addresses of destination words, and publishes that
// ADDRESS in61C358. It does not publish a copied surface pointer there.
struct SourceApplicationSurfaceSlot
{
    SourceApplicationSurface *receiver{};
};
struct SourceColorMaskWords
{
    std::uint32_t &first_bit;
    std::uint32_t &bit_count;
};
void source_decode_color_mask_422bb0(std::uint32_t mask, SourceColorMaskWords);

class SourceApplicationSurfaceCreation : public SourceDisplayModeInvocation
{
    const std::uint32_t &flags_5728dc_;
    std::uint32_t &created_5728d8_;
    SourceApplicationSurfaceSlot &primary_;
    SourceApplicationSurfaceSlot &destination_;
    SourceApplicationSurfaceSlot *&selected_slot_61c358_;

  public:
    SourceApplicationSurfaceCreation(const std::uint32_t &flags, std::uint32_t &created,
                                     SourceApplicationSurfaceSlot &primary, SourceApplicationSurfaceSlot &destination,
                                     SourceApplicationSurfaceSlot *&selected)
        : flags_5728dc_(flags), created_5728d8_(created), primary_(primary), destination_(destination),
          selected_slot_61c358_(selected)
    {
    }
    std::uint32_t create_422df0(std::uint32_t, std::uint32_t) final;
    std::uint32_t create_422d50(std::uint32_t width, std::uint32_t height) final;
    std::uint32_t create_422ea0(std::uint32_t, std::uint32_t) final;
    std::uint32_t fallback_422c60(std::uint32_t, std::uint32_t, std::uint32_t depth) final;
    void configure_16bit_422bf0() final;

  protected:
    virtual SourceApplicationSurface *global_back_surface_61c3a0() = 0;
    virtual std::uint32_t pixel_format_slot54(SourceApplicationSurface &,
                                              std::array<std::optional<std::uint8_t>, 32> &) = 0;
    // red: first576BEC/count576BF8; green:576BF0/576BFC; blue:576BF4/576C00.
    virtual SourceColorMaskWords color_mask_words(unsigned channel) = 0;
    std::uint32_t primary_422a80(SourceSurfaceDescriptor &, SourceApplicationSurfaceSlot &,
                                 std::uint32_t failure_policy);
    std::uint32_t secondary_422ac0(SourceApplicationSurfaceSlot &, std::uint32_t width, std::uint32_t height,
                                   std::uint32_t caps);
    // Actual CreateSurface on the captured device, outer aggregation=null.
    virtual std::uint32_t create_surface_slot18(SourceSurfaceDescriptor &, SourceApplicationSurfaceSlot &) = 0;
    // Read destination.receiver only after successful CreateSurface. Must write
    // the existing5727D4 registry and increment its actual5728D4 count.
    void register_surface_422a00(SourceApplicationSurfaceSlot &);
    virtual SourceApplicationSurfaceRegistry surface_registry() = 0;
    virtual std::uint32_t attached_slot30(SourceApplicationSurface &, std::array<std::optional<std::uint32_t>, 4> &caps,
                                          SourceApplicationSurfaceSlot &destination) = 0;
    virtual void describe_error_422690(std::uint32_t result) = 0;
    // 44E4A0 calls CRT exit(1); implementations must not return success.
    [[noreturn]] virtual void exit_44e4a0() = 0;
};
} // namespace sgn::retail
