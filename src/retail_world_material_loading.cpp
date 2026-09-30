#include "sgn/retail_world_material_loading.hpp"
#include "sgn/session_effect_resources.hpp"
#include "sgn/format_require.hpp"
#include "sgn/host_texture_upscale.hpp"
#include "sgn/host_profiler.hpp"
#include "sgn/host_load_timeline.hpp"
#include "sgn/host_parallel.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>

namespace sgn::retail
{
namespace
{
using sgn::format_check::require;
std::string nul_text(const std::string &value)
{
    return value.substr(0, value.find('\0'));
}
std::uint16_t half(std::span<const std::uint8_t> b, std::size_t p)
{
    require(p + 2 <= b.size(), "short source image header");
    return static_cast<std::uint16_t>(b[p] | (std::uint16_t(b[p + 1]) << 8));
}
std::string arena_text(std::span<const std::uint8_t> bytes, std::size_t offset)
{
    require(offset < bytes.size(), "source image name offset outside arena");
    const auto tail = bytes.subspan(offset);
    const auto end = std::find(tail.begin(), tail.end(), 0);
    require(end != tail.end(), "source image name missing terminator");
    return {tail.begin(), end};
}
void store_text(std::span<std::uint8_t> bytes, std::uint32_t offset, const std::string &text)
{
    require(offset < bytes.size() && text.size() < bytes.size() - offset, "source string arena capacity");
    std::copy(text.begin(), text.end(), bytes.begin() + offset);
    bytes[offset + text.size()] = 0;
}
bool same_insensitive(const std::string &a, const std::string &b)
{
    const auto x = nul_text(a), y = nul_text(b);
    if (x.size() != y.size())
        return false;
    auto lower = [](unsigned char c) { return c >= 'A' && c <= 'Z' ? static_cast<unsigned char>(c + 32) : c; };
    for (std::size_t i = 0; i < x.size(); ++i)
        if (lower(static_cast<unsigned char>(x[i])) != lower(static_cast<unsigned char>(y[i])))
            return false;
    return true;
}
void set_ordinal(WorldMaterialLoadingState &s, std::uint32_t index, std::uint32_t value)
{
    if (index < 255)
    {
        s.image_ordinals[index] = value;
        return;
    }
    const auto at = std::uint64_t(index - 255) * 4;
    require(at + 4 <= s.image_name_arena.size(), "source image ordinal outside shared arena");
    for (unsigned i = 0; i < 4; ++i)
        s.image_name_arena[static_cast<std::size_t>(at) + i] = static_cast<std::uint8_t>(value >> (i * 8));
}
Bytes read_image(MaterialFileSource &files, const MaterialResourcePath &path)
{
    const auto size = files.source_file_size(path);
    if (size == 0 || size > 16 * 1024 * 1024)
        throw FormatError("missing or unsupported source image size: " + path.relative_name);
    Bytes bytes(size);
    const auto read = files.read_source_file(path, bytes);
    require(read.source_size != 0 && read.copied_bytes == size, "incomplete source image payload");
    return bytes;
}
std::vector<std::uint16_t> pc_words(std::span<const std::uint8_t> bytes)
{
    // Reuse current qualified4234E0 decoder; its hostUNORM8 conversion is exactly
    // invertible on the original5-bit domain. The new oracle checks every word.
    // NEW HOST (R30): decode_tm_words returns the words that expansion started from, which are the
    // words the former expand-then-invert loop reproduced (for every 5-bit code c,
    // ((c * 255 + 15) / 31 * 31 + 127) / 255 == c, and bit 15 maps to 255 and back to 0x8000), without
    // the UNORM8 image in between.
    return decode_tm_words(bytes).words;
}
std::vector<std::uint16_t> tga_words(std::span<const std::uint8_t> bytes, WorldMaterialLoadingState &s,
                                     std::uint8_t &transparency)
{
    require(bytes.size() >= 18 && bytes[2] == 2 && bytes[16] == 24, "unsupported source TGA type");
    s.width = s.tga_width = half(bytes, 12);
    s.height = s.tga_height = half(bytes, 14);
    const auto count = std::uint64_t(s.width) * s.height, start = std::size_t(18) + bytes[0];
    require(count > 0 && count <= 4 * 1024 * 1024 && start <= bytes.size() && count * 3 <= bytes.size() - start,
            "source TGA payload bounds");
    std::vector<std::uint16_t> out(static_cast<std::size_t>(count));
    // 462310 tests the IMAGE TYPE byte in BL, not descriptor orientation bit.
    // For its required type2 domain the original always reverses the rows.
    for (std::uint32_t y = 0; y < s.height; ++y)
        for (std::uint32_t x = 0; x < s.width; ++x)
        {
            const auto at = start + (std::size_t(s.height - 1 - y) * s.width + x) * 3;
            auto r = bytes[at + 2], g = bytes[at + 1], b = bytes[at];
            std::uint16_t alpha = 0x8000;
            if (r < 8 && g < 8 && b < 8)
                r = g = b = 8;
            else if (r == 255 && g == 0 && b == 255)
            {
                r = g = b = 0;
                alpha = 0;
                transparency = 1;
            }
            out[std::size_t(y) * s.width + x] =
                static_cast<std::uint16_t>(alpha | ((r & 0xf8) << 7) | ((g & 0xf8) << 2) | (b >> 3));
        }
    return out;
}
std::string resolved_name(const std::string &raw, const std::string &prefix)
{
    auto name = nul_text(raw);
    const auto track = nul_text(prefix);
    if (track.empty() || name.find("Trk_") == std::string::npos)
        return name;
    auto index = name.size();
    while (index > 0 && name[index] != '\\')
        --index;
    if (index)
        ++index;
    return track + "\\" + name.substr(index);
}
MaterialResourcePath select_image(const std::string &name, std::uint32_t requested, WorldMaterialLoadingState &s,
                                  const WorldMaterialLoadingContext &context, MaterialFileSource &files)
{
    s.width = 0;
    s.pc_flag = 0;
    s.format = 1;
    MaterialResourcePath path;
    for (int root = requested == 1 && context.quality == 1 ? 1 : 0; root >= 0; --root)
    {
        path = {root ? MaterialResourceRoot::graphics24_max : MaterialResourceRoot::graphics24, name + ".PC"};
        if (files.source_file_size(path) != 0)
        {
            std::array<std::uint8_t, 10> header{};
            const auto read = files.read_source_file(path, header);
            if (read.source_size != 0)
            {
                require(read.copied_bytes >= 10, "short nonzero PC header read outside valid source domain");
                s.width = half(header, 6);
                s.height = half(header, 8);
                s.pc_flag = 1;
                if (static_cast<std::int32_t>(s.width) <= context.maximum_dimension)
                    return path;
            }
        }
        path.relative_name = name + ".TGA";
        s.width = s.height = s.tga_width = s.tga_height = 0;
        std::array<std::uint8_t, 18> header{};
        const auto read = files.read_source_file(path, header);
        if (read.source_size != 0)
        {
            require(read.copied_bytes >= 18, "short nonzero TGA header read outside valid source domain");
            require(header[2] == 2 && header[16] == 24, "unsupported source TGA header");
            s.width = s.tga_width = half(header, 12);
            s.height = s.tga_height = half(header, 14);
        }
        // Preserve source missingTGA width0 and the earlier PC flag. Decoding
        // must still complete from real bytes; this comparison is not success.
        if (static_cast<std::int32_t>(s.width) <= context.maximum_dimension)
            return path;
    }
    return path;
}
void record_auxiliary(WorldMaterialLoadingState &s)
{
    const auto i = s.names.batch_upload_count;
    require(i < 255, "source per-batch upload index capacity");
    if (s.upload_to_logical.size() <= i)
        s.upload_to_logical.resize(std::size_t(i) + 1);
    s.upload_to_logical[i] = s.names.logical_base + i;
}
// The dimension generate_world_material (430730) gives a generated slot.
std::uint32_t generated_material_dimension(std::uint32_t fallback, std::uint32_t selector) noexcept
{
    return selector >= 4 && selector <= 8 ? 1u << selector : fallback;
}
void verify_completion(const CompletedMaterialUpload &result, std::uint32_t dimension)
{
    require(result.primary != 0 && result.alternate != 0, "backend returned fake/null completed material");
    require(result.dimension == static_cast<std::uint16_t>(dimension),
            "completed material dimension differs from source record");
}
void append_image(WorldMaterialLoadingState &s, const std::string &raw, std::uint32_t format,
                  std::uint32_t allocation_kind, MaterialFileSource &files, MaterialTextureBackend &backend)
{
    (void)allocation_kind; // native shared ownership replaces42E8D0 allocator category
    require(s.image_count < s.images.size(), "source active image record capacity");
    const auto name = nul_text(raw);
    const MaterialResourcePath path{MaterialResourceRoot::graphics24, "\\" + name + (format == 1 ? ".PC" : ".TGA")};
    SGN_LOAD_SCOPE("load.image.read_decode");
    auto payload = std::make_shared<MaterialImagePayload>();
    payload->identity = s.next_payload_identity++;
    auto input = read_image(files, path);
    if (format == 1)
    {
        s.width = half(input, 6);
        s.height = half(input, 8);
        payload->bytes = std::move(input);
    }
    else
    {
        auto words = tga_words(input, s, backend.current_transparency());
        payload->bytes.resize(words.size() * 2);
        std::memcpy(payload->bytes.data(), words.data(), payload->bytes.size());
    }
    s.image_byte_count += static_cast<std::uint32_t>(payload->bytes.size());
    auto &record = s.images[s.image_count++];
    record.name = {MaterialImageName::Arena::active, s.image_name_cursor};
    record.payload = std::move(payload);
    record.width = s.width;
    record.height = s.height;
    record.format = format;
    store_text(s.image_name_arena, s.image_name_cursor, name);
    s.image_name_cursor += static_cast<std::uint32_t>(name.size() + 1);
}
void cached_image(WorldMaterialLoadingState &s, const std::string &name, std::uint32_t format,
                  MaterialFileSource &files, MaterialTextureBackend &backend)
{
    for (std::uint32_t i = 0; i < s.persistent_count; ++i)
        if (same_insensitive(name, s.image_name(s.persistent_images[i].name)))
        {
            require(s.image_count < s.images.size(), "source active image cache-hit capacity");
            s.images[s.image_count++] = s.persistent_images[i];
            return;
        }
    require(s.persistent_count < s.persistent_images.size(), "source persistent image capacity");
    append_image(s, name, format, 1, files, backend);
    auto &record = s.persistent_images[s.persistent_count++];
    record = s.images[s.image_count - 1];
    record.marker = 1;
    record.name = {MaterialImageName::Arena::persistent, s.persistent_name_cursor};
    const auto text = nul_text(name);
    store_text(s.persistent_name_arena, s.persistent_name_cursor, text);
    s.persistent_name_cursor += static_cast<std::uint32_t>(text.size() + 1);
}
bool transient_image(const std::string &name, WorldMaterialLoadingState &s, const WorldMaterialLoadingContext &context)
{
    for (const auto *candidate : context.transient_image_names)
    {
        if (!candidate)
            break;
        if (same_insensitive(*candidate, name))
        {
            if (s.transient_seen == 0)
            {
                s.transient_byte_start = s.image_byte_count;
                s.transient_seen = 1;
            }
            return true;
        }
    }
    return false;
}
// NEW HOST (R30 perf-tick): the former SourceFrontendTextureReplacement raw_image, moved here unchanged
// (same checks, messages and arithmetic) so the load-time prewarm and the swap share one decode.
Image raw_replacement_image(const MaterialImageRecord &record)
{
    if (!record.payload)
        throw FormatError("frontend source texture has no loaded payload");
    const auto count = std::uint64_t(record.width) * record.height;
    if (count == 0 || count > record.payload->bytes.size() / 2)
        throw FormatError("frontend raw image exceeds the loaded payload");
    Image image{record.width, record.height, {}};
    image.rgba.reserve(static_cast<std::size_t>(count) * 4);
    const auto &bytes = record.payload->bytes;
    for (std::size_t i = 0; i < count; ++i)
    {
        const auto v = std::uint32_t(bytes[i * 2]) | (std::uint32_t(bytes[i * 2 + 1]) << 8);
        for (unsigned shift : {10u, 5u, 0u})
            image.rgba.push_back(static_cast<std::uint8_t>((((v >> shift) & 31u) * 255u + 15u) / 31u));
        image.rgba.push_back((v & 0x8000u) ? 255u : 0u);
    }
    return image;
}
// Bounded LRU of decoded replacement images. The key is the payload OBJECT (weak_ptr: an expired entry
// never matches, so a later payload at a reused address cannot alias it) plus every other decode input
// (format, width, height). Payloads are created once in append_image and only ever shared as const.
struct ReplacementImageCache
{
    struct Entry
    {
        std::weak_ptr<const MaterialImagePayload> payload;
        std::uint32_t format{}, width{}, height{};
        std::shared_ptr<const Image> image;
    };
    std::mutex mutex;
    // NEW HOST (R35): 256 entries under a 96 MB budget (was 32 entries): a menu cycles through more
    // pictures than 32, and every one of them is decoded ahead by the menu prewarm.
    static constexpr std::size_t byte_budget = 96u * 1024u * 1024u;
    std::array<Entry, 256> entries{}; // [0] = most recently used
    std::size_t used{};
};
ReplacementImageCache &replacement_image_cache()
{
    static auto *cache = new ReplacementImageCache; // leaked on purpose: no static destruction order issue
    return *cache;
}
// Load-time prewarm of a race-time 430D40 swap source (NEW HOST R30): decode it now and fill the sharp-HUD
// upscale cache with exactly the words the later replacement upload makes. Only the caches change; no
// source state, record, ordinal or upload is touched, and a failure is ignored (the swap then does the
// work itself, as before).
// NEW HOST (R35): the work of one prewarm: the decode (into the decoded-image cache), then the upscale and
// the converted levels of every noted replacement shape (host_texture_upscale::prewarm_rgba8). Any thread;
// the record is a copy holding the immutable payload.
void prewarm_replacement_record(const MaterialImageRecord &record, host_texture_upscale::TextureClass texture_class)
{
    const auto image = source_replacement_image(record);
    host_texture_upscale::prewarm_rgba8(image->rgba, image->width, image->height, texture_class);
}
// Queues that work on host_parallel's background threads (R35: it ran inside the race load pass, ~4.5 ms
// per image). A swap that comes before its prewarm finished converts its image itself, as before; the
// caches then hold the one result (identical values either way).
void queue_replacement_prewarm(const MaterialImageRecord &record, host_texture_upscale::TextureClass texture_class)
{
    MaterialImageRecord copy{};
    copy.payload = record.payload;
    copy.width = record.width;
    copy.height = record.height;
    copy.format = record.format;
    (void)host_parallel::post_background([copy, texture_class] {
        try
        {
            prewarm_replacement_record(copy, texture_class);
        }
        catch (...)
        {
        }
    });
}
void prewarm_replacement_image(const WorldMaterialLoadingState &s, const MaterialImageRecord &record) noexcept
{
    try
    {
        const auto texture_class = host_texture_upscale::classify_source_name(s.image_name(record.name));
        if (texture_class != host_texture_upscale::TextureClass::hud)
            return; // only Overlays\ images (the race swap sources); menu art: prewarm_menu_replacements
        SGN_PROFILE_SCOPE("load.swap_prewarm"); // R35: now only the queueing
        if (record.payload)
            queue_replacement_prewarm(record, texture_class);
    }
    catch (...)
    {
    }
}
// NEW HOST (R35) menu swap prewarm. A menu's 430D40 replacements write the generated (430730) slots of its
// prelude; a source image can only be swapped into a slot of its own width (430D86). So every 2D picture
// of this prelude (FrontEnd/Boot/Loading/Tiles: not Overlays\, which prewarm_replacement_image covers)
// whose width is the dimension of a slot generated here is decoded, upscaled and converted ahead, on the
// background threads, for every noted replacement shape. Host caches only: no source state is touched.
void prewarm_menu_replacements(const WorldMaterialLoadingState &s, std::uint32_t first_image,
                               std::span<const std::uint32_t> generated_dimensions) noexcept
{
    try
    {
        std::uint32_t queued = 0;
        for (std::uint32_t i = first_image; i < s.image_count && i < s.images.size(); ++i)
        {
            const auto &record = s.images[i];
            if (!record.payload || std::find(generated_dimensions.begin(), generated_dimensions.end(), record.width) ==
                                       generated_dimensions.end())
                continue;
            const auto texture_class = host_texture_upscale::classify_source_name(s.image_name(record.name));
            if (texture_class == host_texture_upscale::TextureClass::hud ||
                texture_class == host_texture_upscale::TextureClass::world)
                continue;
            queue_replacement_prewarm(record, texture_class);
            ++queued;
        }
        sgn::host_profile::add("load.menu_swap_prewarm_queued", queued); // counter (value, not time)
    }
    catch (...)
    {
    }
}
} // namespace
std::shared_ptr<const Image> source_replacement_image(const MaterialImageRecord &record)
{
    if (!record.payload)
        throw FormatError("frontend texture points to an absent image payload");
    auto &cache = replacement_image_cache();
    {
        const std::lock_guard lock(cache.mutex);
        for (std::size_t i = 0; i < cache.used; ++i)
        {
            const auto &e = cache.entries[i];
            if (e.format != record.format || e.width != record.width || e.height != record.height ||
                e.payload.lock() != record.payload)
                continue;
            auto image = e.image;
            std::rotate(cache.entries.begin(), cache.entries.begin() + static_cast<std::ptrdiff_t>(i),
                        cache.entries.begin() + static_cast<std::ptrdiff_t>(i) + 1);
            return image;
        }
    }
    SGN_PROFILE_SCOPE("swap.decode_image");
    auto image = std::make_shared<const Image>(record.format == 1 ? decode_tm(record.payload->bytes)
                                                                  : raw_replacement_image(record));
    const std::lock_guard lock(cache.mutex);
    if (cache.used < cache.entries.size())
        ++cache.used;
    std::rotate(cache.entries.begin(), cache.entries.begin() + static_cast<std::ptrdiff_t>(cache.used) - 1,
                cache.entries.begin() + static_cast<std::ptrdiff_t>(cache.used));
    cache.entries[0] = {record.payload, record.format, record.width, record.height, image};
    // NEW HOST (R35): drop the least recently used entries beyond the byte budget (the newest is kept).
    std::size_t bytes = 0, keep = 0;
    for (; keep < cache.used; ++keep)
    {
        const auto size = cache.entries[keep].image ? cache.entries[keep].image->rgba.size() : 0;
        if (keep > 0 && bytes + size > ReplacementImageCache::byte_budget)
            break;
        bytes += size;
    }
    for (std::size_t i = keep; i < cache.used; ++i)
        cache.entries[i] = {};
    cache.used = keep;
    return image;
}
std::string WorldMaterialLoadingState::image_name(MaterialImageName name) const
{
    if (name.arena == MaterialImageName::Arena::none)
        return {};
    return name.arena == MaterialImageName::Arena::active ? arena_text(image_name_arena, name.offset)
                                                          : arena_text(persistent_name_arena, name.offset);
}
std::uint32_t WorldMaterialLoadingState::image_ordinal(std::uint32_t index) const
{
    if (index < 255)
        return image_ordinals[index];
    const auto at = std::uint64_t(index - 255) * 4;
    require(at + 4 <= image_name_arena.size(), "source image ordinal outside shared arena");
    std::uint32_t value = 0;
    for (unsigned i = 0; i < 4; ++i)
        value |= std::uint32_t(image_name_arena[static_cast<std::size_t>(at) + i]) << (i * 8);
    return value;
}
void reset_world_material_loading(WorldMaterialLoadingState &s, MaterialTextureBackend &backend)
{
    backend.release_all();
    if (s.cache_initialized != 1)
    {
        s.persistent_name_arena.fill(0);
        s.persistent_images = {};
        s.persistent_count = s.persistent_name_cursor = 0;
        s.cache_initialized = 1;
    }
    s.image_ordinals.fill(0xffffffff);
    s.names.logical_names.clear();
    s.names.unique_names.clear();
    s.names.materials.reset_loading_42c480();
    if (s.effect_animation_storage)
        s.effect_animation_storage->reset_loading_cursor_42c480(); // 42C4BE, retain rows/flag
    s.names.physical_upload_count = 0;                             // 582724/582718 are retained until the next batch
    s.generated_cursor = 0;
    s.generated_names.clear();
    s.generated_offsets.clear();
    s.image_name_cursor = 0;
    s.image_byte_count = 0;
    s.image_name_arena[0] = 0;
    s.images[0].name = {};
    s.image_count = 0; // 42BC11 MOV [583990],AL
    s.byte_count = 0;
    s.transient_seen = 0;
    s.transient_byte_start = 0;
}
void load_world_material_request(const BorrowedMaterialName &raw, std::uint32_t quality, WorldMaterialLoadingState &s,
                                 const WorldMaterialLoadingContext &c, MaterialFileSource &files,
                                 MaterialTextureBackend &backend)
{
    if (world_material_name_lookup(s.names.logical_names, raw) == 0)
    {
        commit_world_material_request(s.names, raw, std::nullopt);
        return;
    }
    require(c.source_graphics_roots != nullptr, "material loading requires explicit source graphics roots");
    const auto resolved = resolved_name(raw.text_snapshot(), c.track_prefix);
    std::optional<host_load_timeline::Scope> stage(std::in_place, "load.material.probe", false);
    const auto path = select_image(resolved, quality, s, c, files);
    stage.reset();
    const auto physical_dimension = s.width;
    // 4777D4/4777DC: %s%s.PC/TGA. select_image already supplied the extension.
    // Preserve source first-NUL spelling and all existing separators.
    const auto &root = path.root == MaterialResourceRoot::graphics24 ? c.source_graphics_roots->normal
                                                                     : c.source_graphics_roots->maximum;
    const auto source_path = std::string(root.text()) + path.relative_name;
    const auto motion = next_world_material_motion_index(s.names);
    s.names.materials.prepare_next(motion);
    backend.begin_pending_record(motion);
    append_material_reload_42be20(s.reload, s.names.physical_upload_count, source_path);
    require(physical_dimension > 0 && physical_dimension <= 4096, "missing or unsupported source physical dimension");
    stage.emplace("load.material.read", false);
    const auto raw_image = read_image(files, path);
    stage.emplace("load.material.decode", false);
    std::vector<std::uint16_t> words;
    const bool compressed = s.pc_flag == 1;
    if (compressed)
    {
        words = pc_words(raw_image);
        s.width = half(raw_image, 6);
        s.height = half(raw_image, 8);
        s.pc_flag = 0;
    }
    else
        words = tga_words(raw_image, s, backend.current_transparency());
    // 42BF60 passes probed width for both physical dimensions to4306C0.
    const auto physical_count = std::size_t(physical_dimension) * physical_dimension;
    // For a short TGA rectangle,430280 still converts the entire probed square.
    // Zero-initialized RGB tail becomes(8,8,8), not transparent zero.
    require(words.size() <= physical_count, "source image overflows its square physical allocation");
    words.resize(physical_count, static_cast<std::uint16_t>(compressed ? 0u : 0x8421u));
    // NEW HOST (R28) sharp HUD: names this upload's texture class (2D UI or 3D) for the renderer.
    const host_texture_upscale::ScopedTextureSource texture_source(path.relative_name);
    stage.emplace("load.material.upload", false); // conversion + GPU upload (renderer)
    const auto completed = backend.upload({physical_dimension, physical_dimension, words});
    stage.reset();
    verify_completion(completed, physical_dimension);
    s.names.materials.complete_next(completed.primary, completed.alternate, completed.dimension);
    s.byte_count += s.width * s.height * (s.format == 1 ? 2u : 1u);
    record_auxiliary(s);
    publish_world_material_request_42c03c(s.names, raw);
}
void generate_world_material(std::uint32_t fallback, std::uint32_t selector, WorldMaterialLoadingState &s,
                             MaterialTextureBackend &backend)
{
    SGN_LOAD_SCOPE("load.material.generate");
    const auto dimension = generated_material_dimension(fallback, selector);
    s.names.materials.prepare_next(0);
    backend.begin_pending_record(0);
    const auto completed = backend.generate(dimension);
    verify_completion(completed, dimension);
    s.names.materials.complete_next(completed.primary, completed.alternate, completed.dimension);
    s.byte_count += dimension * dimension * (s.format == 1 ? 2u : 1u);
    record_auxiliary(s);
    ++s.names.batch_upload_count;
    ++s.names.physical_upload_count;
    const auto text = std::to_string(std::bit_cast<std::int32_t>(s.names.batch_upload_count));
    const auto offset = s.generated_cursor;
    store_text(s.generated_arena, offset, text);
    s.generated_offsets.push_back(offset);
    s.generated_names.emplace_back();
    for (std::size_t i = 0; i < s.generated_names.size(); ++i)
        s.generated_names[i] = arena_text(s.generated_arena, s.generated_offsets[i]);
    s.generated_cursor += static_cast<std::uint32_t>(text.size()); // source deliberately excludes NUL
    const auto *name = &s.generated_names.back();
    s.names.logical_names.push_back(name);
    s.names.unique_names.push_back(name);
}
void load_world_material_requests(std::span<const std::string> names, WorldMaterialLoadingState &s,
                                  const WorldMaterialLoadingContext &c, MaterialFileSource &files,
                                  MaterialTextureBackend &backend)
{
    begin_world_material_batch(s.names);
    for (const auto &name : names)
        load_world_material_request(name, c.quality, s, c, files, backend);
}
void load_world_material_prelude(WorldMaterialPrelude table, WorldMaterialLoadingState &s,
                                 const WorldMaterialLoadingContext &c, MaterialFileSource &files,
                                 MaterialTextureBackend &backend)
{
    SGN_LOAD_STAGE("load.material_prelude");
    s.names.sky_material_base = 0;
    for (const auto &e : table)
    {
        if (!e.name)
            break;
        if (e.type == 0 || e.type == 2 || e.type == 3)
            ++s.names.sky_material_base;
    }
    begin_world_material_batch(s.names);
    std::uint32_t ordinal = 0;
    const auto first_image = s.image_count;          // NEW HOST (R35) menu swap prewarm
    std::vector<std::uint32_t> generated_dimensions; // NEW HOST (R35) menu swap prewarm
    for (const auto &e : table)
    {
        if (!e.name)
            break;
        if (e.type == 1)
        {
            if (c.scene == 2 && !transient_image(*e.name, s, c))
                cached_image(s, *e.name, e.selector, files, backend);
            else
                append_image(s, *e.name, e.selector, 0, files, backend);
            set_ordinal(s, ordinal, s.image_count - 1);
            prewarm_replacement_image(s, s.images[s.image_count - 1]); // NEW HOST (R30): caches only
        }
        else if (e.type == 2 || e.type == 3)
        {
            generate_world_material(e.type, e.selector, s, backend);
            generated_dimensions.push_back(generated_material_dimension(e.type, e.selector)); // NEW HOST (R35)
        }
        else
            load_world_material_request(*e.name, c.scene == 2 ? 0u : 1u, s, c, files, backend);
        ++ordinal;
    }
    prewarm_menu_replacements(s, first_image, generated_dimensions); // NEW HOST (R35): host caches only
}
void load_world_materials(WorldMaterialRegistry registry, WorldMaterialLoadingState &s, WorldMaterialLoadingContext &c,
                          MaterialFileSource &files, MaterialTextureBackend &backend,
                          WorldMaterialLoadingPresentation *presentation)
{
    SGN_LOAD_STAGE("load.world_materials");
    reset_material_reload_42bdd0(s.reload); // 42C510 first call;42C480 retains this owner.
    if (c.loading_presentation_enabled == 1 && c.loading_already_shown != 1)
    {
        if (!presentation)
            throw MaterialSourceFailure(0x42c51e, 0,
                                        "unsupported-loading-presentation: original445A40 consumer required");
        presentation->run(s, c, files, backend);
    }
    reset_world_material_loading(s, backend);
    load_world_material_prelude(source_world_material_prelude(c.scene), s, c, files, backend);
    SGN_LOAD_SCOPE("load.world_materials.models");
    for (const auto &entry : registry)
    {
        if (!entry.model)
            break;
        if (entry.flags04 != 0)
            continue;
        load_world_material_requests(entry.model->raw_names, s, c, files, backend);
        add_world_material_base(*entry.model, s.names.logical_base);
    }
    remap_world_material_names(registry, s.names.logical_names, s.names.unique_names);
}
// Data-only extraction of both pinned source tables. Stable string OBJECTS live
// for the process; raw PMD names retain their separate resource-owner lifetime.
#include "retail_world_material_preludes.inc"

void load_world_material_request(const std::string &raw, std::uint32_t quality, WorldMaterialLoadingState &s,
                                 const WorldMaterialLoadingContext &c, MaterialFileSource &files,
                                 MaterialTextureBackend &backend)
{
    load_world_material_request(BorrowedMaterialName(raw), quality, s, c, files, backend);
}
void load_source_pmd_material_requests_42c440(const SourcePmdMaterialNames &names, WorldMaterialLoadingState &s,
                                              const WorldMaterialLoadingContext &c, MaterialFileSource &files,
                                              MaterialTextureBackend &backend)
{
    SGN_LOAD_STAGE("load.pmd_material_requests");
    begin_world_material_batch(s.names); // 42C440..42C44F, before even the first table read
    for (std::uint32_t index = 0;; ++index)
    {
        const auto raw = names.at(index); // 42C454 /42C46A: reached current pointer only
        if (!raw)
            return;
        load_world_material_request(raw, c.quality, s, c, files, backend); // same physical owner/backend
    }
}
} // namespace sgn::retail
