#pragma once
#include "sgn/source_save_page.hpp"
#include "sgn/persistence_file_service.hpp"
#include "sgn/source_boot_config.hpp"
#include <span>
namespace sgn::retail
{
class SourceApplicationCore;
class NativeSourceMenuCrt;
class SourceSavePageDrawChildren;
// Concrete card leaves, replay row and raw replay load/save over retained Core/CRT owners.
// This transfers the original replay bytes; it does not implement playback or recording.
class SourceSaveNativePersistence : public SourceSavePageServices
{
    SourceApplicationLiteralRef app_;
    const SourceBootConfigState &config_;
    sgn::PersistenceFileService &files_; // same retained host transport used by bootstrap/load
    SourceApplicationCore *core_{};      // NEW borrow, never a copied frontend/world
    NativeSourceMenuCrt *crt_{};         // same App/platform and actual FILE/descriptor owner
    // NEW ownership guard and borrow of the existing Core menu subobject.
    // Never attaches a menu or initializes source/render state.
    SourceSavePageDrawChildren &draw_children(FrontendState &);
    void publish_card_path_4401e0();
    // One440660/4403B0 route with source strict_size0: row supplies627B54/128,
    // 458E30 supplies its captured destination/count. No content validation here.
    std::uint32_t read_replay_4403b0(FrontendState &, std::uint32_t destination, std::uint32_t requested,
                                     std::span<const std::uint8_t> written_name);
    std::uint32_t read_replay_440660(FrontendState &, std::uint32_t destination, std::uint32_t requested,
                                     std::uint32_t replay);
    std::uint32_t write_replay_4404e0(FrontendState &, std::uint32_t source, std::uint32_t requested,
                                      std::span<const std::uint8_t> written_name);
    std::uint32_t write_replay_4406a0(FrontendState &, std::uint32_t source, std::uint32_t requested,
                                      std::uint32_t replay);

  public:
    SourceSaveNativePersistence(SourceApplicationLiteralRef, const SourceBootConfigState &,
                                sgn::PersistenceFileService &);
    // Core, CRT and files outlive this service. Standalone leaf construction
    // above retains its API; invoking unbound row/load/save/drawing is an explicit gap.
    SourceSaveNativePersistence(SourceApplicationCore &, NativeSourceMenuCrt &, sgn::PersistenceFileService &);
    SourceApplicationLiteralRef application_literals() const final;
    void mark_cards_changed_4400b0();
    void reset_selected_card_440140(); // also literal440150
    std::uint32_t changed_cards_440170() final;
    void select_card_4402f0(std::uint32_t) final;
    std::uint32_t card_capacity_440200(std::uint32_t &) final;
    std::uint32_t setup_status_440320() final;
    std::uint32_t replay_status_4405c0(std::uint32_t replay) final;
    std::uint32_t replay_blocks_4405b0(std::uint32_t value) final;
    void replay_row_457b36(FrontendState &, std::uint32_t page, std::uint32_t record) final;
    std::uint32_t replay_load_458e30(FrontendState &) final;
    std::uint32_t replay_save_458e10(FrontendState &) final;
    PersistenceServices &persistence() final;
    std::uint32_t format_card_440180() final;
    void sleep_source_458de9(std::uint32_t) final;
    void draw_cards_4581b0(FrontendState &, std::uint32_t page) final;
    void draw_progress_458730(FrontendState &, std::uint32_t page) final;
    void draw_confirmation_459070(FrontendState &, std::uint32_t page, std::uint32_t text) final;
    void replay_background_4580fc(FrontendState &, std::uint32_t mode) final;
};
} // namespace sgn::retail
