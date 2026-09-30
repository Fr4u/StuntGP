#pragma once
#include "sgn/race_session_lifecycle.hpp"
#include "sgn/renderer.hpp"
#include <map>

namespace sgn::retail
{
// Mandatory actual destination image storage, not an event sink. Device
// bindings own their widths;430D40 borrows the width at the time of the call.
struct TextureReplacementDevice
{
    virtual ~TextureReplacementDevice() = default;
    virtual std::uint16_t destination_width(std::uint32_t slot) const = 0;
    virtual void upload_to_destination(std::uint32_t slot, const Image &) = 0;
};
// Bounded gameplay table entries16..29 (row addresses4956A8..495744).
// Row26 is Overlays\BoxBack, requested by447441 before pause input dispatch.
// Row27 is BoxBack2 (449FB0); rows28/29 are logo-add/logo-sub (40DE80).
// Image descriptor
// indices are native tokens, distinct from source IDs. Selected61B634 belongs
// to RaceSessionState. This resource owner only borrows its platform lifetime.
// Construction/destruction never initializes or resets that source word.
class RaceOverlayTextures final : public RaceTextureReplacement
{
    TextureReplacementDevice &device_;
    std::vector<Image> images_;
    std::map<std::uint32_t, std::size_t> source_to_image_;
    std::weak_ptr<RaceSessionPlatform> platform_;
    std::shared_ptr<RaceSessionPlatform> require_live() const;
    // New host diagnostic state: last fully completed source replacement per
    // destination. Empty means original cold-world contents, never transparent.
    std::array<std::optional<std::uint32_t>, 255> completed_sources_{};

  public:
    RaceOverlayTextures(const AssetStore &, TextureReplacementDevice &, const std::shared_ptr<RaceSessionPlatform> &);
    // New host admission: identical object AND control block, on owner thread.
    void require_for(const std::shared_ptr<RaceSessionPlatform> &) const;
    std::uint32_t selected_overlay_texture() const override;
    // 44600F: only the source61B634 store; keep images and completed uploads.
    //  Deliberately not noexcept: an expired/wrong-thread host borrow rejects.
    void reset_source_selection_44600f() &;
    void replace_source_texture(std::uint32_t destination_slot, std::uint32_t source_texture) override;
    const Image &source_image(std::uint32_t id) const;
    std::optional<std::uint32_t> completed_source(std::uint32_t destination) const;
};
// Modern D3D12 adapter: source slots map to actual shader descriptor indices.
// Construct/rebind after load_scene or device recreation; no cached dimensions.
class RendererTextureBindings final : public TextureReplacementDevice
{
    Renderer &renderer_;
    std::map<std::uint32_t, unsigned> slots_;

  public:
    explicit RendererTextureBindings(Renderer &renderer) : renderer_(renderer) {}
    void bind(std::uint32_t source_slot, unsigned native_texture);
    unsigned native_texture(std::uint32_t source_slot) const;
    std::uint16_t destination_width(std::uint32_t slot) const override;
    void upload_to_destination(std::uint32_t slot, const Image &) override;
};
} // namespace sgn::retail
