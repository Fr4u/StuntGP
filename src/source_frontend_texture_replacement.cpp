#include "sgn/source_frontend_texture_replacement.hpp"
#include "sgn/host_texture_upscale.hpp"
#include "sgn/host_profiler.hpp"
#include "sgn/race_session.hpp"
namespace sgn::retail
{
SourceFrontendTextureReplacement::SourceFrontendTextureReplacement(FrontendState &frontend,
                                                                   const WorldMaterialLoadingState &images,
                                                                   TextureReplacementDevice &device) noexcept
    : frontend_(frontend), images_(images), device_(device)
{
}
std::uint32_t SourceFrontendTextureReplacement::selected_overlay_texture() const
{
    frontend_.shared_platform()->require_owner_thread();
    return frontend_.world().source_overlay_selection_61b634;
}
void SourceFrontendTextureReplacement::replace_source_texture(std::uint32_t destination, std::uint32_t source)
{
    SGN_PROFILE_SCOPE("swap.replace_430d40"); // NEW HOST (R30): whole 430D40 (check, decode, upload)
    frontend_.shared_platform()->require_owner_thread();
    const auto ordinal = images_.image_ordinal(source); // 430D4F; includes existing ordinal255+ arena aliases
    if (ordinal >= images_.images.size())
        throw FormatError("frontend image ordinal outside loaded record owner");
    const auto &record = images_.images[ordinal];
    const auto destination_width = device_.destination_width(destination); // source reads WORD before record width
    if (destination_width != record.width)
        return; // 430D86: no selected store and no decode/upload
    if (destination == 15)
        frontend_.world().source_overlay_selection_61b634 = source; // 430D93 before failures
    if (!record.payload)
        throw FormatError("frontend texture points to an absent image payload");
    // Modern transient conversion only. Native42E8D0 scratch and COM staging
    // lifetime are not claimed by this bridge. The existing device performs
    // real replacement and430F24's next-free material writes.
    // NEW HOST (R30 perf-tick): the same decode (decode_tm / 5-bit expansion), done once per immutable
    // payload and shared; race swap sources were already decoded at race load (retail_world_material_loading).
    const auto decoded = source_replacement_image(record);
    const auto &image = *decoded;
    if (image.width != record.width || image.height != record.height)
        throw FormatError("frontend decoded image disagrees with source record dimensions");
    // NEW HOST (R28) sharp HUD: the texture class follows the source image name (FrontEnd\ is menu art).
    const sgn::host_texture_upscale::ScopedTextureSource texture_source(images_.image_name(record.name));
    SGN_PROFILE_SCOPE("swap.upload_430d40"); // NEW HOST (R30): the device replacement (upscale/edge/mips/GPU)
    device_.upload_to_destination(destination, image);
}
} // namespace sgn::retail
