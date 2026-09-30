#pragma once
#include "sgn/retail_material_storage.hpp"
#include "sgn/borrowed_material_name.hpp"

namespace sgn::retail
{
struct WorldMaterialPartView
{
    std::span<WorldPolygonHeader> headers;
};
struct WorldMaterialLodView
{
    std::span<const WorldMaterialPartView> parts;
};
struct WorldMaterialModelView
{
    // Exactly descriptor58+1 source slots. Equal pointers remain equal, but are
    // NOT deduplicated across LOD slots. Source42B8B0 checks part headers only.
    std::span<const WorldMaterialLodView *const> lods;
    std::uint32_t part_count{};             // descriptor24; each LOD provides at least this many parts
    std::span<const std::string> raw_names; // current descriptor60 before upload path rewrite
};
struct WorldMaterialRegistryEntry
{
    const WorldMaterialModelView *model{}; // source null terminates traversal
    std::uint32_t flags04{};               // 42C510 load/base selection only;42BA00 ignores it
};
using WorldMaterialRegistry = std::span<const WorldMaterialRegistryEntry>;
// Pointers borrow stable string OBJECTS, allowing content mutations without
// dangling string_view storage. Owners must outlive this namespace and calls.
// Span end represents the source terminal null; an earlier null also terminates.
using WorldMaterialNames = std::span<const std::string *const>;
bool world_material_part_already_visited(const WorldMaterialModelView &, std::uint32_t lod,
                                         std::uint32_t part);                                // 42B8B0
void add_world_material_base(const WorldMaterialModelView &, std::uint32_t logical_base);    // full42B900
void replace_world_material_id(WorldMaterialRegistry, std::uint32_t from, std::uint32_t to); // full42BA00
void remap_world_material_names(WorldMaterialRegistry, WorldMaterialNames logical,
                                WorldMaterialNames unique); // full42BA90
// Full42B9A0 meaningful return:0 for a match, otherwise last unsigned-byte
// strcmp(existing,requested) result (-1/+1); empty table returns1.
std::int32_t world_material_name_lookup(WorldMaterialNames, const std::string &requested);

struct WorldMaterialUpload
{
    std::uint32_t primary_texture{}, alternate_texture{};
    std::uint16_t dimension{};
};
struct WorldMaterialNamespace
{
    std::vector<BorrowedMaterialName> logical_names, unique_names;
    PhysicalMaterialStorage materials;
    std::uint32_t logical_base{}, batch_upload_count{}, physical_upload_count{}, sky_material_base{};
};
// Typed successful-one-upload transition from42C440 and42BF60. These functions
// do not claim full42C510/42BF60 file loading, COM creation or failure behavior.
// Raw name identity must be tested before path rewrite. Duplicate requests have
// no upload; first occurrences require the platform's actual completed upload.
std::int32_t world_material_name_lookup(BorrowedMaterialNames, const BorrowedMaterialName &);
void remap_world_material_names(WorldMaterialRegistry, BorrowedMaterialNames logical, BorrowedMaterialNames unique);
void commit_world_material_request(WorldMaterialNamespace &, const BorrowedMaterialName &,
                                   std::optional<WorldMaterialUpload>);
void publish_world_material_request_42c03c(WorldMaterialNamespace &, const BorrowedMaterialName &);
void begin_world_material_batch(WorldMaterialNamespace &);                               // 42C440..42C44F
std::uint32_t next_world_material_motion_index(const WorldMaterialNamespace &) noexcept; // 42BFA5..42BFD0
void commit_world_material_request(WorldMaterialNamespace &, const std::string &raw_name,
                                   std::optional<WorldMaterialUpload> completed_upload);
// 42C03C..42C08A after actual physical completion and auxiliary publication.
// The full loader already prepared/uploaded the record; do not prepare it again.
void publish_world_material_request_42c03c(WorldMaterialNamespace &, const std::string &raw_name);
// The caller must perform source413190/413220 renames before constructing the
// namespace and must invalidate borrowed name arrays before resource teardown.
} // namespace sgn::retail
