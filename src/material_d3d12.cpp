#include "sgn/material_d3d12.hpp"
#include "sgn/format_require.hpp"
#include "sgn/host_texture_upscale.hpp"
#include "sgn/host_profiler.hpp"
#include <algorithm>
#include <limits>

namespace sgn
{
namespace
{
using sgn::format_check::require;
} // namespace
MaterialD3D12::MaterialD3D12(Renderer &renderer, MaterialTransparencyStorage &transparency, bool separate,
                             std::uint64_t epoch)
    : renderer_(&renderer), transparency_(transparency), separate_stage_surfaces_(separate),
      owner_(std::this_thread::get_id()), live_(std::make_shared<PolygonGpuTextures>())
{
    require(epoch != 0, "material resource epoch must be initialized explicitly");
    live_->epoch = epoch;
}
void MaterialD3D12::check_thread() const
{
    if (std::this_thread::get_id() != owner_)
        throw std::logic_error("material backend used outside its owner thread");
}
void MaterialD3D12::reset_transparency_430690()
{
    check_thread();
    transparency_.bytes.fill(0);
    transparency_.cursor = 0;
    // 430691..4306A2: only255 bytes and592028, not material/GPU-bank cursors.
}
std::uint8_t &MaterialD3D12::current_transparency()
{
    check_thread();
    require(transparency_.cursor && *transparency_.cursor < transparency_.bytes.size(),
            "material transparency pointer is uninitialized or outside its source bank");
    // Image-cache TGA conversion can call this without a pending upload.
    return transparency_.bytes[*transparency_.cursor];
}
void MaterialD3D12::release_all()
{
    check_thread();
    require(live_->epoch < std::numeric_limits<std::uint64_t>::max(), "material resource epoch exhausted");
    auto replacement = std::make_shared<PolygonGpuTextures>();
    replacement->epoch = live_->epoch + 1;
    live_ = std::move(replacement);
    recovery_.reset();
    resource_entries_ = 0;
    primary_tokens_.fill(0);
    pending_ = false;
    failed_ = false;
    resident_.clear(); // NEW HOST (R35): no replacement of the old bank is reused
    resident_bytes_ = 0;
    // 431790 resets the resource bank only. Snapshot/recording owners retain
    // in-flight COM objects independently; both source cursors remain untouched.
}
void MaterialD3D12::begin_pending_record(std::uint32_t motion_index)
{
    check_thread();
    require(renderer_ && !failed_, "failed or suspended material backend cannot upload");
    (void)motion_index;
    // The source+0C/+14 physical fields are not GPU-bank fields. The qualified
    // loader owns successful material publication and its motion index; this
    // adapter only observes the preparation boundary, without a second table.
    pending_ = true;
}
MaterialD3D12::PendingUpload MaterialD3D12::prepare_upload()
{
    check_thread();
    require(renderer_ && pending_ && !failed_, "material upload lacks a renderer or valid pending preparation");
    const auto transparency = current_transparency();
    require(resource_entries_ < 255, "material GPU bank exceeds source successful-upload domain");
    const unsigned count = separate_stage_surfaces_ ? 2 : 1;
    require(next_token_ <= std::numeric_limits<std::uint32_t>::max() - count + 1ull,
            "material GPU token space exhausted");
    live_->entries.reserve(live_->entries.size() + count);
    const auto first = static_cast<std::uint32_t>(next_token_);
    next_token_ += count;
    return {{first, separate_stage_surfaces_ ? first + 1 : first}, transparency};
}
retail::CompletedMaterialUpload MaterialD3D12::complete(std::vector<PolygonGpuTexture> textures, std::uint32_t width,
                                                        std::uint32_t height, PendingUpload pending)
{
    require(textures.size() == (separate_stage_surfaces_ ? 2u : 1u),
            "GPU material allocation returned wrong destination count");
    for (std::size_t index = 0; index < textures.size(); ++index)
    {
        auto &texture = textures[index];
        // NEW HOST (R28b): every destination of one upload carries the same
        // recorded profile, its format is that profile's, and the resource has
        // the profile's extent for the source extent.
        require(texture.resource != nullptr && texture.token == pending.tokens[index] &&
                    texture.format == polygon_profile_format(texture.profile) &&
                    texture.profile == textures.front().profile,
                "GPU material allocation returned wrong identity/format");
        require(polygon_texture_consistent(texture) && texture.recovery->width == width &&
                    texture.recovery->height == height,
                "GPU material allocation returned wrong dimensions");
        // 4304F4..430522 /43081B..430849: retain actual creation state. Do not
        // derive this byte from decoded alpha. DDSD_CKSRCBLT driver behavior is
        // a separate raster-translation boundary, not silently claimed here.
        texture.source_creation =
            PolygonTextureCreationEvidence{0x101007u | (pending.transparency ? 0x10000u : 0u),
                                           static_cast<std::uint32_t>(index), pending.transparency};
    }
    require(!separate_stage_surfaces_ || textures[0].resource.Get() != textures[1].resource.Get(),
            "separate material destinations alias one GPU resource");
    const auto first = pending.tokens[0], second = pending.tokens[1];
    for (auto &texture : textures)
        live_->entries.push_back(std::move(texture));
    primary_tokens_[resource_entries_] = first;
    ++resource_entries_;
    ++*transparency_.cursor;
    pending_ = false;
    // 43065D..430673 /430986..43099C advance the byte cursor once on success.
    // The physical-table transition stays in the qualified outer loader.
    return {first, second, static_cast<std::uint16_t>(width)};
}
retail::CompletedMaterialUpload MaterialD3D12::upload(retail::PreparedMaterialPixels pixels)
{
    const auto pending = prepare_upload();
    const auto &tokens = pending.tokens;
    try
    {
        std::vector<PolygonGpuTexture> textures;
        textures.reserve(separate_stage_surfaces_ ? 2 : 1);
        // NEW HOST (R28b): one settings snapshot for both stage destinations.
        const auto profile = renderer_->polygon_texture_profile();
        textures.push_back(
            renderer_->upload_polygon_texture(tokens[0], pixels.width, pixels.height, pixels.words, profile));
        if (separate_stage_surfaces_)
        {
            textures.push_back(
                renderer_->upload_polygon_texture(tokens[1], pixels.width, pixels.height, pixels.words, profile));
            textures.back().recovery = textures.front().recovery; // Same source words, still distinct GPU destinations.
        }
        return complete(std::move(textures), pixels.width, pixels.height, pending);
    }
    catch (...)
    {
        failed_ = true;
        throw;
    }
}
retail::CompletedMaterialUpload MaterialD3D12::generate(std::uint32_t dimension)
{
    const auto pending = prepare_upload();
    const auto &tokens = pending.tokens;
    try
    {
        return complete(renderer_->generate_polygon_textures(tokens[0], tokens[1], dimension), dimension, dimension,
                        pending);
    }
    catch (...)
    {
        failed_ = true;
        throw;
    }
}
void MaterialD3D12::require_font_upload_ready() const
{
    check_thread();
    require(renderer_ && !pending_ && !failed_, "font upload requires a bound completed material backend");
}
std::uint32_t MaterialD3D12::upload_font_primary(retail::PreparedMaterialPixels pixels)
{
    require_font_upload_ready();
    require(pixels.width == 256 && pixels.height == 256 && pixels.words.size() == 256 * 256,
            "font upload requires original256x256 packed atlas");
    require(resource_entries_ < 255, "font upload exceeds source GPU resource bank");
    require(next_token_ <= std::numeric_limits<std::uint32_t>::max(), "font GPU token space exhausted");
    live_->entries.reserve(live_->entries.size() + 1);
    const auto token = static_cast<std::uint32_t>(next_token_++);
    try
    {
        // NEW HOST (R28) sharp HUD: the atlas is a glyph texture (renderer may upload it upscaled).
        const host_texture_upscale::ScopedTextureSource glyphs(host_texture_upscale::TextureClass::glyph);
        const auto profile = renderer_->polygon_texture_profile(); // NEW HOST (R28b): a new identity
        auto texture = renderer_->upload_polygon_texture(token, pixels.width, pixels.height, pixels.words, profile);
        require(texture.token == token && texture.resource != nullptr && texture.profile == profile &&
                    texture.format == polygon_profile_format(profile),
                "font GPU completion identity/format differs");
        require(polygon_texture_consistent(texture) && texture.recovery->width == 256 &&
                    texture.recovery->height == 256,
                "font GPU completion dimensions differ");
        // 4325E0 never reads592028. The ordinary creation metadata type assumes
        // that byte was read, so leave it absent for fonts instead of inventing
        // a value. Actual source flags0x11007/caps0x4001000 are pinned separately.
        texture.source_creation.reset();
        live_->entries.push_back(std::move(texture));
        primary_tokens_[resource_entries_] = token;
        ++resource_entries_;
        return token;
    }
    catch (...)
    {
        failed_ = true;
        throw;
    }
}
std::shared_ptr<const PolygonGpuTextures> MaterialD3D12::bindings_snapshot() const
{
    check_thread();
    require(renderer_ && !failed_, "cannot publish bindings from a failed or suspended material backend");
    return std::make_shared<const PolygonGpuTextures>(*live_);
}
std::uint64_t MaterialD3D12::resource_epoch() const
{
    check_thread();
    return live_->epoch;
}
std::uint32_t MaterialD3D12::resource_entries() const
{
    check_thread();
    return resource_entries_;
}
std::uint32_t MaterialD3D12::resource_primary_token(std::uint32_t slot) const
{
    check_thread();
    require(!failed_ && !pending_, "material replacement requires a completed live resource bank");
    require(slot < resource_entries_, "material replacement resource slot is outside its bank");
    return primary_tokens_[slot];
}
// NEW HOST (R35) resident replacement versions (see material_d3d12.hpp).
const PolygonGpuTexture *MaterialD3D12::find_resident(const PolygonTextureProfile &profile,
                                                      const retail::PreparedMaterialPixels &pixels,
                                                      const ID3D12Resource *current) const
{
    for (const auto &texture : resident_)
        if (texture.profile == profile && texture.resource.Get() != current && texture.recovery &&
            texture.recovery->width == pixels.width && texture.recovery->height == pixels.height &&
            std::equal(texture.recovery->words.begin(), texture.recovery->words.end(), pixels.words.begin(),
                       pixels.words.end()))
            return &texture;
    return nullptr;
}
void MaterialD3D12::keep_resident(const PolygonGpuTexture &texture)
{
    constexpr std::size_t budget = 128u * 1024u * 1024u; // GPU memory of the kept versions (estimate)
    const auto bytes_of = [](const PolygonGpuTexture &t) -> std::size_t {
        if (!t.recovery)
            return 0;
        const std::size_t texels = std::size_t(t.recovery->width) * t.recovery->height * t.scale * t.scale;
        return texels * (t.profile.high_precision ? 4u : 2u) * 4u / 3u;
    };
    try
    {
        for (auto it = resident_.begin(); it != resident_.end(); ++it)
            if (it->resource.Get() == texture.resource.Get())
            {
                // Already kept: most recently used goes last.
                std::rotate(it, it + 1, resident_.end());
                return;
            }
        resident_.push_back(texture);
        resident_bytes_ += bytes_of(texture);
        while (resident_bytes_ > budget && resident_.size() > 1)
        {
            resident_bytes_ -= bytes_of(resident_.front());
            resident_.erase(resident_.begin());
        }
    }
    catch (...)
    {
        // Keeping a version is optional: the next swap uploads again, as before.
    }
}
void MaterialD3D12::replace_primary(std::uint32_t slot, retail::PreparedMaterialPixels pixels)
{
    check_thread();
    require(renderer_ != nullptr, "source replacement cannot run while renderer is suspended");
    const auto token = resource_primary_token(slot);
    const auto found = std::find_if(live_->entries.begin(), live_->entries.end(),
                                    [&](const auto &texture) { return texture.token == token; });
    require(found != live_->entries.end() && found->resource != nullptr,
            "material replacement primary resource is absent");
    const auto index = static_cast<std::size_t>(found - live_->entries.begin());
    // NEW HOST (R28b): the destination's SOURCE extent (its retained literal
    // extent), which equals the old resource extent unless its profile scales.
    require(polygon_texture_consistent(*found), "material replacement destination identity is inconsistent");
    // The recovered function guards width only. Unequal-height legacy Load
    // requires separate driver evidence and is outside this host adapter.
    require(pixels.width == found->recovery->width && pixels.height == found->recovery->height,
            "material replacement requires matching qualified dimensions");
    require(pixels.words.size() == std::uint64_t(pixels.width) * pixels.height,
            "material replacement pixel count differs from dimensions");
    require(live_->epoch < std::numeric_limits<std::uint64_t>::max(), "material resource epoch exhausted");
    (void)current_transparency(); // 430ED0..430ED8 reads it, but never creates a second destination.
    // Allocate/copy the new bank before touching the GPU. Publication below
    // cannot allocate; a failed upload leaves the old epoch and objects intact.
    auto replacement = std::make_shared<PolygonGpuTextures>(*live_);
    replacement->epoch = live_->epoch + 1;
    // NEW HOST (R32 perf-swap): this identity is a 430D40 destination; later race loads prewarm the
    // converted levels of its swaps. The superseded resource is handed to the renderer's deferred release
    // list below (kept until every submission made before the swap retired). Host bookkeeping only.
    renderer_->note_polygon_replacement_profile(found->profile);
    auto superseded = found->resource;
    try
    {
        // NEW HOST (R35): the same words were already uploaded as a replacement with the same recorded
        // profile (into this or another destination): that immutable texture (a different resource than the
        // current one) is this version. The conversion is a pure function of (words, profile), so its pixels are the
        // ones a new upload would produce; the bank changes exactly this one resource, as before.
        PolygonGpuTexture texture;
        if (const auto *resident = find_resident(found->profile, pixels, found->resource.Get()))
        {
            texture = *resident;
            texture.token = token; // the same immutable resource may serve several destinations
            sgn::host_profile::add("swap.resident_hit", 0); // counter
        }
        else
        {
            // NEW HOST (R28b): 430D40 keeps the destination identity, so the new
            // version is converted with the destination's RECORDED profile (never the
            // current settings): same format, same chain as its original.
            texture =
                renderer_->upload_polygon_texture(token, pixels.width, pixels.height, pixels.words, found->profile);
        }
        require(texture.token == token && texture.resource != nullptr && texture.format == found->format &&
                    texture.profile == found->profile && polygon_texture_consistent(texture),
                "replacement GPU identity/format differs");
        texture.source_creation = found->source_creation;
        keep_resident(texture); // NEW HOST (R35)
        replacement->entries[index] = std::move(texture);
        live_ = std::move(replacement);
        renderer_->retire_polygon_texture(std::move(superseded)); // noexcept
    }
    catch (...)
    {
        failed_ = true;
        throw;
    }
}
} // namespace sgn
