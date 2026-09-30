#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace sgn::retail
{
inline constexpr std::size_t persistence_size = 0x211c;
inline constexpr std::size_t persistence_body_size = persistence_size - 8;
// Storage only. The original cold/default functions establish game defaults.
struct alignas(4) PersistenceBank
{
    std::array<std::uint8_t, persistence_size> bytes{};
};
struct alignas(4) PersistenceReadScratch
{
    std::array<std::uint8_t, persistence_size> bytes{};
};
struct PersistenceHeader
{
    std::uint32_t size{}, checksum{};
};
static_assert(sizeof(PersistenceHeader) == 8 && sizeof(PersistenceBank) == persistence_size);
// saved_body borrows 5E167C..5E378F from the frontend cold-clear suffix.
// scratch is 625938..627A53, a distinct source working owner.
struct PersistenceView
{
    std::span<std::uint8_t, persistence_size> live;
    std::span<std::uint8_t, persistence_body_size> saved_body;
    std::span<std::uint8_t, persistence_size> scratch;
};
std::uint32_t persistence_word(std::span<const std::uint8_t>, std::size_t offset);
void persistence_word(std::span<std::uint8_t>, std::size_t offset, std::uint32_t value);
PersistenceHeader persistence_header(std::span<const std::uint8_t, persistence_size>);
std::span<std::uint8_t, 52> persistence_unlocks(PersistenceView) noexcept;                 // 5DF568
std::span<std::uint8_t, 0x160> persistence_campaign_slot(PersistenceView, unsigned slot);  // 5E047C,8 slots
std::span<std::uint8_t, 32> persistence_campaign_records(PersistenceView, unsigned track); // 5E0F7C,25 records

enum class PersistenceFileKind : std::uint32_t
{
    setup,
    detail,
    language
};
struct PersistenceFile
{
    PersistenceFileKind kind{};
    std::uint32_t card{};
};
enum class PersistenceAccess
{
    read_binary,
    write_binary,
    read_text,
    write_text
};
using PersistenceFileHandle = std::uint64_t; // 0 is fopen failure, never a pointer
enum class PersistenceStatus : std::uint32_t
{
    success = 0,
    no_card = 1,
    open_failed = 6,
    invalid_data = 9
};
// Mandatory synchronous file boundary. Source CRT transfer semantics are the
// contract; the modern host's durable commit errors may throw before success.
struct PersistenceServices
{
    virtual ~PersistenceServices() = default;
    virtual bool card_directory_exists(std::uint32_t card) = 0;
    virtual std::int32_t file_size(PersistenceFile) = 0; //-1 missing/directory
    virtual PersistenceFileHandle open(PersistenceFile, PersistenceAccess) = 0;
    virtual std::size_t read(PersistenceFileHandle, std::span<std::uint8_t>) = 0;
    virtual std::size_t write(PersistenceFileHandle, std::span<const std::uint8_t>) = 0;
    virtual void scan_decimal(PersistenceFileHandle, std::int32_t &) = 0;
    virtual void print_decimal(PersistenceFileHandle, std::int32_t) = 0;
    virtual void close(PersistenceFileHandle) = 0;
};
std::uint32_t persistence_checksum(std::span<const std::uint8_t>) noexcept; // full43D5D0
void copy_persistence_comparison(PersistenceView) noexcept;                 // full43C400
bool persistence_changed(PersistenceView) noexcept;                         // full43C430/43C4E0 return
void clear_persistence_record_changes(PersistenceView) noexcept;            // full43BF30
void acknowledge_persistence(PersistenceView) noexcept;                     // full458EA0
void read_persistence_auxiliary(PersistenceView, PersistenceServices &);    // full43D280
void write_persistence_auxiliary(PersistenceView, PersistenceServices &);   // full43D370
PersistenceStatus read_persistence_setup(PersistenceView, PersistenceServices &, std::uint32_t card);  // 4403A0/B0
PersistenceStatus write_persistence_setup(PersistenceView, PersistenceServices &, std::uint32_t card); // 4404D0/E0
PersistenceStatus save_persistence(PersistenceView, PersistenceServices &, std::uint32_t card);        // full458E50
PersistenceStatus load_persistence(PersistenceView, PersistenceServices &, std::uint32_t card);        // full458EB0
// C-locale %d prefix semantics for the new host service; unsigned arithmetic
// preserves the original 32-bit CRT overflow behavior without signed C++ UB.
bool scan_persistence_decimal(std::span<const std::uint8_t>, std::int32_t &) noexcept;
std::string format_persistence_decimal(std::int32_t); // physical Windows text bytes
} // namespace sgn::retail
