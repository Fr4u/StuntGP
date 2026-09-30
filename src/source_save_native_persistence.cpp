#include "sgn/source_save_native_persistence.hpp"
#include "sgn/source_script_path_formatter.hpp"
#include "sgn/source_application_core.hpp"
#include "sgn/source_native_crt.hpp"
#include "sgn/source_save_page_draw.hpp"
#include "sgn/retail_frontend_page_pool.hpp"
#include "sgn/retail_persistence.hpp"
#include "sgn/race_session.hpp"
#include "sgn/host_profiler.hpp"
#include <chrono>
#include <string_view>
#include <algorithm>
#include <bit>
#include <charconv>
#include <stdexcept>
#include <string>
#include <utility>
#include <windows.h>
namespace sgn::retail
{
namespace
{
// NEW host mapping of completed source formatter output for440603/4403FC/44052C.
std::string_view card_basename(std::span<const std::uint8_t> path, std::span<const std::uint8_t> local,
                               std::size_t initialized)
{
    if (initialized == 0 || initialized > local.size() || local[initialized - 1u] != 0)
        throw std::logic_error("replay path formatter has no completed NUL-inclusive output");
    const auto end = std::find(path.begin(), path.end(), std::uint8_t{0});
    const auto prefix = static_cast<std::size_t>(end - path.begin()), written = initialized - 1u;
    if (end == path.end() || written <= prefix + 1u || local[prefix] != 0x5cu ||
        !std::equal(path.begin(), end, local.begin()))
        throw std::runtime_error("formatted replay path leaves the admitted card mapping");
    return {reinterpret_cast<const char *>(local.data() + prefix + 1u), written - prefix - 1u};
}
} // namespace
SourceSaveNativePersistence::SourceSaveNativePersistence(SourceApplicationLiteralRef app,
                                                         const SourceBootConfigState &config,
                                                         sgn::PersistenceFileService &files)
    : app_(std::move(app)), config_(config), files_(files)
{
    (void)app_.save_card_storage();
}
SourceSaveNativePersistence::SourceSaveNativePersistence(SourceApplicationCore &core, NativeSourceMenuCrt &crt,
                                                         sgn::PersistenceFileService &files)
    : SourceSaveNativePersistence(core.platform()->source_application_literals(), core.platform()->state().boot_config,
                                  files)
{
    crt.require_platform(*core.platform());
    core_ = &core;
    crt_ = &crt;
}
SourceApplicationLiteralRef SourceSaveNativePersistence::application_literals() const
{
    return app_;
}
void SourceSaveNativePersistence::mark_cards_changed_4400b0()
{
    app_.save_card_storage().changed_5ff8bc = 1;
}
void SourceSaveNativePersistence::reset_selected_card_440140()
{
    app_.save_card_storage().selected_card_4798bc = 0xffffffffu;
}
std::uint32_t SourceSaveNativePersistence::changed_cards_440170()
{
    auto &state = app_.save_card_storage();
    const auto value = state.changed_5ff8bc;
    state.changed_5ff8bc = 0;
    return value;
}
void SourceSaveNativePersistence::select_card_4402f0(std::uint32_t card)
{
    app_.save_card_storage().selected_card_4798bc = card;
}
void SourceSaveNativePersistence::publish_card_path_4401e0()
{
    auto &state = app_.save_card_storage();
    const auto end = std::find(config_.base_path.begin(), config_.base_path.end(), '\0');
    if (end == config_.base_path.end())
        throw std::runtime_error("4401E0 source base path leaves its existing owner");
    // Pinned479A18 is %s\\save\\card%02d; fixed conversion, not a general replacement CRT.
    std::array<char, 16> number{};
    const auto converted = std::to_chars(number.data(), number.data() + number.size(),
                                         std::bit_cast<std::int32_t>(state.selected_card_4798bc));
    if (converted.ec != std::errc{})
        throw std::runtime_error("4401E0 card formatting failed");
    std::string card(number.data(), converted.ptr);
    if (card.size() < 2)
        card.insert(card.begin(), '0');
    std::string path(config_.base_path.begin(), end);
    path += "\\save\\card";
    path += card;
    if (path.size() + 1 > state.path_5ff4bc.size())
        throw std::runtime_error("4401E0 path exceeds source scratch");
    std::copy(path.begin(), path.end(), state.path_5ff4bc.begin());
    state.path_5ff4bc[path.size()] = '\0';
    // Only the initialized prefix changes, like46280C; trailing source bytes survive.
}
std::uint32_t SourceSaveNativePersistence::card_capacity_440200(std::uint32_t &free)
{
    publish_card_path_4401e0();
    const auto card = app_.save_card_storage().selected_card_4798bc;
    free = 0;
    if (!files_.card_directory_exists(card))
        return 1;
    const auto used = files_.source_card_used_bytes(card, [this](std::uint32_t e) { app_.map_os_error_4699d7(e); });
    if (std::bit_cast<std::int32_t>(used) < 0)
        return 9;
    free = 0x40000u - used;
    return 0; // source virtual card256KiB, not OS disk capacity
}
std::uint32_t SourceSaveNativePersistence::setup_status_440320()
{
    publish_card_path_4401e0();
    const auto card = app_.save_card_storage().selected_card_4798bc;
    if (!files_.card_directory_exists(card))
        return 1;
    return files_.source_setup_exists(card) ? 4u : 6u;
}
std::uint32_t SourceSaveNativePersistence::replay_blocks_4405b0(std::uint32_t value)
{
    return value; // 4405B0..4405B2: MOV EAX,ECX;RET. No state access or division.
}
std::uint32_t SourceSaveNativePersistence::replay_status_4405c0(std::uint32_t replay)
{
    // 4405CB captures ECX in EDI before the selected-card path is published.
    publish_card_path_4401e0();
    auto &state = app_.save_card_storage();
    const auto card = state.selected_card_4798bc;
    if (!files_.card_directory_exists(card))
        return 1;                         // 4405DC..4405EF, before formatting.
    std::array<std::uint8_t, 1024> local; // Actual uninitialized [EBP-400] caller storage.
    SourceScriptPathCall call;
    const auto path = std::span<const std::uint8_t>{reinterpret_cast<const std::uint8_t *>(state.path_5ff4bc.data()),
                                                    state.path_5ff4bc.size()};
    const std::array<std::uint32_t, 2> arguments{0x5ff4bcu, replay};
    (void)format_save_ui_46280c(config_, app_, state.replay_path_format_479a74, arguments, local, 0x440603, call,
                                SourceSaveStringBorrow{4, path});
    // NEW host mapping: retain the existing private root/card policy. Borrow only
    // the basename from the actual completed output; do not format it a second time.
    const auto name = card_basename(path, local, call.initialized_prefix);
    return files_.source_replay_exists(card, name) ? 4u : 6u; // 44060C..44061F: attributes only.
}
std::uint32_t SourceSaveNativePersistence::read_replay_4403b0(FrontendState &f, std::uint32_t destination,
                                                              std::uint32_t requested,
                                                              std::span<const std::uint8_t> written_name)
{
    const auto card = app_.save_card_storage().selected_card_4798bc; // 4403BC captured before4401E0
    publish_card_path_4401e0();
    if (!files_.card_directory_exists(card))
        return 1; // 4403CF..E3 before full path formatter
    auto &state = app_.save_card_storage();
    const auto path = std::span<const std::uint8_t>{reinterpret_cast<const std::uint8_t *>(state.path_5ff4bc.data()),
                                                    state.path_5ff4bc.size()};
    std::array<std::uint8_t, 1024> local;
    SourceScriptPathCall call;                             // actual uninitialized caller local
    const std::array<std::uint32_t, 2> args{0x5ff4bcu, 0}; // second word is the borrowed440660 stack identity
    (void)format_save_ui_46280c(config_, app_, state.file_path_format_479a50, args, local, 0x4403fc, call, {4, path},
                                {8, written_name});
    const auto name = card_basename(path, local, call.initialized_prefix);
    const auto native_path = files_.source_replay_native_path(card, name); // NEW private-root/encoding transport only
    (void)crt_->file_size_440480(native_path, core_->time());              // 440405 always, including strict_size0
    const auto file = crt_->fopen_binary_read_462ac3(native_path, state.read_binary_mode_479a58); // 44042F
    if (file == 0)
        return 6;
    const auto count =
        crt_->read_frontend_replay_bytes(f, destination, requested, file); // 440450, captured destination/1/count
    (void)crt_->close_file_462a4d(file); // 440458 always after returned fread, result intentionally ignored
    return count == requested ? 0u : 9u; // 440462..6A: captured fread count, not fclose status
}
std::uint32_t SourceSaveNativePersistence::read_replay_440660(FrontendState &f, std::uint32_t destination,
                                                              std::uint32_t requested, std::uint32_t replay)
{
    std::array<std::uint8_t, 64> name;
    SourceScriptPathCall call; // actual [EBP-40], no clear
    const std::array<std::uint32_t, 1> args{replay};
    (void)format_save_ui_46280c(config_, app_, app_.save_card_storage().ghost_name_format_479a88, args, name, 0x44067e,
                                call);
    return read_replay_4403b0(f, destination, requested,
                              std::span<const std::uint8_t>{name}.first(call.initialized_prefix));
}
void SourceSaveNativePersistence::replay_row_457b36(FrontendState &f, std::uint32_t page, std::uint32_t record)
{
    if (!core_ || !crt_)
        throw FormatError("457B36 requires the retained application Core and native CRT");
    crt_->require_platform(*core_->platform());
    if (&f != &core_->frontend())
        throw FormatError("457B36 requires the exact Core frontend object");
    // NEW host admission for the caller's existing eight rows, not a name-length limit.
    if (record > 7u * 0x30u || record % 0x30u != 0)
        throw FormatError("457B36 row offset leaves its source caller domain");
    const auto replay = f.source.source_word(0x5e3c2c); // 457B36 captured before440660
    if (read_replay_440660(f, 0x627b54, 128, replay) != 0)
    {
        auto row =
            frontend_page_object_bytes(std::as_writable_bytes(std::span{f.source.page_pool}), page, record + 0x70u);
        persistence_word(std::span<std::uint8_t>{reinterpret_cast<std::uint8_t *>(row.data()), row.size()},
                         record + 0x6cu, 1); // 457B50
        return;
    }
    const auto time = format_source_time_43d550(f, f.source.source_word(0x627b70)); // 457B59..5F
    auto page_bytes = frontend_page_object_bytes(std::as_writable_bytes(std::span{f.source.page_pool}), page, 0x200);
    auto output =
        std::span<std::uint8_t>{reinterpret_cast<std::uint8_t *>(page_bytes.data()), page_bytes.size()}.subspan(record +
                                                                                                                0x70u);
    // The source scan may reach retained bytes beyond the freshly read header.
    // Borrow to the end of the actual627A54..627C53 owner; never append a NUL.
    const auto name = f.source.source_bytes(0x627b80, 0x627c54u - 0x627b80u);
    const std::array<std::uint32_t, 2> args{0x627b80u, 0x5ff3a8u};
    SourceScriptPathCall call;
    (void)format_save_ui_46280c(config_, app_, f.source.source_bytes(0x488ca4, 8), args, output, 0x457b78, call,
                                {4, name}, {8, time}); // 457B73, write directly into existing page; no32-byte clamp
}
std::uint32_t SourceSaveNativePersistence::replay_load_458e30(FrontendState &f)
{
    // NEW ownership admission before source effects; neither constructs a world nor attaches a menu.
    if (!core_ || !crt_)
        throw FormatError("458E30 requires the retained application Core and native CRT");
    crt_->require_platform(*core_->platform());
    if (&f != &core_->frontend())
        throw FormatError("458E30 requires the exact Core frontend object");
    const auto replay = f.source.source_word(0x5e3c2c);      // 458E30 EAX, captured first
    const auto requested = f.source.source_word(0x5e3c34);   // 458E35 EDX, captured second
    const auto destination = f.source.source_word(0x5e3c30); // 458E3B ECX, captured third
    // 458E42 propagates440660 status. Zero count still reaches path/search/open/read/close.
    // The scoped Native borrow preserves reached partial writes in the existing frontend owner.
    return read_replay_440660(f, destination, requested, replay);
}
std::uint32_t SourceSaveNativePersistence::write_replay_4404e0(FrontendState &f, std::uint32_t source,
                                                               std::uint32_t requested,
                                                               std::span<const std::uint8_t> written_name)
{
    const auto card = app_.save_card_storage().selected_card_4798bc; // 4404EE before4401E0
    publish_card_path_4401e0();
    if (!files_.card_directory_exists(card))
        return 1; // 4404FF..440513
    auto &state = app_.save_card_storage();
    const auto path = std::span<const std::uint8_t>{reinterpret_cast<const std::uint8_t *>(state.path_5ff4bc.data()),
                                                    state.path_5ff4bc.size()};
    std::array<std::uint8_t, 1024> local;
    SourceScriptPathCall call;                             // actual caller local, no clear
    const std::array<std::uint32_t, 2> args{0x5ff4bcu, 0}; // second word is the borrowed4406A0 stack identity
    (void)format_save_ui_46280c(config_, app_, state.write_path_format_479a68, args, local, 0x44052c, call, {4, path},
                                {8, written_name});
    const auto name = card_basename(path, local, call.initialized_prefix);
    const auto native_path = files_.source_replay_native_path(card, name); // existing private-root/encoding transport
    const auto existing = crt_->file_size_440480(native_path, core_->time()); // 440535 always before open/truncate
    // 44053C signed JL admits every negative low size, not onlyFFFFFFFF.
    if (std::bit_cast<std::int32_t>(existing) >= 0 && existing != requested)
        return 9;
    const auto file = crt_->fopen_binary_write_462ac3(native_path, state.write_binary_mode_479a70); // 44055C
    if (file == 0)
        return 6;
    const auto count = crt_->write_frontend_replay_bytes(f, source, requested, file); // 44057D source/1/count
    (void)crt_->close_file_462a4d(file); // 440585 close result intentionally ignored
    return count == requested ? 0u : 9u; // 44058F..97 uses captured fwrite result
}
std::uint32_t SourceSaveNativePersistence::write_replay_4406a0(FrontendState &f, std::uint32_t source,
                                                               std::uint32_t requested, std::uint32_t replay)
{
    std::array<std::uint8_t, 64> name;
    SourceScriptPathCall call; // actual [EBP-40], no clear
    const std::array<std::uint32_t, 1> args{replay};
    (void)format_save_ui_46280c(config_, app_, app_.save_card_storage().write_ghost_format_479a98, args, name, 0x4406be,
                                call);
    return write_replay_4404e0(f, source, requested,
                               std::span<const std::uint8_t>{name}.first(call.initialized_prefix));
}
std::uint32_t SourceSaveNativePersistence::replay_save_458e10(FrontendState &f)
{
    // NEW ownership admission before source effects; borrow the exact Core frontend.
    if (!core_ || !crt_)
        throw FormatError("458E10 requires the retained application Core and native CRT");
    crt_->require_platform(*core_->platform());
    if (&f != &core_->frontend())
        throw FormatError("458E10 requires the exact Core frontend object");
    const auto replay = f.source.source_word(0x5e3c2c);    // 458E10 EAX first
    const auto requested = f.source.source_word(0x5e3c34); // 458E15 EDX second
    const auto source = f.source.source_word(0x5e3c30);    // 458E1B ECX third
    // Count0 still formats, searches, opens/truncates and closes as in retail.
    return write_replay_4406a0(f, source, requested, replay); // 458E22 propagates4406A0 status
}
SourceSavePageDrawChildren &SourceSaveNativePersistence::draw_children(FrontendState &f)
{
    if (!core_ || !crt_)
        throw FormatError("save drawing requires the retained application Core and native CRT");
    crt_->require_platform(*core_->platform());
    if (&f != &core_->frontend())
        throw FormatError("save drawing requires the exact Core frontend object");
    // Core.menu() checks attachment and only returns its existing menu. The
    // upcast selects its nonvirtual Draw methods, not this Save IO interface.
    return core_->menu();
}
void SourceSaveNativePersistence::draw_cards_4581b0(FrontendState &f, std::uint32_t page)
{
    draw_children(f).draw_cards_4581b0(f, page);
}
void SourceSaveNativePersistence::draw_progress_458730(FrontendState &f, std::uint32_t page)
{
    draw_children(f).draw_progress_458730(f, page);
}
void SourceSaveNativePersistence::draw_confirmation_459070(FrontendState &f, std::uint32_t page, std::uint32_t text)
{
    draw_children(f).draw_confirmation_459070(f, page, text);
}
void SourceSaveNativePersistence::replay_background_4580fc(FrontendState &f, std::uint32_t mode)
{
    draw_children(f).replay_background_4580fc(f, mode);
}
PersistenceServices &SourceSaveNativePersistence::persistence()
{
    (void)app_.save_card_storage();
    return files_;
}
std::uint32_t SourceSaveNativePersistence::format_card_440180()
{
    publish_card_path_4401e0();
    const auto card = app_.save_card_storage().selected_card_4798bc;
    if (files_.card_directory_exists(card))
        return 7;
    return files_.source_create_card(
        card, [this](std::uint32_t e) { app_.map_os_error_4699d7(e); }); // singlemkdir, no recursive creation/deletion
}
void SourceSaveNativePersistence::sleep_source_458de9(std::uint32_t milliseconds)
{
    (void)app_.save_card_storage();
    // NEW HOST (R35) diagnostics only: the source's own wait, unchanged, named for the frame trace.
    const auto slept_from = std::chrono::steady_clock::now();
    Sleep(milliseconds);
    sgn::host_profile::add(
        "source.sleep_458de9",
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - slept_from).count());
}
} // namespace sgn::retail
