#pragma once
#include "sgn/retail_world_materials.hpp"
#include "sgn/retail_material_reload.hpp"
#include "sgn/source_allocation_bank.hpp"
#include <deque>
#include <memory>

namespace sgn::retail
{
class EffectAnimationStorage;
// Source decoder globals62F5C8/CC/D0. These are references into the one
// platform allocation bank, never additional payload owners. The cursor can
// refer to a freed generation after462425; do not dereference without bank.bytes.
struct SourceTgaDecodeState
{
    std::uint32_t pixel_depth{};
    SourceAllocationRef file;
    SourceAllocationRef pixel_cursor_allocation;
    std::uint32_t pixel_cursor_offset{};
};
enum class MaterialResourceRoot : std::uint32_t
{
    graphics24,
    graphics24_max
};
struct MaterialResourcePath
{
    MaterialResourceRoot root{};
    std::string relative_name; // source spelling, including extension; no host drive
};
struct MaterialFileRead
{
    std::uint32_t source_size{};
    std::size_t copied_bytes{};
};
struct MaterialFileSource
{
    virtual ~MaterialFileSource() = default;
    virtual std::uint32_t source_file_size(const MaterialResourcePath &) = 0;
    virtual MaterialFileRead read_source_file(const MaterialResourcePath &, std::span<std::uint8_t>) = 0;
};
struct PreparedMaterialPixels
{
    std::uint32_t width{}, height{};
    std::span<const std::uint16_t> words; // source A1R5G5B5; retained only during call
};
struct CompletedMaterialUpload
{
    std::uint32_t primary{}, alternate{};
    std::uint16_t dimension{};
};
// A real backend completion or explicit failure. A failure must not publish
// successful logical/unique records. The backend owns partial resource state.
struct MaterialSourceFailure : FormatError
{
    std::uint32_t source_site{}, hresult{};
    MaterialSourceFailure(std::uint32_t site, std::uint32_t result, const char *message)
        : FormatError(message), source_site(site), hresult(result)
    {
    }
};
struct MaterialTextureBackend
{
    virtual ~MaterialTextureBackend() = default;
    virtual void release_all() = 0;                                    // 431810 ->431790; does not imply430690 reset
    virtual std::uint8_t &current_transparency() = 0;                  // live source592028 target
    virtual void begin_pending_record(std::uint32_t motion_index) = 0; //+0C=0,+14
    virtual CompletedMaterialUpload upload(PreparedMaterialPixels) = 0;
    // 430730 performs no initial pixel fill. The backend must allocate a real
    // resource; any modern initialization policy is explicitly host behavior.
    virtual CompletedMaterialUpload generate(std::uint32_t dimension) = 0;
};
struct WorldMaterialPreludeEntry
{
    const std::string *name{}; // null terminates; generated entries ignore text
    std::uint32_t type{}, selector{};
};
using WorldMaterialPrelude = std::span<const WorldMaterialPreludeEntry>;
WorldMaterialPrelude source_world_material_prelude(std::uint32_t scene); // PE4955E8/47F318

struct MaterialImageName
{
    enum class Arena : std::uint32_t
    {
        none,
        active,
        persistent
    };
    Arena arena{};
    std::uint32_t offset{};
};
struct MaterialImagePayload
{
    Bytes bytes;
    std::uint64_t identity{};
};
struct MaterialImageRecord
{
    std::uint32_t marker{}; // record+0 is retained for new430CA0 loads
    MaterialImageName name;
    std::shared_ptr<const MaterialImagePayload> payload;
    std::uint32_t width{}, height{}, format{};
};
struct WorldMaterialLoadingState
{
    WorldMaterialNamespace names;
    // Sole newly represented source words;42C480 retains these values.
    std::uint32_t source_quality_6249a0{}, source_maximum_dimension_6249a4{};
    std::uint32_t source_loading_shown_60f34c{}; // 445A40 caller/child share this word
    // Borrow the persistent world owner. Null means this bounded loader has
    // no descriptor lifecycle binding; it does not prove full42C480. Session
    // always binds its actual owner before loading. Not owned or reset here.
    EffectAnimationStorage *effect_animation_storage{};
    // Sole6243BC publication snapshot, initial PE0. Only42E772/445A1F/445A92
    // publish this word;42C480 retains it. Consumers borrow this same owner.
    std::uint32_t published_material_count{};
    std::uint32_t width{320}, height{200}, tga_width{320},
        tga_height{200};                             // pinned PE489714..489723; reset retains these
    SourceTgaDecodeState source_tga_decode;          // shared4622C0 globals; migrated font entry only so far
    std::uint32_t format{}, pc_flag{}, byte_count{}; // format is source58332C: PE0,42BD6B writes1; fonts test exactly1.
    std::vector<std::uint32_t> upload_to_logical;
    MaterialReloadStorage reload;                    // One42BDD0 owner, shared by ordinary and font reload entries.
    std::array<std::uint8_t, 600> generated_arena{}; // 583334..58358B
    std::uint32_t generated_cursor{};
    std::vector<std::uint32_t> generated_offsets;
    std::deque<std::string> generated_names;                 // stable OBJECT mirrors of live suffixes
    std::array<std::uint32_t, 255> image_ordinals{};         // 583594..58398F
    std::array<std::uint8_t, 16384> image_name_arena{};      // 583990..58798F; ordinal255+ aliases here
    std::array<std::uint8_t, 12000> persistent_name_arena{}; // 57B424..57E303
    std::array<MaterialImageRecord, 300> images{}, persistent_images{};
    std::uint32_t image_count{}, image_name_cursor{}, image_byte_count{};
    std::uint32_t persistent_count{}, persistent_name_cursor{}, cache_initialized{};
    std::uint32_t transient_seen{}, transient_byte_start{};
    std::uint64_t next_payload_identity{1}; // host ownership token, not a retail address
    WorldMaterialLoadingState() = default;
    WorldMaterialLoadingState(const WorldMaterialLoadingState &) = delete;
    WorldMaterialLoadingState &operator=(const WorldMaterialLoadingState &) = delete;
    std::string image_name(MaterialImageName) const;
    std::uint32_t image_ordinal(std::uint32_t) const;
};
struct WorldMaterialLoadingContext
{
    std::uint32_t scene{}, quality{}, loading_presentation_enabled{}, loading_already_shown{};
    std::int32_t maximum_dimension{};
    std::string track_prefix;
    WorldMaterialNames transient_image_names; // 441180 table; actual initial table is empty
    // Caller-owned source spelling48491C/48499C, independent of host I/O.
    // Bind actual startup state explicitly;42E3E0 may have rewritten normal.
    const SourceGraphicsRoots *source_graphics_roots{};
};
struct WorldMaterialLoadingPresentation
{
    virtual ~WorldMaterialLoadingPresentation() = default;
    virtual void run(WorldMaterialLoadingState &, WorldMaterialLoadingContext &, MaterialFileSource &,
                     MaterialTextureBackend &) = 0;
};
// Private first gate's supported root domain:60DFA0!=1 OR60F34C==1.
// A required445A40 without a separately implemented consumer is explicit failure.
void load_world_materials(WorldMaterialRegistry, WorldMaterialLoadingState &, WorldMaterialLoadingContext &,
                          MaterialFileSource &, MaterialTextureBackend &,
                          WorldMaterialLoadingPresentation * = nullptr);                  // 42C510
void reset_world_material_loading(WorldMaterialLoadingState &, MaterialTextureBackend &); // 42C480
void load_world_material_prelude(WorldMaterialPrelude, WorldMaterialLoadingState &, const WorldMaterialLoadingContext &,
                                 MaterialFileSource &, MaterialTextureBackend &); // 42C340
void load_world_material_requests(std::span<const std::string>, WorldMaterialLoadingState &,
                                  const WorldMaterialLoadingContext &, MaterialFileSource &,
                                  MaterialTextureBackend &); // 42C440
void load_world_material_request(const std::string &, std::uint32_t requested_quality, WorldMaterialLoadingState &,
                                 const WorldMaterialLoadingContext &, MaterialFileSource &,
                                 MaterialTextureBackend &); // 42BF60
void load_world_material_request(const BorrowedMaterialName &, std::uint32_t requested_quality,
                                 WorldMaterialLoadingState &, const WorldMaterialLoadingContext &, MaterialFileSource &,
                                 MaterialTextureBackend &);
void load_source_pmd_material_requests_42c440(const SourcePmdMaterialNames &, WorldMaterialLoadingState &,
                                              const WorldMaterialLoadingContext &, MaterialFileSource &,
                                              MaterialTextureBackend &);
void generate_world_material(std::uint32_t fallback_dimension, std::uint32_t selector, WorldMaterialLoadingState &,
                             MaterialTextureBackend &); // 42C0A0
// NEW HOST (R30 perf-tick), not recovered behaviour. The exact RGBA image the frontend 430D40 binding
// (SourceFrontendTextureReplacement) uploads for this record: decode_tm for format 1, otherwise the
// 5-bit expansion of the stored A1R5G5B5 words, with the same FormatError checks and messages. A payload
// is immutable after loading, so the image is decoded once per payload object (small LRU keyed by that
// object, its format and extent) and shared. A race-time swap then does no decode inside the tick.
std::shared_ptr<const Image> source_replacement_image(const MaterialImageRecord &);
} // namespace sgn::retail
