#pragma once
#include "sgn/retail_frontend_font_metrics.hpp"
#include "sgn/retail_world_material_loading.hpp"
#include <array>
#include <memory>
#include <vector>

namespace sgn::retail {
inline constexpr std::size_t retail_font_record_bytes=0x1ba0;
inline constexpr std::size_t retail_font_record_words=retail_font_record_bytes/4;
enum class FontBankEventKind { allocation,read,decoded,packed,upload,release };
struct FontBankEvent {
    FontBankEventKind kind;
    std::uint32_t source_site;
    SourceAllocationRef allocation;
    std::uint32_t requested_bytes{};
    std::uint32_t category{};
};
// New read-only receiving seam. It observes the ACTUAL bank and consumed
// allocation, never seeds rows. A throwing observer is an explicit host abort.
struct FontBankObserver {
    virtual ~FontBankObserver()=default;
    virtual void observe(const FontBankEvent&,const SourceAllocationBank&)=0;
};
struct SourceFontContext {
    SourceAllocationBank& bank;
    SourceTgaDecodeState& decode;
    FontBankObserver* observer{};
};
//The existing contiguous metrics projection remains the SOLE owner of source
//width and advances. The remaining1607words per font contain every other
//source word. source_word maps the full record without duplicating any value.
class RetailFontResources {
    std::array<std::uint32_t,322> metric_words_{};
    std::array<std::array<std::uint32_t,1607>,2> other_words_{};
    //Legacy preview ownership. Source428C00 requests1024bytes per font;
    //these host owners remain only for the explicitly unmigrated overload.
    //The migrated overload below uses the actual source bank zero fill.
    std::vector<std::unique_ptr<std::array<std::uint8_t,1024>>> scratch_;
    // Migrated mode: only weak generation refs/normalized source+4 IDs.
    // The bank owns even overwritten scratch rows until its source sweep.
    std::vector<SourceAllocationRef> source_scratch_;
    const SourceAllocationBank* source_bank_{}; //borrowed identity; bank outlives this source-bound owner
public:
    std::array<std::uint8_t,160> index{};
    std::array<std::uint8_t,256> glyph_map{};
    std::uint32_t current{},count{},digit_advance_bits{};
    std::uint32_t& source_word(std::size_t font,std::size_t byte_offset);
    const std::uint32_t& source_word(std::size_t font,std::size_t byte_offset) const;
    FrontendFontMetricsViews metrics() &;
    FrontendFontMetricsViews metrics() &&=delete;
    std::uint32_t allocate_scratch(std::size_t font);
    std::size_t scratch_allocations() const noexcept{return scratch_.size()+source_scratch_.size();}
    // Explicit host binding, no initialize/begin/allocate. Rejects a different
    // bank or mixing existing preview allocations into the migrated history.
    void bind_source_bank(const SourceAllocationBank&);
    void require_preview_mode() const;
    std::uint32_t allocate_source_scratch(std::size_t font,SourceAllocationBank&,FontBankObserver*);
    SourceAllocationRef source_scratch_allocation(std::size_t font,const SourceAllocationBank&) const;
};
//Distinct4325E0 boundary. Ordinary4303A0 upload has different resource count,
//descriptor fields, material writes and transparency-cursor effects.
struct FontAtlasUploadTarget {
    virtual ~FontAtlasUploadTarget()=default;
    virtual std::uint8_t& current_transparency()=0;
    //Full4325E0 completion boundary: return the PHYSICAL MATERIAL INDEX,
    //not a host texture token. One primary destination and material word only.
    //This target owns the resource and material cursors; never advances58271C
    //or592028. The CPU initializer does not emulate GPU completion.
    virtual std::uint32_t upload_font_atlas(PreparedMaterialPixels)=0;
};
struct FontReloadViews {
    std::span<std::uint8_t,8160> shared_name_arena; //57EB0C, also used by42BE20
    std::uint32_t& shared_name_cursor; //580AEC, byte offset
    std::span<std::uint32_t,16> font_reload_records; //5895C0, eight pairs
    std::uint32_t& font_reload_cursor; //5895BC, record index
    const std::uint32_t& ordinary_material_count; //58271C, never increment here
};
struct FontImageGlobals {
    std::uint32_t& width;
    std::uint32_t& height;
    std::uint32_t& tga_width;
    std::uint32_t& tga_height;
};
struct FontAtlasObserver {
    virtual ~FontAtlasObserver()=default;
    //Read-only qualification seam at4327E0 entry, before RGB conversion.
    virtual void packed_atlas(std::size_t font,std::span<const std::uint8_t>)=0;
};
void append_font_reload_42be90(FontReloadViews,std::string_view source_path);
void upload_retail_font_atlases(RetailFontResources&,std::size_t font,
    std::span<const std::uint8_t> packed_rgb,std::uint32_t device_enabled,FontAtlasUploadTarget&);
//Full428FA0 CPU owner/call order, including4622C0 RGB decoding and42BE90.
//source_graphics_root is the caller's original48491C spelling (not a host
//file location). MaterialFileSource resolves typed paths to owned WAD bytes.
void initialize_retail_font_resources(RetailFontResources&,MaterialFileSource&,
    FontImageGlobals,FontReloadViews,std::string_view source_graphics_root,
    std::int32_t window_width,std::uint32_t device_enabled,FontAtlasUploadTarget&,
    FontAtlasObserver* =nullptr);
//Candidate source428C00 and its small metric helpers. Input is decoded RGB24
//from4622C0, not TGA file bytes. No GPU/material publication happens here.
Bytes prepare_retail_font_atlas(RetailFontResources&,std::size_t font,
    std::span<const std::uint8_t> decoded_rgb,std::uint32_t source_width,
    std::uint32_t source_height,std::int32_t window_width);
// Explicit migrated entry. The old overload above remains an unmigrated
// preview/fixture path; it never creates or initializes a hidden source bank.
void initialize_retail_font_resources(RetailFontResources&,SourceFontContext,MaterialFileSource&,
    FontImageGlobals,FontReloadViews,std::string_view source_graphics_root,
    std::int32_t window_width,std::uint32_t device_enabled,FontAtlasUploadTarget&,
    FontAtlasObserver* =nullptr);
// One real428E40 producer: current font must already select font+1.
// All temporaries are bank-owned. Exceptions leave reached allocations in
// source state; no host RAII free, artificial rollback or warm activation.
void load_source_font_resource_428e40(RetailFontResources&,std::size_t font,SourceFontContext,
    MaterialFileSource&,FontImageGlobals,FontReloadViews,std::string_view source_graphics_root,
    std::string_view font_name,std::int32_t window_width,std::uint32_t device_enabled,
    FontAtlasUploadTarget&,FontAtlasObserver* =nullptr);
void prepare_source_font_atlas_428c00(RetailFontResources&,std::size_t font,SourceFontContext,
    const SourceAllocationRef& decoded_rgb,const SourceAllocationRef& atlas,
    std::uint32_t width,std::uint32_t height,std::int32_t window_width);
void upload_source_font_atlases_4327e0(RetailFontResources&,std::size_t font,SourceFontContext,
    const SourceAllocationRef& atlas,std::uint32_t device_enabled,FontAtlasUploadTarget&);
}
