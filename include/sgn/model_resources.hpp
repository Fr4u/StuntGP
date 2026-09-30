#pragma once
#include "sgn/model_identity.hpp"
#include "sgn/source_pmd_model.hpp"
#include "sgn/vehicle_presentation.hpp"
#include "sgn/vehicle_trails.hpp"
#include "sgn/retail_level_selection.hpp"
#include <deque>
#include <functional>
#include <variant>

namespace sgn::retail
{
class RaceSessionPlatform;
class SourceVehicleClear444dc9;
struct SourceBootSearchBoundary;
// One authored 80-byte transform. W/tail words are preserved without assigning
// semantics. Every part and LOD that names this transform borrows this owner.
struct ModelTransform
{
    Vec3 position{};
    std::uint32_t position_w{};
    Vec3 scale{};
    std::uint32_t scale_w{};
    Vec3 forward{};
    std::uint32_t forward_w{};
    Vec3 up{};
    std::uint32_t up_w{};
    std::array<std::uint32_t, 4> tail{};
};
struct ModelPartBinding
{
    std::uint16_t &flags;
    ModelTransform &transform;
    const Vec3 &pivot;
};
// 42ED30 binds four authored track-only sections. Entries remain indices,
// never guest pointers;42ABB0 reads node order and list terminators literally.
struct ModelSpatialData
{
    std::vector<VehicleVector> bounds;
    std::vector<std::uint16_t> vertex_indices, polygon_indices;
    std::vector<std::array<std::uint16_t, 32>> nodes; // eight8-byte children
};
class ModelResource
{
    std::string source_name_; // immutable native provenance of the exact load request
    struct Part
    {
        std::uint16_t flags{}, flags_02{};
        std::uint32_t transform{};
        Vec3 pivot{};
        std::uint32_t first_vertex{}, vertex_count{};
        bool relocated{};
        std::uint32_t first_attribute{}, first_coordinate{};
        std::uint16_t attribute_count{}, coordinate_count{};
        std::array<std::uint16_t, 2> bounds_20{};
        std::uint16_t word_04{};
    };
    struct Lod
    {
        Model geometry;
        std::vector<std::size_t> parts;
        std::size_t first_part{};
    };
    std::vector<ModelTransform> transforms_;
    std::vector<Part> parts_;
    std::vector<Lod> lods_;
    std::array<std::optional<unsigned>, 4> source_lods_{};
    // Exactly one current5C owner: model value for ordinary resources, or the
    // existing Session car word after binding. The alternatives are exclusive.
    std::variant<std::uint32_t, std::reference_wrapper<std::uint32_t>> current_detail_level_{std::uint32_t{}};
    std::uint32_t previous_detail_level_{};   // descriptor+64
    const ModelTransform *outer_transform_{}; // descriptor+0C; borrowed queue/template, initial null in all78 PMDs
    std::vector<VehicleVector> vertices_;
    std::vector<std::uint16_t> corner_indices_;
    std::uint16_t model_flags_02_{};
    std::uint16_t model_ambience_{};       // descriptor+04,418150 writes only this halfword
    VehicleVector descriptor_vector_10_{}; // opaque descriptor+10..1F,41C420 copies all four words
    std::vector<std::array<std::uint8_t, 8>> attributes_;
    std::vector<std::array<std::uint32_t, 4>> coordinates_;
    // Current descriptor+60 labels before42BB70's upload path rewrite. Fixed
    // container after import: namespace pointers borrow stable string objects.
    std::vector<std::string> raw_texture_names_;
    std::vector<VehicleVector> normals_, bound_vectors_;
    std::vector<std::uint32_t> bound_indices_;
    std::array<std::uint16_t, 2> bounds_20_{};
    std::optional<ModelSpatialData> spatial_;
    Part &source_record(unsigned lod, unsigned index);

  public:
    ModelResource(const AssetStore &, const std::filesystem::path &retail_root, const std::string &name,
                  std::string_view track_set);
    std::string_view source_name() const noexcept
    {
        return source_name_;
    }
    std::size_t lod_count() const noexcept
    {
        return lods_.size();
    }
    // All four relocated descriptor48..54 slots exist independently of the
    // maximum distinct LOD at58. Authored aliases resolve to the same owner.
    unsigned source_lod_index(unsigned source_slot) const;
    std::uint32_t source_maximum_detail_level() const noexcept
    {
        return static_cast<std::uint32_t>(lods_.size() - 1);
    }
    std::uint32_t &current_detail_level() noexcept;
    const std::uint32_t &current_detail_level() const noexcept;
    // At resource binding, transfer the imported5C value into the sole Session
    // car word and retain its reference. Rebinding to a different word is an error.
    void bind_current_detail_level(std::uint32_t &session_word);
    const ModelTransform *&source_outer_transform() noexcept
    {
        return outer_transform_;
    }
    const ModelTransform *const &source_outer_transform() const noexcept
    {
        return outer_transform_;
    }
    std::uint32_t &previous_detail_level() noexcept
    {
        return previous_detail_level_;
    }
    const std::uint32_t &previous_detail_level() const noexcept
    {
        return previous_detail_level_;
    }
    std::size_t part_count(unsigned lod) const;
    const Model &geometry(unsigned lod) const;
    std::span<const std::string> raw_source_texture_names() const noexcept
    {
        return raw_texture_names_;
    }
    // 413190/413220 mutate each name in the descriptor+60 table. Every LOD
    // borrows that same source namespace; resident render copies stay in sync.
    // First case-sensitive match; replace everything after marker with signed
    // decimal. Selection/RNG/caller order remain owned by the Session loader.
    void replace_source_texture_suffix(std::string_view marker, std::int32_t value);
    ModelPartBinding part(unsigned lod, unsigned index);
    // Some original initializer predicates reach an adjacent LOD record. Only
    // actual authored/relocated records are accessible through this interface.
    ModelPartBinding source_part(unsigned lod, unsigned index);
    std::uint16_t &source_part_flags_02(unsigned lod, unsigned index); // distinct source part+2, not visibility flags+0
    std::uint16_t &source_part_word_04(unsigned lod, unsigned index);  // 42A870 reads literal part+04
    // 413E60 walks contiguous80-byte records from part11's transform identity.
    //  It does not consult the following parts' individual transform indices.
    std::span<ModelTransform> source_transform_run(unsigned lod, unsigned part, unsigned count);
    std::span<std::array<std::uint8_t, 8>> source_attributes(unsigned lod, unsigned part);
    std::span<std::array<std::uint32_t, 4>> source_coordinates(unsigned lod, unsigned part);
    std::span<const VehicleVector> source_vertices(unsigned lod, unsigned part);
    // 426ED0 helpers mutate this same owner; aliases and subsequent producers
    // must observe those writes without rebuilding a geometry snapshot.
    std::span<VehicleVector> mutable_source_vertices(unsigned lod, unsigned part);
    // Literal descriptor+28 normal table and42EBB0's bound-pointer table.
    // Returned owners are stable; index aliases resolve to the same vector.
    std::span<VehicleVector> source_normals() noexcept
    {
        return normals_;
    }
    std::span<const VehicleVector> source_normals() const noexcept
    {
        return normals_;
    }
    std::span<const std::uint32_t> source_bound_indices() const noexcept
    {
        return bound_indices_;
    }
    const VehicleVector &source_bound_vertex(std::size_t table_index) const
    {
        return bound_vectors_.at(bound_indices_.at(table_index));
    }
    const std::array<std::uint16_t, 2> &source_model_bounds() const noexcept
    {
        return bounds_20_;
    }
    const std::array<std::uint16_t, 2> &source_part_bounds(unsigned lod, unsigned part);
    const ModelSpatialData *source_spatial() const noexcept
    {
        return spatial_ ? &*spatial_ : nullptr;
    }
    std::span<const std::uint16_t> source_corner_indices() const noexcept
    {
        return corner_indices_;
    }
    std::span<std::uint16_t> source_corner_indices() noexcept
    {
        return corner_indices_;
    }
    std::uint16_t source_model_flags_02() const noexcept
    {
        return model_flags_02_;
    }
    std::uint16_t &source_model_flags_02() noexcept
    {
        return model_flags_02_;
    }
    std::uint16_t &source_model_ambience() noexcept
    {
        return model_ambience_;
    }
    std::uint16_t source_model_ambience() const noexcept
    {
        return model_ambience_;
    }
    VehicleVector &source_model_vector_10() noexcept
    {
        return descriptor_vector_10_;
    }
    const VehicleVector &source_model_vector_10() const noexcept
    {
        return descriptor_vector_10_;
    }
    std::span<std::array<std::uint8_t, 8>> source_attribute_table() noexcept
    {
        return attributes_;
    }
    float maximum_source_vertex_z(unsigned lod, unsigned index);
    VehiclePresentationParts presentation_parts(unsigned lod);
    VehicleTrailModel trail_model(); // source always selects LOD zero
};
// Native resource identities are nonzero 32-bit tokens in LevelCarSlot's two
// resource words. Only this table resolves tokens to size_t handles. Zero means
// no resource. No guest or host address is stored in a selection record.
class ModelResources
{
    friend class SceneResources;          // 42F090 uses the sole retained loading state
    friend class SourceWorldTrackContext; // 42ABB0 reads actual last-loader locations in this same bank
    const AssetStore &assets_;
    std::filesystem::path root_;
    // Exclusive ownership modes per entry: a source model never constructs
    // the legacy decoded geometry/transform/name vectors beside its bank block.
    std::deque<std::variant<ModelResource, SourcePmdModel>> models_;
    SourceAllocationBank *source_bank_{};  // same bank for the entire source-mode table; outlives this owner
    SourcePmdLoadingState source_loading_; // one retained58D810/58C230/58C804/58C800 owner
  public:
    ModelResources(const AssetStore &assets, std::filesystem::path retail_root)
        : assets_(assets), root_(std::move(retail_root))
    {
    }
    // Each request creates a distinct mutable instance, including equal names.
    // 42F090 calls42EFB0/42E8D0 separately for every request before relocation.
    std::uint32_t load(const std::string &name, std::string_view track_set = {});
    // Explicit source-mode request. No hidden42EAB0,begin,warm or name-cache
    // shortcut. Cannot mix into an already populated legacy resource table.
    std::uint32_t load_source(SourceAllocationBank &, const std::string &name, SourcePmdObserver * = nullptr);
    std::uint32_t load_source(SourceAllocationBank &, SourcePmdFileSource &, const std::string &name,
                              SourcePmdObserver * = nullptr);
    SourcePmdModelView source_at(std::size_t handle) const;
    bool source_backed(std::size_t handle) const;
    std::uint32_t current_detail_level(
        std::size_t handle) const; // value from the current owner, never a second LOD field
    unsigned source_lod_index(std::size_t handle, unsigned source_slot) const;
    std::size_t distinct_lod_count(std::size_t handle) const;
    VehicleVector source_pivot(std::size_t handle) const;
    const SourcePmdLoadingState &source_loading_state() const noexcept
    {
        return source_loading_;
    }
    void produce_source_car_requests_444db0(RaceSessionPlatform &, SourceVehicleClear444dc9 &);
    void produce_source_level_assets_42c740(RaceSessionPlatform &, SourceBootSearchBoundary);
    // Bounded source entries; no full42D070/42F090/42EB00 activation.
    SourceRequestBoundary initialize_source_requests_42d070_prefix(SourceRequestObserver *observer = nullptr)
    {
        return source_loading_.requests.initialize_42d070_prefix(observer);
    }
    SourceRequestBoundary produce_source_requests_42c740_prefix(LevelBlock block,
                                                                SourceRequestObserver *observer = nullptr)
    {
        return source_loading_.requests.produce_42c740_prefix(block, observer);
    }
    SourceRequestBoundary reset_source_requests_42eb00_prefix(SourceRequestObserver *observer = nullptr)
    {
        return source_loading_.requests.reset_42eb00_prefix(observer);
    }
    // Full leaf on the same loading/request owner; no normal-entry activation.
    SourceRequestBoundary reset_source_resources_42eb00(SourceRequestObserver *observer = nullptr)
    {
        return source_loading_.reset_42eb00(observer);
    }
    // Reached42E656..42E6BC: clears624460 then the same loader track globals.
    // Keeps all PMD allocations/metadata;42EB00 remains a separate tail call.
    void reset_source_track_globals_42e656();
    // Full42D700 and42D7E0/42D970 on SAME static or actual PMD allocation owners.
    // Method name retained for existing callers; scene!=2 is not a menu fallback.
    void initialize_source_menu_lights_42d700(const std::shared_ptr<RaceSessionPlatform> &);
    void replace_texture_suffix(std::size_t handle, std::string_view marker, std::int32_t value);
    std::optional<std::size_t> resolve(std::uint32_t identity) const;
    std::uint32_t identity(std::optional<std::size_t> handle) const;
    ModelResource &at(std::size_t handle);
    std::string_view source_name(std::size_t handle) const;
    std::size_t size() const noexcept
    {
        return models_.size();
    }
};
struct LevelModelHandles
{
    std::optional<std::size_t> sky, track;
};
using ModelBindingSlots = std::array<std::optional<std::size_t>, 16>;
// Platform resource fulfillment of the already recovered request order. Assets
// are validated before publishing bindings. This is not another car selector.
LevelModelHandles bind_level_model_resources(ModelResources &, std::span<const LevelAssetRequest>, ModelBindingSlots &,
                                             std::span<const SelectionRecord, 6>, std::span<LevelCarSlot, 6>,
                                             std::string_view track_set = {});
// Same42F090 request/publication order, explicit already-initialized bank.
// Loading and bank lifecycle remain caller-owned; no legacy owner is created.
LevelModelHandles bind_source_level_model_resources(ModelResources &, SourceAllocationBank &,
                                                    std::span<const LevelAssetRequest>, ModelBindingSlots &,
                                                    std::span<const SelectionRecord, 6>, std::span<LevelCarSlot, 6>,
                                                    SourcePmdObserver * = nullptr);
// 413190 after its separate413110 RNG producer, followed by413220. Physical
// slot order and all names of each body model are preserved. Run before upload.
void remap_level_vehicle_textures(ModelResources &, std::span<const LevelCarSlot, 6>,
                                  std::span<const std::int8_t, 6> random_liveries);
} // namespace sgn::retail
