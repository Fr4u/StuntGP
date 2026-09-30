#pragma once
#include "sgn/source_allocation_bank.hpp"
#include "sgn/source_pmd_requests.hpp"
#include "sgn/source_scene_static_pointer.hpp"
#include <variant>
#include "sgn/retail_vehicle_anchors.hpp"
#include <cstring>
#include <memory>
#include <type_traits>
#include <utility>

namespace sgn::retail
{
// Pointer-free80-byte source value. A returned value is a snapshot; writes
// use a checked field and update the same bank allocation immediately.
struct SourcePmdTransform
{
    VehicleVector position, scale, forward, up;
    std::array<std::uint32_t, 4> tail;
};
static_assert(sizeof(SourcePmdTransform) == 80 && std::is_trivially_copyable_v<SourcePmdTransform>);
namespace detail
{
struct SourcePmdMountedAccess; // private raw0 camera projection
struct SourcePmdLaunchAccess;  // private captured-head projection; no public access method
struct SourcePmdPartRange
{
    std::uint32_t offset{}, bytes{};
};
struct SourcePmdAccess
{
    SourceAllocationBank *bank{}; // bank must outlive the owning ModelResources
    SourceAllocationRef allocation;
    // Host validity history: actual42ECA0-relocated records, not mutable logical24.
    // Four small ranges share this allocation; no part/geometry payload is copied.
    std::array<SourcePmdPartRange, 4> relocated_parts{};
};
// Lock metadata first, so a view outliving its model rejects before touching
// its borrowed bank. Payload release is checked by the actual bank generation.
std::span<std::uint8_t> checked_pmd_bytes(const std::weak_ptr<SourcePmdAccess> &, std::uint32_t offset,
                                          std::uint32_t length, std::uint32_t site = 0x42efb0);
std::shared_ptr<SourcePmdAccess> checked_pmd_extent(const std::weak_ptr<SourcePmdAccess> &, std::uint32_t offset,
                                                    std::uint32_t length);
void write_pmd_known(const std::weak_ptr<SourcePmdAccess> &, std::uint32_t offset, std::span<const std::uint8_t>);
// NEW R19 (perf_transport) inline admission for the view hot path. Same checks,
// order and diagnoses as checked_pmd_extent, without a weak_ptr lock: the
// owner-thread view keeps the raw metadata pointer it was created from and
// dereferences it only while its weak_ptr reports the metadata alive.
[[noreturn]] void throw_expired_pmd();
[[noreturn]] void throw_pmd_format(const char *);
inline SourcePmdAccess &live_pmd(const std::weak_ptr<SourcePmdAccess> &reference, SourcePmdAccess *raw)
{
    if (!raw || reference.expired() || !raw->bank)
        throw_expired_pmd();
    return *raw;
}
inline SourcePmdAccess &checked_pmd_range(const std::weak_ptr<SourcePmdAccess> &reference, SourcePmdAccess *raw,
                                          std::uint32_t offset, std::uint32_t length)
{
    auto &owner = live_pmd(reference, raw);
    owner.bank->check_extent(owner.allocation, offset, length, 0x42efb0);
    return owner;
}
inline void pmd_extent(std::size_t size, std::uint64_t offset, std::uint64_t count)
{
    if (!(offset <= size && count <= size - offset))
        throw_pmd_format("PMD source byte extent");
}
inline std::uint32_t pmd_add(std::uint32_t a, std::uint64_t b)
{
    if (b > 0xffffffffull - a)
        throw_pmd_format("PMD normalized source offset overflow");
    return a + static_cast<std::uint32_t>(b);
}
} // namespace detail
template <class T>
concept SourcePmdValue =
    std::is_same_v<T, std::uint8_t> || std::is_same_v<T, std::uint16_t> || std::is_same_v<T, std::uint32_t> ||
    std::is_same_v<T, float> || std::is_same_v<T, VehicleVector> || std::is_same_v<T, SourcePmdTransform> ||
    std::is_same_v<T, std::array<std::uint8_t, 8>> || std::is_same_v<T, std::array<std::uint32_t, 4>>;
struct SourcePmdLocation
{
    SourceAllocationRef allocation;
    std::uint32_t offset{};
    bool operator==(const SourcePmdLocation &) const = default;
};
class SourcePmdByteView;
class SourcePmdTrackView;
class SourcePmdSpline;
template <SourcePmdValue T> class SourcePmdField
{
    std::weak_ptr<detail::SourcePmdAccess> owner_;
    std::uint32_t offset_{};
    detail::SourcePmdAccess *access_{}; // NEW R19 host pointer, dereferenced only while owner_ is alive
    SourcePmdField(std::weak_ptr<detail::SourcePmdAccess> owner, detail::SourcePmdAccess *access, std::uint32_t offset)
        : owner_(std::move(owner)), offset_(offset), access_(access)
    {
    }
    friend class SourcePmdByteView;

  public:
    // Same sequence as checked_pmd_bytes: metadata lifetime, thread/generation
    // and extent at 0x42efb0, then the opaque-byte admission at site.
    T value(std::uint32_t site = 0x42efb0) const
    {
        auto &owner = detail::checked_pmd_range(owner_, access_, offset_, sizeof(T));
        const auto bytes = owner.bank->readable_bytes(owner.allocation, offset_, sizeof(T), site);
        T out;
        std::memcpy(&out, bytes.data(), sizeof(T));
        return out;
    }
    // Same sequence as write_pmd_known.
    void set(const T &value) const
    {
        auto &owner = detail::checked_pmd_range(owner_, access_, offset_, sizeof(T));
        owner.bank->write_known(owner.allocation, offset_, {reinterpret_cast<const std::uint8_t *>(&value), sizeof(T)},
                                0x42efb0);
    }
    SourcePmdLocation location() const
    {
        auto &owner = detail::checked_pmd_range(owner_, access_, offset_, sizeof(T));
        return {owner.allocation, offset_};
    }
};
// No raw pointer/span escapes. Every field read/write and size query validates
// the model metadata lifetime, actual bank generation, thread and byte extent.
class SourcePmdByteView
{
    std::weak_ptr<detail::SourcePmdAccess> owner_;
    std::uint32_t offset_{}, length_{};
    // NEW R19 host pointer to the same metadata as owner_ (captured once when
    // the root view is created). Dereferenced only after owner_.expired() is
    // false on the owner thread; never a lifetime or identity of its own.
    detail::SourcePmdAccess *access_{};
    SourcePmdByteView(std::weak_ptr<detail::SourcePmdAccess>, std::uint32_t, std::uint32_t);
    SourcePmdByteView(const std::shared_ptr<detail::SourcePmdAccess> &, std::uint32_t, std::uint32_t);
    // Subrange of an already admitted view: its own admission is implied by the
    // parent's (contained range, same metadata, same synchronous call).
    struct Contained
    {
    };
    SourcePmdByteView(Contained, const SourcePmdByteView &parent, std::uint32_t offset, std::uint32_t length)
        : owner_(parent.owner_), offset_(offset), length_(length), access_(parent.access_)
    {
    }
    SourcePmdByteView relocated_part(std::uint32_t absolute_offset) const;
    friend class SourcePmdModel;
    friend class SourcePmdModelView;
    friend class SourcePmdSpline; // private synchronous borrow; never public span
    friend struct detail::SourcePmdMountedAccess;
    friend struct detail::SourcePmdLaunchAccess; // reuses immutable relocated_part admission
  public:
    SourcePmdByteView() = default;
    std::uint32_t size() const
    {
        (void)detail::checked_pmd_range(owner_, access_, offset_, length_);
        return length_;
    }
    SourcePmdLocation location() const
    {
        (void)size();
        return {access_->allocation, offset_};
    }
    SourcePmdByteView subview(std::uint32_t offset, std::uint32_t length) const
    {
        detail::pmd_extent(size(), offset, length);
        return {Contained{}, *this, detail::pmd_add(offset_, offset), length};
    }
    SourceTransportWord load_transport_word(std::uint32_t offset, std::uint32_t site) const
    {
        // Same admission as the former subview(offset,4) + whole-view recheck.
        detail::pmd_extent(size(), offset, 4);
        const auto at = detail::pmd_add(offset_, offset);
        return access_->bank->load_transport_word(access_->allocation, at, site);
    }
    void store_transport_word(std::uint32_t offset, const SourceTransportWord &value, std::uint32_t site) const
    {
        detail::pmd_extent(size(), offset, 4);
        const auto at = detail::pmd_add(offset_, offset);
        access_->bank->store_transport_word(access_->allocation, at, value, site);
    }
    template <SourcePmdValue T> SourcePmdField<T> field(std::uint32_t offset) const
    {
        detail::pmd_extent(size(), offset, sizeof(T));
        return SourcePmdField<T>(owner_, access_, detail::pmd_add(offset_, offset));
    }
};
template <SourcePmdValue T> class SourcePmdArrayView
{
    SourcePmdByteView bytes_;

  public:
    explicit SourcePmdArrayView(SourcePmdByteView bytes) : bytes_(std::move(bytes))
    {
        if (bytes_.size() % sizeof(T))
            throw FormatError("PMD checked array stride");
    }
    std::uint32_t size() const
    {
        return bytes_.size() / static_cast<std::uint32_t>(sizeof(T));
    }
    SourcePmdField<T> at(std::uint32_t index) const
    {
        if (index >= size())
            throw FormatError("PMD checked array index");
        return bytes_.field<T>(index * static_cast<std::uint32_t>(sizeof(T)));
    }
    SourcePmdLocation location() const
    {
        return bytes_.location();
    }
};
class SourcePmdPartView
{
    SourcePmdByteView file_, record_;
    SourcePmdByteView pointed_section(std::uint32_t pointer, std::uint32_t count, std::uint32_t stride,
                                      std::uint32_t section_header) const;
    SourcePmdPartView(SourcePmdByteView, SourcePmdByteView);
    friend struct detail::SourcePmdMountedAccess;
    friend class SourcePmdModelView;

  public:
    SourcePmdField<std::uint16_t> flags() const
    {
        return record_.field<std::uint16_t>(0);
    }
    SourcePmdField<std::uint16_t> flags_02() const
    {
        return record_.field<std::uint16_t>(2);
    }
    SourcePmdField<std::uint16_t> word_04() const
    {
        return record_.field<std::uint16_t>(4);
    }
    SourcePmdField<VehicleVector> pivot() const
    {
        return record_.field<VehicleVector>(0x10);
    }
    SourcePmdField<std::uint16_t> bound_index(unsigned lane) const;
    SourcePmdField<SourcePmdTransform> transform() const;
    SourcePmdArrayView<SourcePmdTransform> transform_run(std::uint32_t count) const;
    SourcePmdArrayView<VehicleVector> vertices() const;
    SourcePmdField<std::uint16_t> attribute_count() const
    {
        return record_.field<std::uint16_t>(0x24);
    }
    SourcePmdField<std::uint8_t> attribute_byte(std::uint32_t index, unsigned lane) const;
    SourcePmdArrayView<std::array<std::uint8_t, 8>> attributes() const;
    SourcePmdArrayView<std::array<std::uint32_t, 4>> coordinates() const;
    float maximum_vertex_z() const; // source417E10 uses the low16bits of count
    SourcePmdLocation location() const
    {
        return record_.location();
    }
};
class SourcePmdModelView
{
    SourcePmdByteView file_;
    SourcePmdByteView section(std::uint32_t header) const;
    SourcePmdByteView descriptor_table(std::uint32_t member, std::uint32_t header, std::uint32_t stride) const;
    explicit SourcePmdModelView(SourcePmdByteView);
    friend class SourcePmdModel;

  public:
    SourcePmdModelView() = default;
    void check() const
    {
        (void)file_.size();
    }
    SourcePmdByteView bytes() const
    {
        check();
        return file_;
    }
    SourceAllocationRef allocation() const
    {
        return file_.location().allocation;
    }
    SourcePmdByteView descriptor() const;
    // 41816F writes exactly WORD+04; adjacent descriptor bytes retain their owner.
    SourcePmdField<std::uint16_t> ambience() const
    {
        return descriptor().field<std::uint16_t>(4);
    }
    SourcePmdField<std::uint8_t> first_polygon_attribute_flags() const; // 41C4E3: current descriptor+2C -> byte3
    SourcePmdField<std::uint32_t> current_detail_level() const
    {
        return descriptor().field<std::uint32_t>(0x5c);
    }
    SourcePmdField<std::uint32_t> previous_detail_level() const
    {
        return descriptor().field<std::uint32_t>(0x64);
    }
    SourcePmdTrackView track() const; // kind1, checked canonical projections
    std::uint32_t kind() const
    {
        return file_.field<std::uint32_t>(0x18).value();
    }
    std::uint32_t part_count() const
    {
        return descriptor().field<std::uint32_t>(0x24).value();
    }
    std::uint32_t maximum_detail_level() const
    {
        return descriptor().field<std::uint32_t>(0x58).value();
    }
    unsigned source_lod_index(unsigned slot) const;
    SourcePmdPartView part(unsigned lod, unsigned index) const;
    // 42B055 captures descriptor44 before44FBD0. Resolve only at the reached
    // part-access branch; count<=1/null44 callers must not invoke this method.
    // Reuse actual42ECA0 relocation history, never infer validity from extent.
    SourcePmdPartView captured_base_part_42b055(std::uint32_t captured_reference) const
    {
        return {file_, file_.relocated_part(captured_reference)};
    }
    SourcePmdArrayView<VehicleVector> normals() const;
    SourcePmdArrayView<std::uint16_t> corner_indices() const;
    SourcePmdField<VehicleVector> bound_vertex(std::uint32_t table_index) const;
    std::uint32_t texture_count() const;
    std::string texture_name(std::uint32_t index) const; // current canonical string, value only
    void replace_texture_suffix(std::string_view marker, std::int32_t value) const;
};
// Per-model track projections, not a snapshot of the process-global last track.
// Pointer-bearing members are resolved from their current canonical DWORD on
// each accessor. Returned byte/field views validate actual allocation lifetime.
class SourcePmdTrackView
{
    SourcePmdModelView model_;
    SourcePmdByteView section(std::uint32_t header, std::uint32_t stride) const;
    SourcePmdByteView collision_tail(std::uint32_t member, std::uint32_t header, std::uint32_t stride) const;
    explicit SourcePmdTrackView(SourcePmdModelView model) : model_(std::move(model)) {}
    friend class SourcePmdModelView;

  public:
    SourcePmdModelView model() const
    {
        model_.check();
        return model_;
    }
    SourcePmdByteView spatial_nodes() const; // 64-byte records
    SourcePmdArrayView<VehicleVector> spatial_bounds() const;
    SourcePmdArrayView<std::uint16_t> spatial_vertex_indices() const;
    SourcePmdArrayView<std::uint16_t> spatial_polygon_indices() const;
    SourcePmdByteView collision_descriptor() const;
    SourcePmdArrayView<std::uint16_t> collision_polygon_lists() const;
    SourcePmdArrayView<std::uint16_t> collision_indices() const;
    SourcePmdArrayView<VehicleVector> collision_vertices() const;
    SourcePmdArrayView<VehicleVector> collision_normals() const;
    SourcePmdByteView collision_polygons() const;          // 8-byte records
    SourcePmdByteView collision_materials() const;         // 4-byte records
    SourcePmdByteView collision_tree(unsigned lane) const; // 48-byte nodes from current root
    SourcePmdField<float> collision_tree_radius(unsigned lane) const;
    std::uint32_t spline_count() const; // literal unsigned floor(header10C/28)
    SourcePmdByteView spline_descriptor(std::uint32_t index) const;
    SourcePmdArrayView<VehicleVector> spline_points(std::uint32_t index) const;
    SourcePmdByteView spline_point_storage() const;
    SourcePmdByteView light_records() const; // 144-byte records, count from headerFC
    std::uint32_t light_count() const;
    std::uint32_t render_light_count() const;
    SourcePmdByteView ambient() const;           // authored bytes, no lighting-state copy
    SourcePmdLocation opaque58_location() const; // no invented record shape for42EF20
    std::uint32_t opaque58_count() const;
    SourcePmdByteView texture_motion_bytes() const;
    SourcePmdByteView texture_index_bytes() const;
    SourcePmdArrayView<VehicleVector> generated_normals() const;
};
// Exact source-global bindings. Normalized pointers retain weak bank identities
// after source free; they neither own payloads nor clear on non-track loads.
struct SourcePmdTrackGlobals
{
    SourcePmdLocation spatial_vertex_indices;                                 // 624464
    SourcePmdLocation spatial_polygon_indices;                                // 624468
    SourcePmdLocation spatial_bounds;                                         // 62446C
    SourcePmdLocation spatial_nodes;                                          // 624470
    SourcePmdLocation splines;                                                // 624474
    SourcePmdLocation opaque58;                                               // 624478
    std::variant<SourcePmdLocation, SourceSceneStaticPointer> lights;         // sole62447C: PMD or static scene2 alias
    SourcePmdLocation collision;                                              // 624480
    SourcePmdLocation texture_indices;                                        // 624484
    std::variant<SourcePmdLocation, SourceSceneStaticPointer> texture_motion; // sole624488
    SourcePmdLocation normals;                                                // 62448C
    SourcePmdLocation generated_normals;                                      // 624490
    std::variant<SourcePmdLocation, SourceSceneStaticPointer> ambient;        // sole624494
    std::uint32_t spline_count{};                                             // 624498
    std::uint32_t light_count{};                                              // 62449C
    std::uint32_t render_light_count{};                                       // 6244A0
    std::uint32_t opaque58_count{};                                           // 6244A4
    bool operator==(const SourcePmdTrackGlobals &) const = default;
};
// Exactly one model metadata owner. No geometry/transform/name payload arrays.
// Destruction expires checked views but does not source-free its bank block.
struct SourcePmdLoadingState;
struct SourcePmdFileSource;
struct SourcePmdObserver;
class SourcePmdModel
{
    std::shared_ptr<detail::SourcePmdAccess> access_;
    std::string source_name_;
    SourcePmdModel(std::shared_ptr<detail::SourcePmdAccess>, std::string name);
    friend SourcePmdModel load_source_pmd_42efb0(SourceAllocationBank &, SourcePmdLoadingState &, SourcePmdFileSource &,
                                                 std::string_view, SourcePmdObserver *);

  public:
    SourcePmdModel(const SourcePmdModel &) = delete;
    SourcePmdModel &operator=(const SourcePmdModel &) = delete;
    SourcePmdModel(SourcePmdModel &&) noexcept = default;
    SourcePmdModel &operator=(SourcePmdModel &&) noexcept = default;
    std::string_view source_name() const noexcept
    {
        return source_name_;
    }
    SourcePmdModelView view() const;
};
// Diagnostic copies of the pre-existing globals carry weak references only.
// They do not copy/retain the canonical request owner or its byte region.
struct SourcePmdLoadingValues
{
    SourceAllocationRef file;
    std::uint32_t file_size{}, version{};
    SourceAllocationRef descriptor_file;
    std::uint32_t descriptor_offset{};
    SourcePmdTrackGlobals track;
    bool operator==(const SourcePmdLoadingValues &) const = default;
};
struct SourcePmdLoadingState
{
    SourceAllocationRef file;             // 58D810, retained even after source free
    std::uint32_t file_size{}, version{}; // 58C230,58C804
    SourceAllocationRef descriptor_file;
    std::uint32_t descriptor_offset{}; // 58C800 normalized pointer into actual file
    SourcePmdTrackGlobals track;       // 624464..6244A4;576CC8 is not written by42EF80
    SourcePmdRequests requests;        // sole static name/list owner; no source bank allocation
    // Sole raw storage for the static regions reached by42EB00. No invented
    // renderer records, queue interpretation or source bank allocation.
    std::array<std::uint32_t, 1024> opaque_words_58c810{};
    std::array<std::uint32_t, 128> opaque_words_58c600{};
    // Source32-bit values, never host pointers. Non-null cursor producers and
    // consumers are not implemented by this reset-only contract.
    std::uint32_t opaque_cursor_58c538{}, opaque_cursor_58c80c{}, opaque_word_58d814{};
    SourceRequestBoundary reset_42eb00(SourceRequestObserver *observer = nullptr)
    {
        (void)requests.reset_42eb00_prefix(observer); // same first240 writes/thread guard
        std::uint32_t ordinal = 240;
        for (std::uint32_t i = 0; i < 1024; ++i)
        {
            opaque_words_58c810[i] = 0;
            requests.event(observer, ordinal, 0x42eb25, 0x58c810 + i * 4, 4);
        }
        for (std::uint32_t i = 0; i < 128; ++i)
        {
            opaque_words_58c600[i] = 0;
            requests.event(observer, ordinal, 0x42eb33, 0x58c600 + i * 4, 4);
        }
        requests.append_ = {};
        requests.event(observer, ordinal, 0x42eb35, 0x58c234, 4);
        requests.count_ = 0;
        requests.event(observer, ordinal, 0x42eb3b, 0x58c808, 4);
        descriptor_file = {};
        descriptor_offset = 0; // one normalized source-pointer store
        requests.event(observer, ordinal, 0x42eb41, 0x58c800, 4);
        file = {};
        requests.event(observer, ordinal, 0x42eb47, 0x58d810, 4);
        file_size = 0;
        requests.event(observer, ordinal, 0x42eb4d, 0x58c230, 4);
        opaque_cursor_58c538 = 0;
        requests.event(observer, ordinal, 0x42eb53, 0x58c538, 4);
        opaque_cursor_58c80c = 0;
        requests.event(observer, ordinal, 0x42eb59, 0x58c80c, 4);
        opaque_word_58d814 = 0;
        requests.event(observer, ordinal, 0x42eb5f, 0x58d814, 4);
        // Source has no free/call, no58C804 or624460/track store, no model/LOD edit.
        return {0x42eb66, 0, true};
    }
    SourcePmdLoadingValues snapshot() const
    {
        return {file, file_size, version, descriptor_file, descriptor_offset, track};
    }
};
struct SourcePmdFileRead
{
    std::uint32_t source_size{}, copied_bytes{};
};
struct SourcePmdFileSource
{
    virtual ~SourcePmdFileSource() = default;
    virtual std::uint32_t size(std::string_view model_name) = 0;
    virtual SourcePmdFileRead read(std::string_view model_name, std::span<std::uint8_t>) = 0;
};
// Actual loose MeshData route. No source allocation/name cache is introduced;
// each request queries/reads independently into its own actual bank block.
class SourcePmdFileSystem final : public SourcePmdFileSource
{
    std::filesystem::path root_;
    std::filesystem::path path(std::string_view) const;

  public:
    explicit SourcePmdFileSystem(std::filesystem::path root) : root_(std::move(root)) {}
    std::uint32_t size(std::string_view) override;
    SourcePmdFileRead read(std::string_view, std::span<std::uint8_t>) override;
};
enum class SourcePmdEventKind
{
    allocation,
    read,
    globals,
    descriptor,
    parts,
    names,
    bounds,
    track_spatial,
    track_collision,
    track_splines,
    track_lighting,
    track_opaque58,
    track_textures,
    track_normals,
    complete
};
struct SourcePmdEvent
{
    SourcePmdEventKind kind;
    std::uint32_t site;
    SourceAllocationRef allocation;
    std::uint32_t requested_bytes;
};
struct SourcePmdObserver
{
    virtual ~SourcePmdObserver() = default;
    virtual void observe(const SourcePmdEvent &, const SourceAllocationBank &, const SourcePmdLoadingState &) = 0;
};
SourcePmdModel load_source_pmd_42efb0(SourceAllocationBank &, SourcePmdLoadingState &, SourcePmdFileSource &,
                                      std::string_view name, SourcePmdObserver * = nullptr);
} // namespace sgn::retail
