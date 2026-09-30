#include "sgn/material_d3d12.hpp"
#include "sgn/polygon_submission.hpp"
#include "sgn/format_require.hpp"
#include <unordered_map>

namespace sgn
{
// One CPU-only handoff over live bank and one/two retained command recipes.
// No old device/resource is retained across actual Renderer replacement.
struct MaterialDeviceRecovery
{
    struct Texture
    {
        std::uint32_t token;
        DXGI_FORMAT format;
        std::optional<PolygonTextureCreationEvidence> creation;
        std::shared_ptr<const PolygonTexturePixels> pixels;
        PolygonTextureProfile profile; // NEW HOST (R28b): the recorded conversion chain, reused on rebind
    };
    struct Bank
    {
        std::uint64_t epoch;
        std::vector<std::size_t> resources;
    };
    struct Frame
    {
        std::shared_ptr<const CapturedPolygonSubmission> commands;
        std::vector<std::size_t> segment_banks;
    };
    std::vector<Texture> textures;
    std::vector<Bank> banks;
    std::size_t live_bank{};
    std::vector<Frame> frames;
};
namespace
{
using sgn::format_check::require;
bool same_creation_for_rebind(const std::optional<PolygonTextureCreationEvidence> &a,
                              const std::optional<PolygonTextureCreationEvidence> &b)
{
    return a.has_value() == b.has_value() &&
           (!a || (a->descriptor_flags == b->descriptor_flags && a->texture_stage == b->texture_stage &&
                   a->transparency_byte == b->transparency_byte));
}
void require_recovery_pixels(const PolygonGpuTexture &texture)
{
    // NEW HOST (R28b): format = the recorded profile's format.
    require(texture.resource && texture.recovery && texture.format == polygon_profile_format(texture.profile),
            "material recovery requires retained actual packed pixels");
    const auto &pixels = *texture.recovery;
    // NEW HOST (R28b): the resource is exactly `scale` (1, or the sharp-HUD 2/4)
    // times the retained source extent, in the recorded format.
    require(pixels.width > 0 && pixels.height > 0 && pixels.width <= 16384 && pixels.height <= 16384 &&
                pixels.words.size() == std::uint64_t(pixels.width) * pixels.height &&
                polygon_texture_consistent(texture),
            "material recovery image identity/extent differs");
}
} // namespace
bool MaterialD3D12::bound_to_renderer(const Renderer &renderer) const
{
    check_thread();
    return renderer_ == &renderer && !failed_ && !recovery_;
}
void MaterialD3D12::suspend_renderer_for_recreation(ImmutablePolygonSubmission &displayed)
{
    suspend_renderer_for_recreation(std::span{&displayed, std::size_t(1)});
}
void MaterialD3D12::suspend_renderer_for_recreation(std::span<ImmutablePolygonSubmission> retained)
{
    suspend_renderer_impl(retained, false);
}
void MaterialD3D12::suspend_renderer_for_recreation(SourceMenuMaterialRecovery,
                                                    std::span<ImmutablePolygonSubmission> retained)
{
    suspend_renderer_impl(retained, true);
}
void MaterialD3D12::suspend_renderer_impl(std::span<ImmutablePolygonSubmission> retained, bool source_menu_history)
{
    check_thread();
    require(renderer_ && !recovery_ && !pending_ && !failed_,
            "unfinished source upload cannot be recovered as a completed frame");
    require(retained.size() == 1 || retained.size() == 2, "material recovery requires one recipe or two surface roles");
    if (!source_menu_history)
        require(resource_entries_ > 0 && !live_->entries.empty(), "material recovery requires a completed bank");
    else
        require((resource_entries_ == 0) == live_->entries.empty(),
                "source menu recovery has an incomplete empty bank");
    for (const auto &frame : retained)
        if (frame)
        {
            long internal = 0;
            for (const auto &other : retained)
                if (other == frame)
                    ++internal;
            require(frame.use_count() == internal, "release external retained-frame copies before device recreation");
            (void)validate_polygon_submission(*frame);
        }
    validate_polygon_bank(*live_);
    auto next = std::make_shared<MaterialDeviceRecovery>();
    next->frames.resize(retained.size());
    std::unordered_map<ID3D12Resource *, std::size_t> resources; // Temporary nonowning keys.
    std::unordered_map<const PolygonGpuTextures *, std::size_t> banks;
    const auto stage = [&](const std::shared_ptr<const PolygonGpuTextures> &old) {
        require(bool(old), "missing retained material bank");
        if (const auto found = banks.find(old.get()); found != banks.end())
            return found->second;
        require(old->epoch <= live_->epoch, "retained frame has a future material epoch");
        if (!source_menu_history)
            require(old->entries.size() == live_->entries.size(),
                    "retained frame has another material extent/lifetime");
        validate_polygon_bank(*old);
        MaterialDeviceRecovery::Bank bank{old->epoch, {}};
        bank.resources.reserve(old->entries.size());
        for (std::size_t i = 0; i < old->entries.size(); ++i)
        {
            const auto &entry = old->entries[i];
            if (source_menu_history)
            {
                // SourceMenu's surface adapter owns both recipe cells. Real
                // boot/loading resets change count/tokens without clearing the
                // already displayed image. Validate native device provenance
                // and this backend's issued-token domain, then stage each bank
                // independently in the EXISTING recovery graph.
                require(entry.token != 0 && entry.token < next_token_,
                        "menu recovery token was not issued in this backend lifetime");
                renderer_->require_polygon_texture_owner(entry);
            }
            else
            {
                const auto &current = live_->entries[i];
                // NEW HOST (R28b): versions of one identity share its recorded
                // profile, so the format identity is strict again.
                require(entry.token == current.token && entry.format == current.format &&
                            entry.profile == current.profile &&
                            same_creation_for_rebind(entry.source_creation, current.source_creation),
                        "retained frame belongs to another token/creation layout");
            }
            require_recovery_pixels(entry);
            auto found = resources.find(entry.resource.Get());
            if (found == resources.end())
            {
                const auto index = next->textures.size();
                next->textures.push_back(
                    {entry.token, entry.format, entry.source_creation, entry.recovery, entry.profile});
                found = resources.emplace(entry.resource.Get(), index).first;
            }
            else
            {
                const auto &prior = next->textures[found->second];
                require(prior.token == entry.token && prior.format == entry.format && prior.profile == entry.profile &&
                            prior.pixels == entry.recovery &&
                            same_creation_for_rebind(prior.creation, entry.source_creation),
                        "one GPU resource has conflicting recovery identity");
            }
            bank.resources.push_back(found->second);
        }
        const auto index = next->banks.size();
        next->banks.push_back(std::move(bank));
        banks.emplace(old.get(), index);
        return index;
    };
    next->live_bank = stage(live_);
    for (std::size_t i = 0; i < retained.size(); ++i)
        if (retained[i])
        {
            auto commands = std::make_shared<CapturedPolygonSubmission>(*retained[i]);
            auto &saved = next->frames[i];
            saved.segment_banks.reserve(commands->segments.size());
            for (auto &segment : commands->segments)
            {
                saved.segment_banks.push_back(stage(segment.textures));
                segment.textures.reset();
            }
            saved.commands = std::move(commands);
        }
    auto unbound = std::make_shared<PolygonGpuTextures>();
    unbound->epoch = live_->epoch;
    // Nonthrowing commit only after all recipes/current resource versions are
    // CPU-owned. Null output cells and live_ release old GPU owners together.
    recovery_ = std::move(next);
    for (auto &frame : retained)
        frame.reset();
    live_ = std::move(unbound);
    renderer_ = nullptr;
    resident_.clear(); // NEW HOST (R35): versions of the suspended device are never reused
    resident_bytes_ = 0;
}
void MaterialD3D12::rebind_renderer(Renderer &next, ImmutablePolygonSubmission &displayed)
{
    (void)rebind_renderer_impl(next, std::span{&displayed, std::size_t(1)}, {});
}
PolygonSurfaceRoles MaterialD3D12::rebind_renderer(Renderer &next, std::array<ImmutablePolygonSubmission, 2> &retained,
                                                   const std::array<SdrGammaRamp, 2> &ramps)
{
    return rebind_renderer_impl(next, retained, ramps);
}
PolygonSurfaceRoles MaterialD3D12::rebind_renderer_impl(Renderer &next, std::span<ImmutablePolygonSubmission> retained,
                                                        std::span<const SdrGammaRamp> ramps)
{
    check_thread();
    require(!renderer_ && recovery_ && !pending_ && !failed_, "material rebind requires CPU-only suspension");
    const auto &saved = *recovery_;
    require(retained.size() == saved.frames.size() && (retained.size() == 1 || retained.size() == 2),
            "recovery recipe extent differs");
    require((retained.size() == 1 && ramps.empty()) || (retained.size() == 2 && ramps.size() == 2),
            "recovery surface ramp extent differs");
    for (const auto &frame : retained)
        require(!frame, "recovery destination must not retain old GPU recipes");
    if (!ramps.empty())
    {
        const auto images = next.polygon_surface_roles();
        require(!images.front_image && !images.back_image, "candidate renderer must have empty surface image roles");
    }
    std::vector<PolygonGpuTexture> resources(saved.textures.size());
    std::vector<std::shared_ptr<PolygonGpuTextures>> banks;
    banks.reserve(saved.banks.size());
    for (const auto &bank : saved.banks)
    {
        auto staged = std::make_shared<PolygonGpuTextures>();
        staged->epoch = bank.epoch;
        staged->entries.resize(bank.resources.size());
        banks.push_back(std::move(staged));
    }
    std::vector<std::shared_ptr<CapturedPolygonSubmission>> frames(saved.frames.size());
    for (std::size_t i = 0; i < frames.size(); ++i)
        if (saved.frames[i].commands)
        {
            frames[i] = std::make_shared<CapturedPolygonSubmission>(*saved.frames[i].commands);
            require(frames[i]->segments.size() == saved.frames[i].segment_banks.size(),
                    "recovery segment extent differs");
        }
    // One existing resource-version graph across CURRENT bank and BOTH roles.
    for (std::size_t i = 0; i < saved.textures.size(); ++i)
    {
        const auto &old = saved.textures[i];
        const auto &pixels = *old.pixels;
        // NEW HOST (R28b): rebuilt with the RECORDED profile, not the current
        // settings: same format, same conversion chain, same pixels.
        auto texture = next.upload_polygon_texture(old.token, pixels.width, pixels.height, pixels.words, old.profile);
        require(texture.token == old.token && texture.resource && texture.format == old.format &&
                    texture.profile == old.profile,
                "recreated material identity differs");
        texture.source_creation = old.creation;
        texture.recovery = old.pixels;
        resources[i] = std::move(texture);
    }
    for (std::size_t i = 0; i < banks.size(); ++i)
        for (std::size_t j = 0; j < saved.banks[i].resources.size(); ++j)
            banks[i]->entries[j] = resources.at(saved.banks[i].resources[j]);
    auto new_live = banks.at(saved.live_bank);
    validate_polygon_bank(*new_live);
    for (std::size_t i = 0; i < frames.size(); ++i)
        if (frames[i])
        {
            for (std::size_t j = 0; j < frames[i]->segments.size(); ++j)
                frames[i]->segments[j].textures = banks.at(saved.frames[i].segment_banks[j]);
            (void)validate_polygon_submission(*frames[i]);
        }
    PolygonSurfaceRoles images{};
    if (!ramps.empty())
        images = next.restore_polygon_surfaces(frames[0].get(), ramps[0], frames[1].get(), ramps[1]);
    // GPU images have BOTH published or restore threw. Nothing can throw from
    // here: a partial Execute/Signal failure above leaves this CPU handoff and
    // every caller recipe cell unchanged, while candidate Renderer retains work.
    for (std::size_t i = 0; i < frames.size(); ++i)
        retained[i] = std::move(frames[i]);
    live_ = std::move(new_live);
    renderer_ = &next;
    recovery_.reset();
    return images;
}
} // namespace sgn
