#pragma once
#include "sgn/source_allocation_bank.hpp"
#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <stdexcept>
#include <string_view>

namespace sgn::retail
{
class RaceSession;
class RaceSessionPlatform;
class SourceBootConfigError final : public std::runtime_error
{
  public:
    const std::uint32_t source_site;
    SourceBootConfigError(std::uint32_t site, const char *message) : std::runtime_error(message), source_site(site) {}
};
struct SourceBootRawFree
{
    void operator()(std::uint8_t *) const noexcept;
};
using SourceBootRawAllocation = std::unique_ptr<std::uint8_t, SourceBootRawFree>;
// This is the original separate malloc-owned WAD cache, NOT the42E8D0 bank.
struct SourceBootFileState
{
    std::array<char, 256> searched_name{}; // 61B758..61B857
    std::array<char, 32> archive_name{};   // 61B858..61B877
    SourceBootRawAllocation archive;       // 61B878
    std::uint32_t archive_size{};          // 61B87C
    std::size_t readable_prefix{};         // host initialized-byte tracking; malloc is NOT zeroed
};
struct SourceBootSearchBoundary
{
    SourceAllocationRef allocation; // weak identity; retained after source release
    std::uint32_t offset{};
    bool is_null{true}; // semantic source null, not an extra source flag
};
struct SourceBootConfigState
{
    SourceBootConfigState();
    SourceBootConfigState(const SourceBootConfigState &) = delete;
    SourceBootConfigState &operator=(const SourceBootConfigState &) = delete;
    SourceBootConfigState(SourceBootConfigState &&) = delete;
    SourceBootConfigState &operator=(SourceBootConfigState &&) = delete;
    std::array<char, 256> game_location{};         // 61B640..61B73F
    std::array<char, 128> base_path{};             // 48489C, pinned PE root spelling
    std::array<char, 128> graphics_path{};         // 48491C, mutable normal root;42D34F/42E40F
    std::array<char, 128> maximum_graphics_path{}; // 48499C, distinct PE owner; neither call rewrites it
    std::array<char, 128> sound_path{};            // 484A1C, PE D:\Game\Sound
    std::array<char, 32> level_script{};           // 589604..589623
    SourceBootSearchBoundary boundary;             // 589624, one source pointer representation
    std::uint32_t display_width{}, display_height{}, display_depth{};                // 576BCC/D0/D4
    std::uint32_t device_index{}, driver_index{}, uv_fix{}, display_boot_graphics{}; // 60F2A0/A4/A8,60DFA0
    SourceBootFileState files;
};
struct SourceBootAudio
{
    virtual ~SourceBootAudio() = default;
    virtual void configure_460c20(std::uint32_t master, std::uint32_t effects, std::uint32_t music,
                                  std::uint32_t rate) = 0;
};
struct SourceBootConfigWorld
{
    SourceBootConfigState &config;
    std::uint32_t &control_method;           // actual Session.control_kind/60F018
    std::uint32_t &disable_frame_correction; // actual source_scheduler.fixed_mode_a/60F2AC
    SourceBootAudio &audio;
};
// Real loose-file/CRT-heap boundary. Size0 includes failed fopen and empty file.
// Read receives the ORIGINAL signed count word (<=0 requests2MiB in44AFA0).
// copied is updated for actual written prefix even if a host bound is reached.
struct SourceBootFilePlatform
{
    virtual ~SourceBootFilePlatform() = default;
    virtual std::uint32_t size_loose_44af60(std::string_view path) = 0;
    virtual std::uint32_t read_loose_44afa0(std::string_view path, std::uint32_t count,
                                            std::span<std::uint8_t> destination, std::size_t &copied) = 0;
    // New native transport for the same file route with a lazy bank destination.
    // Legacy/custom platforms explicitly reject an unimplemented bank route.
    virtual std::uint32_t read_loose_bank_44afa0(std::string_view, std::uint32_t, SourceAllocationBank &,
                                                 const SourceAllocationRef &, std::size_t &copied);
    virtual SourceBootRawAllocation allocate_wad_raw(std::uint32_t bytes) = 0;
};
// Same malloc-owned WAD cache for44B1D0 and the reached42E749 call.
// A null pointer retains name/size; no allocation-bank or material reset.
void release_source_archive_44b1a0(SourceBootFileState &) noexcept;
void release_platform_source_archive_42e749(RaceSessionPlatform &);
std::uint32_t source_boot_file_size_44b3b0(SourceBootConfigState &, SourceBootFilePlatform &, std::string_view);
std::uint32_t source_boot_file_read_44b260(SourceBootConfigState &, SourceBootFilePlatform &, std::string_view,
                                           std::uint32_t, std::span<std::uint8_t>);
// NEW initialized-byte observation, separate from source EAX (WAD member size
// or loose fread result). Updated after actual stores, including before throws.
std::uint32_t source_boot_file_read_44b260(SourceBootConfigState &, SourceBootFilePlatform &, std::string_view,
                                           std::uint32_t, std::span<std::uint8_t>, std::size_t &initialized_prefix);
void apply_source_boot_settings_42d2e0(SourceBootConfigWorld, SourceAllocationBank &, const SourceAllocationRef &);
// Same44B260 route/cache, actual nullable destination owner, no eager bytes().
std::uint32_t source_boot_file_read_bank_44b260(SourceBootConfigState &, SourceBootFilePlatform &,
                                                SourceAllocationBank &, std::string_view, std::uint32_t,
                                                const SourceAllocationRef &, std::size_t &copied);
void initialize_source_boot_config_42d660(SourceBootConfigWorld, SourceAllocationBank &, SourceBootFilePlatform &);
// Explicit pre-Session binding to the actual platform CFG, control, scheduler,
// audio scalar owners and already-initialized shared bank. No native audio access.
void initialize_platform_source_boot_config_42d660(RaceSessionPlatform &, SourceBootFilePlatform &);
// Compatibility entry: delegates once to the same platform binding.
void initialize_session_source_boot_config_42d660(RaceSession &, SourceBootFilePlatform &);

// Explicit source working-directory binding; never defaults to the OLD task cwd.
// Windows UTF-16 path transport is new host I/O; source path spelling/routing is retained.
class NativeSourceBootFilePlatform final : public SourceBootFilePlatform
{
    std::filesystem::path source_working_directory_;
    const SourceBootConfigState *relocated_source_state_{}; // Borrowed, never a copied root owner.
    std::filesystem::path resolve(std::string_view) const;

  public:
    explicit NativeSourceBootFilePlatform(std::filesystem::path source_working_directory);
    // New host path transport. Preserve source spelling through44B050/WAD
    // lookup; remap the live source base only at the native filesystem edge.
    // The configuration must outlive this backend.
    NativeSourceBootFilePlatform(std::filesystem::path local_retail_directory,
                                 const SourceBootConfigState &source_state);
    std::uint32_t size_loose_44af60(std::string_view) override;
    // NEW HOST (audio-r36): the same relocated native path the loose-file edge opens, for native file APIs
    // that take a path (music fullpath/drive/MMIO). No source spelling or WAD routing change.
    std::filesystem::path native_path(std::string_view spelling) const
    {
        return resolve(spelling);
    }
    std::uint32_t read_loose_44afa0(std::string_view, std::uint32_t, std::span<std::uint8_t>, std::size_t &) override;
    std::uint32_t read_loose_bank_44afa0(std::string_view, std::uint32_t, SourceAllocationBank &,
                                         const SourceAllocationRef &, std::size_t &) override;
    SourceBootRawAllocation allocate_wad_raw(std::uint32_t) override;
};
} // namespace sgn::retail
