#include "sgn/race_overlay_textures.hpp"
#include "sgn/host_texture_upscale.hpp"
#include "sgn/host_profiler.hpp"
#include <limits>

namespace sgn::retail
{
RaceOverlayTextures::RaceOverlayTextures(const AssetStore &assets, TextureReplacementDevice &device,
                                         const std::shared_ptr<RaceSessionPlatform> &platform)
    : device_(device), platform_(platform)
{
    const auto owner = require_live(); // host lifetime admission before asset I/O
    (void)owner;
    // Source42C340/430CA0 IDs are not native descriptor offsets. The full
    // front-end/gameplay registry and legacy COM cache are separate from this
    // bounded race-start/notice/finish/pause resource implementation.
    constexpr std::array names{"Overlays\\Start0",    "Overlays\\Start1",   "Overlays\\Start2",  "Overlays\\Start3",
                               "Overlays\\LapRecord", "Overlays\\Result1",  "Overlays\\Result2", "Overlays\\Result3",
                               "Overlays\\Finish",    "Overlays\\GameOver", "Overlays\\BoxBack", "Overlays\\BoxBack2",
                               "Overlays\\logo-add",  "Overlays\\logo-sub"};
    for (unsigned i = 0; i < names.size(); ++i)
    {
        auto image = assets.texture(names[i]);
        if (image.width != 256 || image.height != 256 || image.rgba.size() != 256 * 256 * 4)
            throw FormatError("race overlay differs from pinned256x256asset domain");
        source_to_image_.emplace(16 + i, images_.size());
        images_.push_back(std::move(image));
    }
    // NEW HOST (R28) sharp HUD: upscale the HUD overlays now (race load), not at their first mid-race swap.
    for (const auto &image : images_)
        sgn::host_texture_upscale::prewarm_rgba8(image.rgba, image.width, image.height,
                                                 sgn::host_texture_upscale::TextureClass::hud);
}
std::shared_ptr<RaceSessionPlatform> RaceOverlayTextures::require_live() const
{
    auto owner = platform_.lock();
    if (!owner)
        throw FormatError("overlay platform lifetime has expired");
    owner->require_owner_thread();
    return owner;
}
void RaceOverlayTextures::require_for(const std::shared_ptr<RaceSessionPlatform> &expected) const
{
    const auto owner = require_live();
    if (!expected || owner.get() != expected.get() || owner.owner_before(expected) || expected.owner_before(owner))
        throw FormatError("overlay belongs to a different platform or control block");
}
std::uint32_t RaceOverlayTextures::selected_overlay_texture() const
{
    const auto owner = require_live();
    return owner->state().source_overlay_selection_61b634;
}
void RaceOverlayTextures::reset_source_selection_44600f() &
{
    const auto owner = require_live();
    owner->state().source_overlay_selection_61b634 = 0;
}
const Image &RaceOverlayTextures::source_image(std::uint32_t id) const
{
    const auto found = source_to_image_.find(id);
    if (found == source_to_image_.end())
        throw FormatError("race overlay source ID outside recovered16..29domain");
    return images_.at(found->second);
}
void RaceOverlayTextures::replace_source_texture(std::uint32_t destination, std::uint32_t source)
{
    SGN_PROFILE_SCOPE("swap.replace_430d40"); // NEW HOST (R30): whole 430D40 (check, upload)
    const auto owner = require_live();        // retain through synchronous device callbacks
    const auto &image = source_image(source); // 430D4F source ID -> descriptor
    if (device_.destination_width(destination) != image.width)
        return; // 430D86: width only
    if (destination >= completed_sources_.size())
        throw FormatError("completed source destination outside physical capacity");
    // NEW HOST (R28) sharp HUD: every source image here is an Overlays\ HUD image.
    const sgn::host_texture_upscale::ScopedTextureSource texture_source(sgn::host_texture_upscale::TextureClass::hud);
    if (destination == 15)
        owner->state().source_overlay_selection_61b634 = source; // 430D93: published before decode/device failure
    device_.upload_to_destination(destination, image);           // replace the actual consumed image
    completed_sources_[destination] = source; // No allocation; rejected/failed calls retain the prior completed source.
}
std::optional<std::uint32_t> RaceOverlayTextures::completed_source(std::uint32_t destination) const
{
    if (destination >= completed_sources_.size())
        throw FormatError("completed source destination outside physical capacity");
    return completed_sources_[destination];
}
void RendererTextureBindings::bind(std::uint32_t source_slot, unsigned native_texture)
{
    const auto dimensions = renderer_.texture_dimensions(native_texture);
    if (dimensions[0] > std::numeric_limits<std::uint16_t>::max())
        throw FormatError("destination width exceeds original16-bit field");
    slots_.insert_or_assign(source_slot, native_texture);
}
unsigned RendererTextureBindings::native_texture(std::uint32_t source_slot) const
{
    const auto found = slots_.find(source_slot);
    if (found == slots_.end())
        throw FormatError("unbound source texture destination");
    return found->second;
}
std::uint16_t RendererTextureBindings::destination_width(std::uint32_t slot) const
{
    const auto dimensions = renderer_.texture_dimensions(native_texture(slot));
    if (dimensions[0] > std::numeric_limits<std::uint16_t>::max())
        throw FormatError("destination width exceeds original16-bit field");
    return static_cast<std::uint16_t>(dimensions[0]);
}
void RendererTextureBindings::upload_to_destination(std::uint32_t slot, const Image &image)
{
    renderer_.replace_texture(native_texture(slot), image);
}
} // namespace sgn::retail
