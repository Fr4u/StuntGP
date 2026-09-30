#pragma once
#include "sgn/retail_persistence.hpp"
#include <functional>
#include <filesystem>
#include <memory>
#include <string_view>
#include <vector>
#include <cstdint>

namespace sgn
{
// New host policy: explicit private root, fixed source names, bounded files,
// no reparse destinations, durable temporary-file replacement on close.
// A commit failure throws; it must not be presented as a successful save.
class PersistenceFileService final : public retail::PersistenceServices
{
    struct Impl;
    std::unique_ptr<Impl> impl_;

  public:
    explicit PersistenceFileService(std::filesystem::path root);
    ~PersistenceFileService() override;
    PersistenceFileService(const PersistenceFileService &) = delete;
    PersistenceFileService &operator=(const PersistenceFileService &) = delete;
    void format_card(std::uint32_t card); // explicit new host action
    // NEW host transport used by recovered440180/440250; same root, no additional FILE registry.
    bool source_setup_exists(std::uint32_t); // 440380 attributes only, no size validation
    // 4405C0 supplies the basename already produced by46280C. This transport
    // only maps it under the same card root and checks440380 file attributes.
    bool source_replay_exists(std::uint32_t, std::string_view formatted_basename);
    // NEW host path transport only: same admitted basename/private card root,
    // NUL-inclusive bytes for current Win32 A file APIs. Lossy encodings are
    // rejected. No FILE registry, whole-file read, or source errno change.
    std::vector<std::uint8_t> source_replay_native_path(std::uint32_t, std::string_view formatted_basename);
    std::uint32_t source_create_card(std::uint32_t, const std::function<void(std::uint32_t)> &report_os_error);
    std::uint32_t source_card_used_bytes(std::uint32_t, const std::function<void(std::uint32_t)> &report_os_error);
    bool card_directory_exists(std::uint32_t) override;
    std::int32_t file_size(retail::PersistenceFile) override;
    retail::PersistenceFileHandle open(retail::PersistenceFile, retail::PersistenceAccess) override;
    std::size_t read(retail::PersistenceFileHandle, std::span<std::uint8_t>) override;
    std::size_t write(retail::PersistenceFileHandle, std::span<const std::uint8_t>) override;
    void scan_decimal(retail::PersistenceFileHandle, std::int32_t &) override;
    void print_decimal(retail::PersistenceFileHandle, std::int32_t) override;
    void close(retail::PersistenceFileHandle) override;
};
} // namespace sgn
