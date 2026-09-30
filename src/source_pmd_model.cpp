#include "sgn/source_pmd_model.hpp"
#include "sgn/format_require.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <fstream>
#include <limits>

namespace sgn::retail
{
namespace
{
using sgn::format_check::require;
void extent(std::size_t size, std::uint64_t offset, std::uint64_t count)
{
    require(offset <= size && count <= size - offset, "PMD source byte extent");
}
std::uint32_t word(std::span<const std::uint8_t> bytes, std::uint32_t offset)
{
    extent(bytes.size(), offset, 4);
    std::uint32_t value;
    std::memcpy(&value, bytes.data() + offset, 4);
    return value;
}
void put(std::span<std::uint8_t> bytes, std::uint32_t offset, std::uint32_t value)
{
    extent(bytes.size(), offset, 4);
    std::memcpy(bytes.data() + offset, &value, 4);
}
std::uint32_t add(std::uint32_t a, std::uint64_t b)
{
    require(b <= std::numeric_limits<std::uint32_t>::max() - a, "PMD normalized source offset overflow");
    return a + static_cast<std::uint32_t>(b);
}
void valid_name(std::string_view name)
{
    require(!name.empty() && name.size() < 64, "PMD request name extent");
    for (unsigned char c : name)
        require(c >= 32 && c < 127 && c != '/' && c != '\\' && c != ':' && c != '.', "PMD request name spelling");
}
std::uint32_t stream_size(std::ifstream &stream)
{
    const auto end = stream.tellg();
    require(end >= 0 && end <= std::numeric_limits<std::int32_t>::max(), "PMD loose file source size");
    return static_cast<std::uint32_t>(end);
}
std::uint32_t u32(const SourcePmdByteView &view, std::uint32_t at)
{
    return view.field<std::uint32_t>(at).value();
}
SourcePmdByteView section_of(const SourcePmdByteView &file, std::uint32_t header)
{
    return file.subview(add(0x148, u32(file, header)), u32(file, header + 0x94));
}
SourcePmdByteView within_section(const SourcePmdByteView &file, std::uint32_t at, std::uint32_t count,
                                 std::uint32_t stride, std::uint32_t header)
{
    const auto section = section_of(file, header);
    const auto start = section.location().offset;
    require(at >= start && (at - start) % stride == 0, "PMD relocated pointer has wrong section/alignment");
    const auto bytes = std::uint64_t(count) * stride;
    require(bytes <= std::numeric_limits<std::uint32_t>::max(), "PMD projected span overflow");
    return section.subview(at - start, static_cast<std::uint32_t>(bytes));
}
} // namespace
namespace detail
{
void throw_expired_pmd()
{
    throw SourceAllocationError(0x42efb0, "expired PMD model metadata");
}
void throw_pmd_format(const char *reason)
{
    throw FormatError(reason);
}
std::shared_ptr<SourcePmdAccess> checked_pmd_extent(const std::weak_ptr<SourcePmdAccess> &reference,
                                                    std::uint32_t offset, std::uint32_t length)
{
    const auto owner = reference.lock();
    if (!owner || !owner->bank)
        throw SourceAllocationError(0x42efb0, "expired PMD model metadata");
    owner->bank->check_extent(owner->allocation, offset, length, 0x42efb0);
    return owner;
}
std::span<std::uint8_t> checked_pmd_bytes(const std::weak_ptr<SourcePmdAccess> &reference, std::uint32_t offset,
                                          std::uint32_t length, std::uint32_t site)
{
    const auto owner = checked_pmd_extent(reference, offset, length);
    return owner->bank->readable_bytes(owner->allocation, offset, length, site);
}
void write_pmd_known(const std::weak_ptr<SourcePmdAccess> &reference, std::uint32_t offset,
                     std::span<const std::uint8_t> value)
{
    const auto owner = checked_pmd_extent(reference, offset, static_cast<std::uint32_t>(value.size()));
    owner->bank->write_known(owner->allocation, offset, value, 0x42efb0);
}
} // namespace detail
SourcePmdByteView::SourcePmdByteView(std::weak_ptr<detail::SourcePmdAccess> owner, std::uint32_t offset,
                                     std::uint32_t length)
    : owner_(std::move(owner)), offset_(offset), length_(length)
{
    access_ = owner_.lock().get();
    (void)size();
}
SourcePmdByteView::SourcePmdByteView(const std::shared_ptr<detail::SourcePmdAccess> &owner, std::uint32_t offset,
                                     std::uint32_t length)
    : owner_(owner), offset_(offset), length_(length), access_(owner.get())
{
    (void)size();
}
// size/location/subview/load_transport_word/store_transport_word are inline
// in the header (NEW R19): one admission per access instead of repeated
// weak_ptr locks and bank lookups for the same already-checked range.
SourcePmdByteView SourcePmdByteView::relocated_part(std::uint32_t at) const
{
    (void)size();
    const auto *access = access_; // non-null and alive: size() admitted it
    require(at >= offset_, "PMD part before checked file view");
    for (const auto range : access->relocated_parts)
    {
        if (at >= range.offset && (at - range.offset) % 64 == 0 && std::uint64_t(at - range.offset) + 64 <= range.bytes)
            return subview(at - offset_, 64); // actual requested allocation, never reserved padding
    }
    throw FormatError("PMD part was not relocated in this allocation");
}
SourcePmdPartView::SourcePmdPartView(SourcePmdByteView file, SourcePmdByteView record)
    : file_(std::move(file)), record_(std::move(record))
{
}
SourcePmdByteView SourcePmdPartView::pointed_section(std::uint32_t pointer, std::uint32_t count, std::uint32_t stride,
                                                     std::uint32_t header) const
{
    return within_section(file_, u32(record_, pointer), count, stride, header);
}
SourcePmdField<std::uint16_t> SourcePmdPartView::bound_index(unsigned lane) const
{
    require(lane < 2, "PMD part bound lane");
    return record_.field<std::uint16_t>(0x20 + lane * 2);
}
SourcePmdField<SourcePmdTransform> SourcePmdPartView::transform() const
{
    return pointed_section(8, 1, 80, 0x44).field<SourcePmdTransform>(0);
}
SourcePmdArrayView<SourcePmdTransform> SourcePmdPartView::transform_run(std::uint32_t count) const
{
    return SourcePmdArrayView<SourcePmdTransform>(pointed_section(8, count, 80, 0x44));
}
SourcePmdArrayView<VehicleVector> SourcePmdPartView::vertices() const
{
    return SourcePmdArrayView<VehicleVector>(pointed_section(0x30, u32(record_, 0x28), 16, 0x2c));
}
SourcePmdField<std::uint8_t> SourcePmdPartView::attribute_byte(std::uint32_t index, unsigned lane) const
{
    require(lane < 8 && index < attribute_count().value(), "PMD attribute byte index/lane");
    return pointed_section(0x34, attribute_count().value(), 8, 0x24).field<std::uint8_t>(index * 8 + lane);
}
SourcePmdArrayView<std::array<std::uint8_t, 8>> SourcePmdPartView::attributes() const
{
    return SourcePmdArrayView<std::array<std::uint8_t, 8>>(
        pointed_section(0x34, record_.field<std::uint16_t>(0x24).value(), 8, 0x24));
}
SourcePmdArrayView<std::array<std::uint32_t, 4>> SourcePmdPartView::coordinates() const
{
    return SourcePmdArrayView<std::array<std::uint32_t, 4>>(
        pointed_section(0x2c, record_.field<std::uint16_t>(0x26).value(), 16, 0x28));
}
float SourcePmdPartView::maximum_vertex_z() const
{
    const auto all = vertices();
    const auto count = record_.field<std::uint16_t>(0x28).value();
    float result = 0;
    require(count <= all.size(), "PMD low-word vertex count exceeds owner");
    for (std::uint32_t i = 0; i < count; ++i)
    {
        const auto z = all.at(i).value().z;
        require(std::isfinite(z), "nonfinite source PMD vertex");
        if (result < z)
            result = z;
    }
    return result;
}
SourcePmdModelView::SourcePmdModelView(SourcePmdByteView file) : file_(std::move(file)) {}
SourcePmdByteView SourcePmdModelView::section(std::uint32_t header) const
{
    return section_of(file_, header);
}
SourcePmdByteView SourcePmdModelView::descriptor_table(std::uint32_t member, std::uint32_t header,
                                                       std::uint32_t stride) const
{
    const auto size = section(header).size();
    require(size % stride == 0, "PMD descriptor table stride");
    return within_section(file_, u32(descriptor(), member), size / stride, stride, header);
}
SourcePmdByteView SourcePmdModelView::descriptor() const
{
    return file_.subview(add(0x148, u32(file_, 0x4c)), 0x68);
}
SourcePmdField<std::uint8_t> SourcePmdModelView::first_polygon_attribute_flags() const
{
    return within_section(file_, u32(descriptor(), 0x2c), 1, 8, 0x24).field<std::uint8_t>(3);
}
unsigned SourcePmdModelView::source_lod_index(unsigned slot) const
{
    require(slot < 4, "PMD source LOD slot");
    const auto d = descriptor();
    const auto last = u32(d, 0x58);
    require(last < 4, "PMD current maximum LOD exceeds four source slots");
    const auto target = u32(d, 0x48 + slot * 4);
    for (unsigned i = 0; i <= last; ++i)
        if (u32(d, 0x48 + i * 4) == target)
            return i;
    throw FormatError("PMD source LOD points to an unrelocated part range");
}
SourcePmdPartView SourcePmdModelView::part(unsigned lod, unsigned index) const
{
    const auto d = descriptor();
    const auto last = u32(d, 0x58);
    require(last < 4 && lod <= last, "PMD checked part LOD");
    const auto at = add(u32(d, 0x48 + lod * 4), std::uint64_t(index) * 64);
    // Logical descriptor24 may change after relocation (engine count17/part17).
    // Current raw pointers still select the record; immutable history owns it.
    return {file_, file_.relocated_part(at)};
}
SourcePmdArrayView<VehicleVector> SourcePmdModelView::normals() const
{
    return SourcePmdArrayView<VehicleVector>(descriptor_table(0x28, 0x20, 16));
}
SourcePmdArrayView<std::uint16_t> SourcePmdModelView::corner_indices() const
{
    return SourcePmdArrayView<std::uint16_t>(descriptor_table(0x38, 0x30, 2));
}
SourcePmdField<VehicleVector> SourcePmdModelView::bound_vertex(std::uint32_t index) const
{
    const SourcePmdArrayView<std::uint32_t> table(descriptor_table(0x40, 0x40, 4));
    return within_section(file_, table.at(index).value(), 1, 16, 0x3c).field<VehicleVector>(0);
}
std::uint32_t SourcePmdModelView::texture_count() const
{
    const SourcePmdArrayView<std::uint32_t> names(descriptor_table(0x60, 0x38, 4));
    for (std::uint32_t i = 0; i < names.size(); ++i)
        if (names.at(i).value() == 0)
            return i;
    if (names.size() == 0)
        return 0;
    throw FormatError("PMD texture name table has no terminator");
}
std::string SourcePmdModelView::texture_name(std::uint32_t index) const
{
    require(index < texture_count(), "PMD texture index");
    const auto names = descriptor_table(0x60, 0x38, 4), strings = section(0x34);
    const auto at = u32(names, index * 4), start = strings.location().offset;
    require(at >= start && at - start < strings.size(), "PMD relocated texture name outside strings");
    std::string name;
    for (std::uint32_t i = at - start; i < strings.size(); ++i)
    {
        const auto c = strings.field<std::uint8_t>(i).value();
        if (!c)
            return name;
        name.push_back(static_cast<char>(c));
    }
    throw FormatError("PMD texture string has no terminator");
}
void SourcePmdModelView::replace_texture_suffix(std::string_view marker, std::int32_t value) const
{
    require(!marker.empty() && marker.find('\0') == marker.npos, "PMD texture marker");
    const auto suffix = std::to_string(value);
    const auto count = texture_count();
    const auto names = descriptor_table(0x60, 0x38, 4), strings = section(0x34);
    const auto start = strings.location().offset, end = add(start, strings.size());
    for (std::uint32_t i = 0; i < count; ++i)
    {
        const auto name = texture_name(i);
        const auto match = name.find(marker);
        if (match == name.npos)
            continue;
        const auto at = u32(names, i * 4);
        auto limit = end;
        for (std::uint32_t j = 0; j < count; ++j)
        {
            const auto next = u32(names, j * 4);
            if (next > at)
                limit = std::min(limit, next);
        }
        const auto write_at = add(at, match + marker.size());
        require(write_at < limit && suffix.size() < limit - write_at,
                "PMD texture suffix would overwrite another source string");
        for (std::uint32_t j = 0; j < suffix.size(); ++j)
            file_.field<std::uint8_t>(write_at + j).set(static_cast<std::uint8_t>(suffix[j]));
        file_.field<std::uint8_t>(write_at + static_cast<std::uint32_t>(suffix.size())).set(0);
        // Like source sprintf, retain every byte after the new terminator.
    }
}
SourcePmdTrackView SourcePmdModelView::track() const
{
    require(kind() == 1, "PMD track projection requires kind1");
    return SourcePmdTrackView(*this);
}
SourcePmdByteView SourcePmdTrackView::section(std::uint32_t header, std::uint32_t stride) const
{
    require(model_.kind() == 1, "PMD current model is no longer kind1");
    const auto bytes = section_of(model_.bytes(), header);
    require(bytes.size() % stride == 0, "PMD checked track section stride");
    return bytes;
}
SourcePmdByteView SourcePmdTrackView::spatial_nodes() const
{
    return section(0x88, 64);
}
SourcePmdArrayView<VehicleVector> SourcePmdTrackView::spatial_bounds() const
{
    return SourcePmdArrayView<VehicleVector>(section(0x7c, 16));
}
SourcePmdArrayView<std::uint16_t> SourcePmdTrackView::spatial_vertex_indices() const
{
    return SourcePmdArrayView<std::uint16_t>(section(0x80, 2));
}
SourcePmdArrayView<std::uint16_t> SourcePmdTrackView::spatial_polygon_indices() const
{
    return SourcePmdArrayView<std::uint16_t>(section(0x84, 2));
}
SourcePmdByteView SourcePmdTrackView::collision_descriptor() const
{
    const auto bytes = section(0xa0, 1);
    require(bytes.size() >= 40, "PMD track collision descriptor extent");
    return bytes;
}
SourcePmdByteView SourcePmdTrackView::collision_tail(std::uint32_t member, std::uint32_t header,
                                                     std::uint32_t stride) const
{
    const auto bytes = section(header, stride);
    const auto start = bytes.location().offset, at = u32(collision_descriptor(), member);
    require(at >= start && (at - start) % stride == 0, "PMD current collision pointer alignment/section");
    require(at - start <= bytes.size(), "PMD current collision pointer exceeds section");
    return bytes.subview(at - start, bytes.size() - (at - start));
}
SourcePmdArrayView<std::uint16_t> SourcePmdTrackView::collision_polygon_lists() const
{
    return SourcePmdArrayView<std::uint16_t>(collision_tail(0, 0x90, 2));
}
SourcePmdArrayView<std::uint16_t> SourcePmdTrackView::collision_indices() const
{
    return SourcePmdArrayView<std::uint16_t>(collision_tail(4, 0x8c, 2));
}
SourcePmdArrayView<VehicleVector> SourcePmdTrackView::collision_vertices() const
{
    return SourcePmdArrayView<VehicleVector>(collision_tail(8, 0x2c, 16));
}
SourcePmdArrayView<VehicleVector> SourcePmdTrackView::collision_normals() const
{
    return SourcePmdArrayView<VehicleVector>(collision_tail(12, 0x20, 16));
}
SourcePmdByteView SourcePmdTrackView::collision_polygons() const
{
    return collision_tail(16, 0x98, 8);
}
SourcePmdByteView SourcePmdTrackView::collision_materials() const
{
    return collision_tail(20, 0x94, 4);
}
SourcePmdByteView SourcePmdTrackView::collision_tree(unsigned lane) const
{
    require(lane < 2, "PMD collision tree lane");
    return collision_tail(0x1c + 8 * lane, 0x9c, 48);
}
SourcePmdField<float> SourcePmdTrackView::collision_tree_radius(unsigned lane) const
{
    require(lane < 2, "PMD collision radius lane");
    return collision_descriptor().field<float>(0x18 + 8 * lane);
}
std::uint32_t SourcePmdTrackView::spline_count() const
{
    return section(0x78, 1).size() / 28; // source does not require size divisible by28
}
SourcePmdByteView SourcePmdTrackView::spline_descriptor(std::uint32_t index) const
{
    require(index < spline_count(), "PMD checked spline descriptor index");
    return section(0x78, 1).subview(index * 28, 28);
}
SourcePmdByteView SourcePmdTrackView::spline_point_storage() const
{
    return section(0x74, 16);
}
SourcePmdArrayView<VehicleVector> SourcePmdTrackView::spline_points(std::uint32_t index) const
{
    const auto d = spline_descriptor(index);
    return SourcePmdArrayView<VehicleVector>(within_section(model_.bytes(), u32(d, 4), u32(d, 8), 16, 0x74));
}
std::uint32_t SourcePmdTrackView::light_count() const
{
    model_.check();
    return u32(model_.bytes(), 0xfc);
}
std::uint32_t SourcePmdTrackView::render_light_count() const
{
    model_.check();
    return u32(model_.bytes(), 0x100);
}
SourcePmdByteView SourcePmdTrackView::light_records() const
{
    const auto bytes = section(0x70, 144);
    return bytes.subview(0, add(0, std::uint64_t(light_count()) * 144));
}
SourcePmdByteView SourcePmdTrackView::ambient() const
{
    return section(0x64, 1);
}
SourcePmdLocation SourcePmdTrackView::opaque58_location() const
{
    const auto file = model_.bytes();
    return file.subview(add(0x148, u32(file, 0x58)), 0).location();
}
std::uint32_t SourcePmdTrackView::opaque58_count() const
{
    return u32(model_.bytes(), 0xe8);
}
SourcePmdByteView SourcePmdTrackView::texture_motion_bytes() const
{
    return section(0x5c, 1);
}
SourcePmdByteView SourcePmdTrackView::texture_index_bytes() const
{
    return section(0x60, 1);
}
SourcePmdArrayView<VehicleVector> SourcePmdTrackView::generated_normals() const
{
    const auto file = model_.bytes(), normal = section(0x20, 16);
    const auto first = u32(file, 0xe4), at = u32(model_.descriptor(), 0x28);
    const auto tail = within_section(file, at, normal.size() / 16, 16, 0x20);
    const auto skip = add(0, std::uint64_t(first) * 16);
    require(skip <= tail.size(), "PMD generated normal start exceeds storage");
    return SourcePmdArrayView<VehicleVector>(tail.subview(skip, tail.size() - skip));
}
SourcePmdModel::SourcePmdModel(std::shared_ptr<detail::SourcePmdAccess> access, std::string name)
    : access_(std::move(access)), source_name_(std::move(name))
{
}
SourcePmdModelView SourcePmdModel::view() const
{
    require(bool(access_) && access_->bank, "PMD model metadata unavailable");
    const auto size = access_->bank->allocation_size(access_->allocation);
    return SourcePmdModelView(SourcePmdByteView(access_, 0, size));
}
std::filesystem::path SourcePmdFileSystem::path(std::string_view name) const
{
    valid_name(name);
    return root_ / "meshdata" / (std::string(name) + ".PMD");
}
std::uint32_t SourcePmdFileSystem::size(std::string_view name)
{
    std::ifstream stream(path(name), std::ios::binary | std::ios::ate);
    return stream ? stream_size(stream) : 0;
}
SourcePmdFileRead SourcePmdFileSystem::read(std::string_view name, std::span<std::uint8_t> destination)
{
    std::ifstream stream(path(name), std::ios::binary | std::ios::ate);
    if (!stream)
        return {};
    const auto total = stream_size(stream);
    require(!destination.empty() || total == 0,
            "PMD zero loose read would exceed the actual allocation through source2MiB fallback");
    const auto count = static_cast<std::uint32_t>(std::min<std::size_t>(total, destination.size()));
    stream.seekg(0);
    if (count && !stream.read(reinterpret_cast<char *>(destination.data()), count))
        throw FormatError("PMD actual loose read failed");
    return {count, count}; // 44AFCC/44AFDC return actual fread count, not total length
}
SourcePmdModel load_source_pmd_42efb0(SourceAllocationBank &bank, SourcePmdLoadingState &state,
                                      SourcePmdFileSource &files, std::string_view name, SourcePmdObserver *observer)
{
    require(bank.initialized(), "source PMD requires explicit prior42EAB0");
    valid_name(name);
    auto access = std::make_shared<detail::SourcePmdAccess>();
    access->bank = &bank;
    std::string provenance(name); // metadata allocation before the actual source request
    const auto size = files.size(name);
    if (size > 64 * 1024 * 1024)
        throw SourceAllocationError(0x42efe0, "PMD input exceeds bounded file domain");
    access->allocation = bank.allocate_42e8d0(size, 0);
    auto observe = [&](SourcePmdEventKind kind, std::uint32_t site) {
        if (observer)
            observer->observe({kind, site, access->allocation, size}, bank, state);
    };
    observe(SourcePmdEventKind::allocation, 0x42efe0);
    if (!access->allocation)
        throw SourceAllocationError(0x42efed, "PMD null payload is outside admitted read domain");
    auto bytes = bank.bytes(access->allocation);
    const auto read = files.read(name, bytes);
    observe(SourcePmdEventKind::read, 0x42efed);
    state.file = access->allocation;
    state.file_size = size;
    state.version = 0;
    observe(SourcePmdEventKind::globals, 0x42f006);
    if (read.copied_bytes != size)
        throw SourceAllocationError(0x42f010, "PMD incomplete read outside complete-file domain");
    constexpr std::string_view version = "PMD V1.83";
    bool recognized = bytes.size() > version.size() && bytes[version.size()] == 0;
    for (std::size_t i = 0; recognized && i < version.size(); ++i)
    {
        auto c = bytes[i];
        if (c >= 'a' && c <= 'z')
            c = static_cast<std::uint8_t>(c - 32);
        recognized = c == static_cast<std::uint8_t>(version[i]);
    }
    if (!recognized)
        throw SourceAllocationError(0x42f038, "PMD source fatal: version differs from pinned V1.83");
    state.version = 0x12;
    if (bytes.size() < 0x148)
        throw SourceAllocationError(0x42ebf7, "PMD header outside actual source block");
    const auto descriptor = add(0x148, word(bytes, 0x4c));
    extent(bytes.size(), descriptor, 0x68);
    // Bounded complete-file validation uses only the actual bank bytes. No
    // decoded mesh/rig or retained texture/transform vector is constructed.
    constexpr std::array<std::uint32_t, 11> strides{16, 8, 16, 16, 2, 1, 4, 16, 4, 80, 64};
    std::array<std::pair<std::uint32_t, std::uint32_t>, 12> ranges{};
    for (unsigned i = 0; i < strides.size(); ++i)
    {
        const auto at = add(0x148, word(bytes, 0x20 + i * 4)), length = word(bytes, 0xb4 + i * 4);
        extent(bytes.size(), at, length);
        require(length % strides[i] == 0, "PMD source section stride");
        ranges[i] = {at, length};
    }
    ranges.back() = {descriptor, 0x68};
    for (unsigned i = 0; i < ranges.size(); ++i)
        for (unsigned j = i + 1; j < ranges.size(); ++j)
        {
            const auto [a, n] = ranges[i];
            const auto [b, m] = ranges[j];
            require(!n || !m || std::uint64_t(a) + n <= b || std::uint64_t(b) + m <= a, "PMD source sections overlap");
        }
    state.descriptor_file = access->allocation;
    state.descriptor_offset = descriptor; // 42EC0B
    constexpr std::array<std::pair<unsigned, unsigned>, 9> bindings{{{0x28, 0x20},
                                                                     {0x2c, 0x24},
                                                                     {0x30, 0x28},
                                                                     {0x34, 0x2c},
                                                                     {0x38, 0x30},
                                                                     {0x3c, 0x44},
                                                                     {0x40, 0x40},
                                                                     {0x44, 0x48},
                                                                     {0x60, 0x38}}};
    for (const auto [member, header] : bindings)
        put(bytes, descriptor + member, add(0x148, word(bytes, header)));
    for (unsigned i = 0; i < 4; ++i)
        put(bytes, descriptor + 0x48 + i * 4,
            add(word(bytes, descriptor + 0x44), std::uint64_t(word(bytes, descriptor + 0x48 + i * 4)) * 64));
    observe(SourcePmdEventKind::descriptor, 0x42f03d);
    const auto last = word(bytes, descriptor + 0x58), parts = word(bytes, descriptor + 0x24);
    require(last < 4 && parts > 0, "PMD maximum LOD/part count outside admitted domain");
    const auto part_start = ranges[10].first, part_bytes = ranges[10].second;
    for (unsigned lod = 0; lod <= last; ++lod)
    {
        const auto at = word(bytes, descriptor + 0x48 + lod * 4);
        require(at >= part_start && (at - part_start) % 64 == 0, "PMD relocated LOD pointer");
        extent(part_bytes, at - part_start, std::uint64_t(parts) * 64);
        for (unsigned prior = 0; prior < lod; ++prior)
        {
            const auto old = word(bytes, descriptor + 0x48 + prior * 4);
            require(std::uint64_t(at) + std::uint64_t(parts) * 64 <= old ||
                        std::uint64_t(old) + std::uint64_t(parts) * 64 <= at,
                    "PMD active LOD overlap would double-relocate source parts");
        }
        for (std::uint32_t p = 0; p < parts; ++p)
        {
            const auto record = at + p * 64;
            // Literal42ECA0 publication order: transform,coordinates,vertices,attributes.
            const auto transform = word(bytes, record + 8);
            extent(ranges[9].second, std::uint64_t(transform) * 80, 80);
            put(bytes, record + 8, add(ranges[9].first, std::uint64_t(transform) * 80));
            const auto coordinate = word(bytes, record + 0x2c), coordinate_count = word(bytes, record + 0x24) >> 16;
            extent(ranges[2].second, std::uint64_t(coordinate) * 16, std::uint64_t(coordinate_count) * 16);
            put(bytes, record + 0x2c, add(ranges[2].first, std::uint64_t(coordinate) * 16));
            const auto vertex = word(bytes, record + 0x30), vertex_count = word(bytes, record + 0x28);
            extent(ranges[3].second, std::uint64_t(vertex) * 16, std::uint64_t(vertex_count) * 16);
            put(bytes, record + 0x30, add(ranges[3].first, std::uint64_t(vertex) * 16));
            const auto attribute = word(bytes, record + 0x34), attribute_count = word(bytes, record + 0x24) & 0xffff;
            extent(ranges[1].second, std::uint64_t(attribute) * 8, std::uint64_t(attribute_count) * 8);
            put(bytes, record + 0x34, add(ranges[1].first, std::uint64_t(attribute) * 8));
        }
        // Only successfully relocated records become ready; no unused section
        // tail, mutable logical count or current raw alias can extend this range.
        access->relocated_parts[lod] = {at, static_cast<std::uint32_t>(std::uint64_t(parts) * 64)};
    }
    observe(SourcePmdEventKind::parts, 0x42f042);
    const auto names_at = ranges[6].first, names_bytes = ranges[6].second;
    bool terminated = names_bytes == 0;
    for (std::uint32_t i = 0; i < names_bytes; i += 4)
    {
        const auto relative = word(bytes, names_at + i);
        if (relative == 0)
        {
            terminated = true;
            break;
        }
        const auto at = add(0x148, relative);
        require(at >= ranges[5].first && at - ranges[5].first < ranges[5].second, "PMD source name outside strings");
        const auto end = add(ranges[5].first, ranges[5].second);
        require(std::find(bytes.begin() + at, bytes.begin() + end, std::uint8_t{}) != bytes.begin() + end,
                "PMD source name missing terminator");
        put(bytes, names_at + i, at);
    }
    require(terminated, "PMD source name table missing terminator");
    observe(SourcePmdEventKind::names, 0x42f047);
    for (std::uint32_t i = 0; i < ranges[8].second; i += 4)
    {
        const auto index = word(bytes, ranges[8].first + i);
        extent(ranges[7].second, std::uint64_t(index) * 16, 16);
        put(bytes, ranges[8].first + i, add(ranges[7].first, std::uint64_t(index) * 16));
    }
    observe(SourcePmdEventKind::bounds, 0x42f04c);
    if (word(bytes, 0x18) == 1)
    {
        // Host guards bound the admitted complete-file domain. No recovered
        // failure behavior is claimed for corrupt offsets or overlapping writes.
        // Each guard is at the reached child, so earlier source stores survive.
        auto pointer = [&](std::uint32_t header) {
            const auto at = add(0x148, word(bytes, header));
            extent(bytes.size(), at, 0);
            return at;
        };
        auto table = [&](std::uint32_t header, std::uint32_t stride) {
            const auto at = pointer(header), length = word(bytes, header + 0x94);
            extent(bytes.size(), at, length);
            require(length % stride == 0, "PMD source track section stride");
            return std::pair{at, length};
        };
        auto binding = [&](std::uint32_t header, std::uint32_t stride) {
            return SourcePmdLocation{access->allocation, table(header, stride).first};
        };
        auto &track = state.track;
        // 42ED30..42ED7E: exact four-global store order, no payload mutation.
        track.spatial_nodes = binding(0x88, 64);          // 42ED45 ->624470
        track.spatial_bounds = binding(0x7c, 16);         // 42ED58 ->62446C
        track.spatial_polygon_indices = binding(0x84, 2); // 42ED6B ->624468
        track.spatial_vertex_indices = binding(0x80, 2);  // 42ED78 ->624464
        observe(SourcePmdEventKind::track_spatial, 0x42f05d);
        bytes = bank.bytes(access->allocation);
        // 42ED80..42EE45: descriptor itself stays in the actual FILESIZE block.
        const auto [collision, collision_bytes] = table(0xa0, 1);
        extent(collision_bytes, 0, 40);
        (void)word(bytes, collision);                      // source42ED93 reads before global publication
        track.collision = {access->allocation, collision}; // 42EDA1 ->624480
        struct Relocation
        {
            std::uint32_t member, header, stride;
        };
        constexpr std::array<Relocation, 8> relocation{{{0, 0x90, 2},
                                                        {4, 0x8c, 2},
                                                        {8, 0x2c, 16},
                                                        {12, 0x20, 16},
                                                        {16, 0x98, 8},
                                                        {20, 0x94, 4},
                                                        {28, 0x9c, 48},
                                                        {36, 0x9c, 48}}};
        for (const auto item : relocation)
        {
            const auto [start, length] = table(item.header, item.stride);
            const auto relative = std::uint64_t(word(bytes, collision + item.member)) * item.stride;
            extent(length, relative, 0);
            put(bytes, collision + item.member, add(start, relative)); // 42EDB6,C7,DC,F4/42EE09,1D,3E,3E
        }
        observe(SourcePmdEventKind::track_collision, 0x42f062);
        bytes = bank.bytes(access->allocation);
        // 42EED0 tail42EE50: unsigned floor(header10C/28), not rounded up.
        const auto [splines, spline_bytes] = table(0x78, 1);
        const auto spline_count = spline_bytes / 28;
        if (spline_count)
        {
            const auto [points, point_bytes] = table(0x74, 16);
            for (std::uint32_t i = 0; i < spline_count; ++i)
            {
                const auto at = splines + i * 28;
                const auto relative = std::uint64_t(word(bytes, at + 4)) * 16;
                extent(point_bytes, relative, 0);
                put(bytes, at + 4, add(points, relative)); // 42EE9C; authored point_count is not rewritten
            }
        }
        track.splines = {access->allocation, splines}; // 42EEA5 or42EEB7
        track.spline_count = spline_count;             // 42EEAC or42EEBE
        observe(SourcePmdEventKind::track_splines, 0x42f067);
        bytes = bank.bytes(access->allocation);
        // 42EEE0..42EF1D: exact scalar/scalar/pointer/pointer publication.
        track.light_count = word(bytes, 0xfc);         // 42EEF1 ->62449C
        track.render_light_count = word(bytes, 0x100); // 42EEFA ->6244A0
        track.lights = binding(0x70, 144);             // 42EF0A ->62447C
        track.ambient = binding(0x64, 1);              // 42EF17 ->624494
        observe(SourcePmdEventKind::track_lighting, 0x42f06c);
        bytes = bank.bytes(access->allocation);
        track.opaque58_count = word(bytes, 0xe8);             // 42EF2E ->6244A4
        track.opaque58 = {access->allocation, pointer(0x58)}; // 42EF3B ->624478; shape intentionally opaque
        observe(SourcePmdEventKind::track_opaque58, 0x42f071);
        bytes = bank.bytes(access->allocation);
        track.texture_indices = binding(0x60, 1); // 42EF62 ->624484
        track.texture_motion = binding(0x5c, 1);  // 42EF6F ->624488
        observe(SourcePmdEventKind::track_textures, 0x42f076);
        bytes = bank.bytes(access->allocation);
        const auto normal = word(bytes, descriptor + 0x28), first_generated = word(bytes, 0xe4);
        const auto generated = add(normal, std::uint64_t(first_generated) * 16);
        const auto [normal_start, normal_bytes] = table(0x20, 16);
        require(normal == normal_start && generated - normal <= normal_bytes, "PMD generated normal region extent");
        track.normals = {access->allocation, normal};              // 42EF99 ->62448C
        track.generated_normals = {access->allocation, generated}; // 42EF9E ->624490
        // 576CC8 is neither initialized nor advanced by42EF80.
        observe(SourcePmdEventKind::track_normals, 0x42f07b);
        bytes = bank.bytes(access->allocation);
    }
    require(word(bytes, 0x18) <= 2, "PMD kind outside qualified file corpus");
    observe(SourcePmdEventKind::complete, 0x42f080);
    return SourcePmdModel(std::move(access), std::move(provenance));
}
} // namespace sgn::retail
