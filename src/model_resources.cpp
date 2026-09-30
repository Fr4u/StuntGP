#include "sgn/model_resources.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <limits>
#include <type_traits>

namespace sgn::retail
{
namespace
{
std::uint32_t word(std::span<const std::uint8_t> bytes, std::size_t offset)
{
    if (offset > bytes.size() || bytes.size() - offset < 4)
        throw FormatError("model resource word outside file");
    std::uint32_t value;
    std::memcpy(&value, bytes.data() + offset, 4);
    return value;
}
void validate_name(std::string_view name)
{
    if (name.empty() || name.find_first_of("/\\:") != std::string_view::npos ||
        name.find("..") != std::string_view::npos)
        throw FormatError("invalid model resource name");
}
VehicleVector vector(Vec3 xyz, std::uint32_t w)
{
    return {xyz.x, xyz.y, xyz.z, std::bit_cast<float>(w)};
}
template <class T> std::vector<T> source_section(std::span<const std::uint8_t> raw, std::size_t header)
{
    static_assert(std::is_trivially_copyable_v<T>);
    const auto at = 328ull + word(raw, header), bytes = std::uint64_t(word(raw, header + 0x94));
    if (bytes % sizeof(T) || at > raw.size() || bytes > raw.size() - at)
        throw FormatError("model render section outside original PMD");
    std::vector<T> out(static_cast<std::size_t>(bytes / sizeof(T)));
    if (bytes)
        std::memcpy(out.data(), raw.data() + at, static_cast<std::size_t>(bytes));
    return out;
}
} // namespace
ModelResource::ModelResource(const AssetStore &assets, const std::filesystem::path &retail_root,
                             const std::string &name, std::string_view track_set)
    : source_name_(name)
{
    validate_name(name);
    const auto raw = read_file(retail_root / "meshdata" / (name + ".PMD"));
    // Existing validated parser checks all section spans, overlap, every part
    // transform index and the authored LOD domain before raw word ownership.
    auto first = assets.rig(name, 0, track_set);
    const auto descriptor = 328 + std::size_t(word(raw, 0x4c));
    const auto names_at = 328ull + word(raw, 0x38), names_bytes = std::uint64_t(word(raw, 0xcc));
    const auto strings_at = 328ull + word(raw, 0x34), strings_bytes = std::uint64_t(word(raw, 0xc8));
    if (names_bytes % 4 || names_at > raw.size() || names_bytes > raw.size() - names_at || strings_at > raw.size() ||
        strings_bytes > raw.size() - strings_at)
        throw FormatError("model raw texture name sections outside PMD");
    raw_texture_names_.reserve(static_cast<std::size_t>(names_bytes / 4));
    bool names_terminated = names_bytes == 0;
    for (std::size_t i = 0; i < names_bytes; i += 4)
    {
        const auto offset = word(raw, static_cast<std::size_t>(names_at) + i);
        if (offset == 0)
        {
            names_terminated = true;
            break;
        }
        const auto begin = 328ull + offset, end = strings_at + strings_bytes;
        if (begin < strings_at || begin >= end)
            throw FormatError("model raw texture name pointer outside source strings");
        const auto name_begin = raw.begin() + static_cast<std::ptrdiff_t>(begin),
                   limit = raw.begin() + static_cast<std::ptrdiff_t>(end);
        const auto name_end = std::find(name_begin, limit, std::uint8_t{});
        if (name_end == limit)
            throw FormatError("unterminated source model texture name");
        raw_texture_names_.emplace_back(name_begin, name_end);
    }
    if (!names_terminated)
        throw FormatError("unterminated source model texture-name table");
    current_detail_level_ = word(raw, descriptor + 0x5c);
    previous_detail_level_ = word(raw, descriptor + 0x64);
    model_flags_02_ = static_cast<std::uint16_t>(word(raw, descriptor) >> 16);
    model_ambience_ = static_cast<std::uint16_t>(word(raw, descriptor + 4));
    std::memcpy(&descriptor_vector_10_, raw.data() + descriptor + 0x10, sizeof(descriptor_vector_10_));
    std::memcpy(bounds_20_.data(), raw.data() + descriptor + 0x20, 4);
    // 42EBF0 descriptor+28 and42EBB0. Keep authored normal/bound words and
    // index aliases; no reconstructed AABB or normal generation substitutes.
    normals_ = source_section<VehicleVector>(raw, 0x20);
    bound_vectors_ = source_section<VehicleVector>(raw, 0x3c);
    bound_indices_ = source_section<std::uint32_t>(raw, 0x40);
    for (const auto index : bound_indices_)
        if (index >= bound_vectors_.size())
            throw FormatError("model bound pointer index outside source vectors");
    if (word(raw, 0x18) == 1)
    {
        ModelSpatialData spatial;
        spatial.bounds = source_section<VehicleVector>(raw, 0x7c);
        spatial.vertex_indices = source_section<std::uint16_t>(raw, 0x80);
        spatial.polygon_indices = source_section<std::uint16_t>(raw, 0x84);
        spatial.nodes = source_section<std::array<std::uint16_t, 32>>(raw, 0x88);
        if (spatial.nodes.empty())
            throw FormatError("track has no original spatial root");
        for (const auto &node : spatial.nodes)
            for (unsigned i = 0; i < 8; ++i)
            {
                const auto at = i * 4;
                if (node[at] == 0xffff)
                    continue;
                if (node[at] >= spatial.nodes.size() || node[at + 1] >= spatial.polygon_indices.size() ||
                    node[at + 2] >= spatial.vertex_indices.size() || node[at + 3] > spatial.bounds.size() ||
                    spatial.bounds.size() - node[at + 3] < 8)
                    throw FormatError("track spatial child outside original tables");
            }
        spatial_ = std::move(spatial);
    }
    const auto transform_at = 328 + std::size_t(word(raw, 0x44));
    const auto transform_bytes = word(raw, 0xd8), part_bytes = word(raw, 0xdc);
    static_assert(std::endian::native == std::endian::little && sizeof(ModelTransform) == 80 &&
                  std::is_trivially_copyable_v<ModelTransform>);
    if (transform_bytes % 80 || part_bytes % 64 || transform_at > raw.size() ||
        transform_bytes > raw.size() - transform_at)
        throw FormatError("model transform section invalid");
    transforms_.resize(transform_bytes / 80);
    if (transform_bytes)
        std::memcpy(transforms_.data(), raw.data() + transform_at, transform_bytes);
    const auto vertex_at = 328 + std::size_t(word(raw, 0x2c));
    const auto vertex_bytes = word(raw, 0xc0);
    if (vertex_bytes % 16 || vertex_at > raw.size() || vertex_bytes > raw.size() - vertex_at)
        throw FormatError("model vertex section invalid");
    static_assert(sizeof(VehicleVector) == 16 && std::is_trivially_copyable_v<VehicleVector>);
    vertices_.resize(vertex_bytes / 16);
    if (vertex_bytes)
        std::memcpy(vertices_.data(), raw.data() + vertex_at, vertex_bytes);
    const auto index_at = 328 + std::size_t(word(raw, 0x30));
    const auto index_bytes = word(raw, 0xc4);
    if (index_bytes % 2 || index_at > raw.size() || index_bytes > raw.size() - index_at)
        throw FormatError("model corner index section invalid");
    corner_indices_.resize(index_bytes / 2);
    if (index_bytes)
        std::memcpy(corner_indices_.data(), raw.data() + index_at, index_bytes);
    const auto attribute_at = 328 + std::size_t(word(raw, 0x24)), coordinate_at = 328 + std::size_t(word(raw, 0x28));
    const auto attribute_bytes = word(raw, 0xb8), coordinate_bytes = word(raw, 0xbc);
    if (attribute_bytes % 8 || coordinate_bytes % 16 || attribute_at > raw.size() ||
        attribute_bytes > raw.size() - attribute_at || coordinate_at > raw.size() ||
        coordinate_bytes > raw.size() - coordinate_at)
        throw FormatError("model mutable attribute/coordinate section invalid");
    attributes_.resize(attribute_bytes / 8);
    if (attribute_bytes)
        std::memcpy(attributes_.data(), raw.data() + attribute_at, attribute_bytes);
    coordinates_.resize(coordinate_bytes / 16);
    if (coordinate_bytes)
        std::memcpy(coordinates_.data(), raw.data() + coordinate_at, coordinate_bytes);
    const auto part_at = 328 + std::size_t(word(raw, 0x48));
    parts_.resize(part_bytes / 64);
    std::vector<bool> seen(parts_.size());
    lods_.reserve(first.geometry.lod_count);
    const auto lod_count = first.geometry.lod_count;
    for (unsigned slot = 0; slot < 4; ++slot)
    {
        const auto authored = word(raw, descriptor + 0x48 + slot * 4);
        for (unsigned lod = 0; lod < lod_count; ++lod)
            if (word(raw, descriptor + 0x48 + lod * 4) == authored)
            {
                source_lods_[slot] = lod;
                break;
            }
    }
    for (unsigned lod = 0; lod < lod_count; ++lod)
    {
        auto rig = lod == 0 ? std::move(first) : assets.rig(name, lod, track_set);
        const auto start = word(raw, descriptor + 0x48 + lod * 4);
        if (start > parts_.size() || rig.parts.size() > parts_.size() - start)
            throw FormatError("model LOD part span invalid");
        Lod storage;
        storage.geometry = std::move(rig.geometry);
        storage.parts.reserve(rig.parts.size());
        storage.first_part = start;
        for (unsigned j = 0; j < rig.parts.size(); ++j)
        {
            const auto &p = rig.parts[j];
            const std::size_t at = start + j;
            if (p.transform_index >= transforms_.size())
                throw FormatError("model transform identity invalid");
            if (!seen[at])
            {
                const auto first_vertex = word(raw, part_at + at * 64 + 0x30);
                const auto vertex_count = word(raw, part_at + at * 64 + 0x28);
                if (first_vertex > vertices_.size() || vertex_count > vertices_.size() - first_vertex)
                    throw FormatError("model source vertex span invalid");
                const auto first_attribute = word(raw, part_at + at * 64 + 0x34),
                           first_coordinate = word(raw, part_at + at * 64 + 0x2c);
                const auto attribute_count = static_cast<std::uint16_t>(word(raw, part_at + at * 64 + 0x24)),
                           coordinate_count = static_cast<std::uint16_t>(word(raw, part_at + at * 64 + 0x24) >> 16);
                if (first_attribute > attributes_.size() || attribute_count > attributes_.size() - first_attribute ||
                    first_coordinate > coordinates_.size() || coordinate_count > coordinates_.size() - first_coordinate)
                    throw FormatError("model mutable part attribute/coordinate span invalid");
                parts_[at] = {p.flags,
                              static_cast<std::uint16_t>(word(raw, part_at + at * 64) >> 16),
                              p.transform_index,
                              p.pivot,
                              first_vertex,
                              vertex_count,
                              true,
                              first_attribute,
                              first_coordinate,
                              attribute_count,
                              coordinate_count};
                std::memcpy(parts_[at].bounds_20.data(), raw.data() + part_at + at * 64 + 0x20, 4);
                parts_[at].word_04 = static_cast<std::uint16_t>(word(raw, part_at + at * 64 + 4));
                seen[at] = true;
            }
            else if (parts_[at].flags != p.flags || parts_[at].transform != p.transform_index)
                throw FormatError("inconsistent aliased LOD part");
            storage.parts.push_back(at);
        }
        lods_.push_back(std::move(storage));
    }
}
std::size_t ModelResource::part_count(unsigned lod) const
{
    return lods_.at(lod).parts.size();
}
std::uint32_t &ModelResource::current_detail_level() noexcept
{
    if (auto *owned = std::get_if<std::uint32_t>(&current_detail_level_))
        return *owned;
    return std::get<std::reference_wrapper<std::uint32_t>>(current_detail_level_).get();
}
const std::uint32_t &ModelResource::current_detail_level() const noexcept
{
    if (const auto *owned = std::get_if<std::uint32_t>(&current_detail_level_))
        return *owned;
    return std::get<std::reference_wrapper<std::uint32_t>>(current_detail_level_).get();
}
void ModelResource::bind_current_detail_level(std::uint32_t &session_word)
{
    if (&current_detail_level() == &session_word)
        return;
    if (!std::holds_alternative<std::uint32_t>(current_detail_level_))
        throw FormatError("source model current LOD already belongs to another Session word");
    session_word = current_detail_level();
    current_detail_level_ = std::ref(session_word);
}
unsigned ModelResource::source_lod_index(unsigned source_slot) const
{
    const auto &index = source_lods_.at(source_slot);
    if (!index)
        throw FormatError("source LOD slot has no relocated resident owner");
    return *index;
}
const Model &ModelResource::geometry(unsigned lod) const
{
    return lods_.at(lod).geometry;
}
void ModelResource::replace_source_texture_suffix(std::string_view marker, std::int32_t value)
{
    if (marker.empty() || marker.find('\0') != marker.npos)
        throw FormatError("invalid source texture marker");
    // The original descriptor has one texture-name table for all four source
    // LOD slots. Lod::geometry retains render snapshots of that same table.
    auto names = lods_.at(0).geometry.textures;
    const auto suffix = std::to_string(value);
    for (auto &name : raw_texture_names_)
    {
        const auto at = name.find(marker);
        if (at != name.npos)
            name.replace(at + marker.size(), name.size() - (at + marker.size()), suffix);
    }
    for (auto &name : names)
    {
        const auto at = name.find(marker);
        if (at != name.npos)
            name.replace(at + marker.size(), name.size() - (at + marker.size()), suffix);
    }
    for (auto &lod : lods_)
        lod.geometry.textures = names;
}
ModelPartBinding ModelResource::part(unsigned lod, unsigned index)
{
    auto &p = parts_.at(lods_.at(lod).parts.at(index));
    return {p.flags, transforms_.at(p.transform), p.pivot};
}
ModelResource::Part &ModelResource::source_record(unsigned lod, unsigned index)
{
    const auto first = lods_.at(lod).first_part;
    if (index >= parts_.size() || first >= parts_.size() - index)
        throw std::out_of_range("source part outside model section");
    auto &p = parts_[first + index];
    if (!p.relocated)
        throw FormatError("source access reaches unrelocated model part");
    return p;
}
ModelPartBinding ModelResource::source_part(unsigned lod, unsigned index)
{
    auto &p = source_record(lod, index);
    return {p.flags, transforms_.at(p.transform), p.pivot};
}
std::uint16_t &ModelResource::source_part_flags_02(unsigned lod, unsigned index)
{
    return source_record(lod, index).flags_02;
}
std::uint16_t &ModelResource::source_part_word_04(unsigned lod, unsigned index)
{
    return source_record(lod, index).word_04;
}
std::span<ModelTransform> ModelResource::source_transform_run(unsigned lod, unsigned part, unsigned count)
{
    const auto first = source_record(lod, part).transform;
    if (first > transforms_.size() || count > transforms_.size() - first)
        throw std::out_of_range("source transform run outside model table");
    return std::span<ModelTransform>(transforms_).subspan(first, count);
}
std::span<std::array<std::uint8_t, 8>> ModelResource::source_attributes(unsigned lod, unsigned part)
{
    const auto &record = source_record(lod, part);
    return std::span(attributes_).subspan(record.first_attribute, record.attribute_count);
}
std::span<std::array<std::uint32_t, 4>> ModelResource::source_coordinates(unsigned lod, unsigned part)
{
    const auto &record = source_record(lod, part);
    return std::span(coordinates_).subspan(record.first_coordinate, record.coordinate_count);
}
std::span<const VehicleVector> ModelResource::source_vertices(unsigned lod, unsigned part)
{
    const auto &record = source_record(lod, part);
    return std::span<const VehicleVector>(vertices_).subspan(record.first_vertex, record.vertex_count);
}
std::span<VehicleVector> ModelResource::mutable_source_vertices(unsigned lod, unsigned part)
{
    const auto &record = source_record(lod, part);
    return std::span(vertices_).subspan(record.first_vertex, record.vertex_count);
}
const std::array<std::uint16_t, 2> &ModelResource::source_part_bounds(unsigned lod, unsigned part)
{
    return source_record(lod, part).bounds_20;
}
float ModelResource::maximum_source_vertex_z(unsigned lod, unsigned index)
{
    const auto &p = source_record(lod, index);
    float maximum = 0;
    for (unsigned i = 0; i < static_cast<std::uint16_t>(p.vertex_count); ++i)
    {
        const float z = vertices_[std::size_t(p.first_vertex) + i].z;
        if (!std::isfinite(z))
            throw FormatError("nonfinite source vertex");
        if (maximum < z)
            maximum = z;
    }
    return maximum;
}
VehiclePresentationParts ModelResource::presentation_parts(unsigned lod)
{
    if (part_count(lod) < 21)
        throw FormatError("vehicle presentation needs source parts 0..20");
    const auto p19 = part(lod, 19), p20 = part(lod, 20);
    return {{part(lod, 11).flags, part(lod, 12).flags, part(lod, 13).flags, part(lod, 14).flags, part(lod, 15).flags,
             part(lod, 16).flags},
            {p19.flags, p19.transform.scale},
            {p20.flags, p20.transform.scale}};
}
VehicleTrailModel ModelResource::trail_model()
{
    if (part_count(0) < 24)
        throw FormatError("vehicle trails need source LOD-zero parts 0..23");
    const auto &t = part(0, 0).transform;
    return {vector(t.forward, t.forward_w), vector(t.up, t.up_w), part(0, 23).transform.position.z};
}
std::uint32_t ModelResources::load(const std::string &name, std::string_view track_set)
{
    if (source_bank_)
        throw FormatError("legacy model load cannot enter a source-bank resource table");
    validate_name(name);
    const auto identity = checked_imported_model_identity(models_.size());
    // Construct before exposing a token. deque preserves existing borrowed
    // transforms/flags when another asset is loaded during preparation.
    models_.emplace_back(std::in_place_type<ModelResource>, assets_, root_, name, track_set);
    return identity;
}
std::uint32_t ModelResources::load_source(SourceAllocationBank &bank, const std::string &name,
                                          SourcePmdObserver *observer)
{
    SourcePmdFileSystem files(root_);
    return load_source(bank, files, name, observer);
}
std::uint32_t ModelResources::load_source(SourceAllocationBank &bank, SourcePmdFileSource &files,
                                          const std::string &name, SourcePmdObserver *observer)
{
    if (!bank.initialized())
        throw FormatError("source PMD table requires explicit prior42EAB0");
    if (source_bank_ && source_bank_ != &bank)
        throw FormatError("PMD table belongs to another source bank");
    if (!source_bank_ && !models_.empty())
        throw FormatError("PMD source mode cannot inherit legacy decoded owners");
    const auto id = checked_imported_model_identity(models_.size());
    source_bank_ = &bank;
    auto model = load_source_pmd_42efb0(bank, source_loading_, files, name, observer);
    models_.emplace_back(std::in_place_type<SourcePmdModel>, std::move(model));
    return id;
}
SourcePmdModelView ModelResources::source_at(std::size_t handle) const
{
    const auto *model = std::get_if<SourcePmdModel>(&models_.at(handle));
    if (!model)
        throw FormatError("legacy model has no canonical source-bank view");
    return model->view();
}
bool ModelResources::source_backed(std::size_t handle) const
{
    return std::holds_alternative<SourcePmdModel>(models_.at(handle));
}
std::uint32_t ModelResources::current_detail_level(std::size_t handle) const
{
    if (source_backed(handle))
        return source_at(handle).current_detail_level().value();
    return std::get<ModelResource>(models_.at(handle)).current_detail_level();
}
unsigned ModelResources::source_lod_index(std::size_t handle, unsigned slot) const
{
    if (source_backed(handle))
        return source_at(handle).source_lod_index(slot);
    return std::get<ModelResource>(models_.at(handle)).source_lod_index(slot);
}
std::size_t ModelResources::distinct_lod_count(std::size_t handle) const
{
    if (source_backed(handle))
        return std::size_t(source_at(handle).maximum_detail_level()) + 1;
    return std::get<ModelResource>(models_.at(handle)).lod_count();
}
VehicleVector ModelResources::source_pivot(std::size_t handle) const
{
    if (source_backed(handle))
        return source_at(handle).descriptor().field<VehicleVector>(0x10).value();
    return std::get<ModelResource>(models_.at(handle)).source_model_vector_10();
}
std::string_view ModelResources::source_name(std::size_t handle) const
{
    return std::visit([](const auto &model) { return model.source_name(); }, models_.at(handle));
}
void ModelResources::replace_texture_suffix(std::size_t handle, std::string_view marker, std::int32_t value)
{
    auto &model = models_.at(handle);
    if (auto *source = std::get_if<SourcePmdModel>(&model))
        source->view().replace_texture_suffix(marker, value);
    else
        std::get<ModelResource>(model).replace_source_texture_suffix(marker, value);
}
std::optional<std::size_t> ModelResources::resolve(std::uint32_t id) const
{
    if (id == 0)
        return {};
    if (id > models_.size())
        throw std::out_of_range("unknown model resource identity");
    return std::size_t(id - 1);
}
std::uint32_t ModelResources::identity(std::optional<std::size_t> handle) const
{
    if (!handle)
        return 0;
    if (*handle >= models_.size())
        throw std::out_of_range("unknown model resource handle");
    return static_cast<std::uint32_t>(*handle + 1);
}
ModelResource &ModelResources::at(std::size_t handle)
{
    auto *model = std::get_if<ModelResource>(&models_.at(handle));
    if (!model)
        throw FormatError("source-bank model requires checked projections; legacy borrowed spans are not admitted");
    return *model;
}
namespace
{
template <class Load>
LevelModelHandles bind_models(ModelResources &resources, std::span<const LevelAssetRequest> requests,
                              ModelBindingSlots &slots, std::span<const SelectionRecord, 6> selection,
                              std::span<LevelCarSlot, 6> cars, Load &&load)
{
    auto pending = slots;
    for (const auto &request : requests)
    {
        if (request.type < 2 || request.type > 15)
            throw std::out_of_range("unsupported level model binding slot");
        pending[request.type] = resources.resolve(load(request.name));
    }
    // 42F090 assigns compact request pairs to the actual physical selection
    // slots with kind==2. Other kinds do not consume a pair and retain fields.
    auto planned = std::array<LevelCarSlot, 6>{};
    std::copy(cars.begin(), cars.end(), planned.begin());
    unsigned compact = 0;
    for (unsigned physical = 0; physical < 6; ++physical)
        if (selection[physical].kind == 2)
        {
            planned[physical].source_word00 = resources.identity(pending[4 + compact * 2]);
            planned[physical].source_word04 = resources.identity(pending[5 + compact * 2]);
            ++compact;
        }
    slots = pending;
    std::copy(planned.begin(), planned.end(), cars.begin());
    return {slots[2], slots[3]};
}
} // namespace
LevelModelHandles bind_level_model_resources(ModelResources &resources, std::span<const LevelAssetRequest> requests,
                                             ModelBindingSlots &slots, std::span<const SelectionRecord, 6> selection,
                                             std::span<LevelCarSlot, 6> cars, std::string_view track_set)
{
    return bind_models(resources, requests, slots, selection, cars,
                       [&](const std::string &name) { return resources.load(name, track_set); });
}
LevelModelHandles bind_source_level_model_resources(ModelResources &resources, SourceAllocationBank &bank,
                                                    std::span<const LevelAssetRequest> requests,
                                                    ModelBindingSlots &slots,
                                                    std::span<const SelectionRecord, 6> selection,
                                                    std::span<LevelCarSlot, 6> cars, SourcePmdObserver *observer)
{
    return bind_models(resources, requests, slots, selection, cars,
                       [&](const std::string &name) { return resources.load_source(bank, name, observer); });
}
void remap_level_vehicle_textures(ModelResources &resources, std::span<const LevelCarSlot, 6> cars,
                                  std::span<const std::int8_t, 6> random_liveries)
{
    for (unsigned physical = 0; physical < cars.size(); ++physical)
    {
        const auto &car = cars[physical];
        if (!car.source_word00)
            continue;
        const auto handle = *resources.resolve(car.source_word00);
        const auto skin = car.settings.livery ? std::bit_cast<std::int32_t>(car.settings.livery)
                                              : std::int32_t(random_liveries[physical]);
        resources.replace_texture_suffix(handle, "grid", skin);
    }
    // The tyre pass follows all six livery slots, including when several slots
    // reference the same model. Do not interleave the two original traversals.
    for (const auto &car : cars)
    {
        if (!car.source_word00 || !car.settings.tyre)
            continue;
        resources.replace_texture_suffix(*resources.resolve(car.source_word00), "tyre",
                                         std::bit_cast<std::int32_t>(car.settings.tyre));
    }
}
} // namespace sgn::retail
