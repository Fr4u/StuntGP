#pragma once
#include "sgn/source_crt_descriptor_io.hpp"
#include "sgn/source_crt_read.hpp"
#include "sgn/source_crt_buffer.hpp"
#include "sgn/source_crt_file_flush.hpp"
#include "sgn/source_crt_close.hpp"
#include "sgn/source_crt_binary_open.hpp"
#include "sgn/source_menu_crt_exit.hpp"
#include "sgn/source_application_literals.hpp"
#include "sgn/source_crt_fraction_storage.hpp"
#include <memory>
#include <span>
namespace sgn::retail
{
class RaceSessionPlatform;
class SourceColdTimePlatform;
// One process-lifetime CRT owner; construct once beside App, before source startup.
// Host allocations/handle relocation are new Windows x64 transport, not game state defaults.
class NativeSourceMenuCrt final : public SourceCrtStartupServices,
                                  public SourceCrtFileFlushServices,
                                  public SourceCrtExitMemory,
                                  public SourceCrtReadFileImport,
                                  public SourceCrtBufferServices,
                                  public SourceCrtCloseImports,
                                  public SourceCrtBinaryOpenImport
{
    struct Impl;
    std::unique_ptr<Impl> p_;

  public:
    // Standalone provider: owns its local PE-zero 636840 slot.
    explicit NativeSourceMenuCrt(SourceApplicationLiteralRef);
    // NEW host borrow: retains the existing platform/world, never copies
    // or resets its SourceColdTimeState::code_page (source 636840).
    // Pass the actual Core platform, bound to the same live App. Both
    // objects must be used on their owner thread; App outlives this CRT.
    NativeSourceMenuCrt(SourceApplicationLiteralRef, const std::shared_ptr<RaceSessionPlatform> &existing_platform);
    ~NativeSourceMenuCrt();
    NativeSourceMenuCrt(const NativeSourceMenuCrt &) = delete;
    NativeSourceMenuCrt &operator=(const NativeSourceMenuCrt &) = delete;
    // NEW HOST identity guard: same retained Core platform, live App and owner thread.
    void require_platform(const RaceSessionPlatform &) const;
    // Borrow actual process CRT state only after completed startup. Same owner
    // is also exposed through source word/byte memory; no per-receiver copy.
    SourceCrtFractionStorage &fraction_storage_46291a();
    void initialize(); // 469792 then462DCD; one actual startup, never in an exit callback.
    // Actual retained CRT open for source replay mode rb NUL/share40 only.
    // Returns0 FILE on source failure, keeping errno/doserrno effects.
    std::uint32_t fopen_binary_read_462ac3(std::span<const std::uint8_t> path, std::span<const std::uint8_t> mode);
    // Separate actual wb caller; same FILE/descriptor/handle and mutable umask owner.
    std::uint32_t fopen_binary_write_462ac3(std::span<const std::uint8_t> path, std::span<const std::uint8_t> mode);
    // NEW Windows ABI transport; descriptor table publication remains source46B83A.
    std::uint32_t create_file_a(std::span<const std::uint8_t> path, std::uint32_t access, std::uint32_t share,
                                bool inherit, std::uint32_t disposition, std::uint32_t attributes) final;
    // Same CRT/App and actual platform time state. Pattern is a bounded
    // borrow containing its source NUL; time is the existing Core service.
    // Full find record: attributes,+4/+8/+C timestamps,+10/+14 size,
    // name at+18 through its NUL. No destination or errno clear.
    // Success yields a positive find-domain token, failure FFFFFFFF.
    std::uint32_t find_first_46355b(std::span<const std::uint8_t> pattern, std::span<std::uint8_t, 0x120> destination,
                                    SourceColdTimePlatform &time);
    // Only a live find token from this CRT is admitted. Success consumes
    // it; failed FindClose preserves it and writes errno22 only.
    std::uint32_t find_close_4634d8(std::uint32_t find_identity);
    std::uint32_t file_size_440480(std::span<const std::uint8_t> pattern, SourceColdTimePlatform &time);
    // NEW HOST scoped borrow of the existing frontend save scratch. Calls
    // the real463729 with destination627B54,size1,count128 on this CRT.
    // The borrow ends before return/throw; partial bytes remain in Frontend.
    std::uint32_t read_frontend_replay_header(FrontendState &, std::uint32_t file);
    // Same scoped transport for440450: existing scratch and both adjacent
    // replay payload areas5E3C48..5F26A7 in this Frontend, with no copied bank.
    // Calls463729(destination,1,count,file); destination access is deferred
    // until reached by fread. In particular count0 never dereferences it.
    std::uint32_t read_frontend_replay_bytes(FrontendState &, std::uint32_t destination, std::uint32_t count,
                                             std::uint32_t file);
    // Same synchronous owners for44057D: calls463811(source,1,count,file).
    // No payload staging, preclear, persistent borrow, or source read for count0.
    std::uint32_t write_frontend_replay_bytes(FrontendState &, std::uint32_t source, std::uint32_t count,
                                              std::uint32_t file);
    SourceCrtExitWords exit_words();
    void exit_menu(FrontendState &, SourceApplicationShutdownServices &, std::uint32_t status);
    std::uint32_t word(std::uint32_t) final;
    std::uint8_t byte(std::uint32_t) final;
    void word(std::uint32_t, std::uint32_t) final;
    void byte(std::uint32_t, std::uint8_t) final;
    std::uint32_t read_word(std::uint32_t, std::uint32_t) final;
    void write_word(std::uint32_t, std::uint32_t, std::uint32_t) final;
    std::uint32_t flush_file_466564(std::uint32_t) final;
    std::uint32_t close_file_462a4d(std::uint32_t file);
    // Actual CloseHandle/SetStdHandle use this CRT's existing native map.
    // Successful closes tombstone their token; failed closes retain it.
    bool close_native_handle(std::uint32_t) final;
    bool set_std_handle(std::uint32_t, std::uint32_t) final;
    // NEW paired host release, exact allocated base only; null is a no-op.
    void free_462f9e(std::uint32_t) final;
    std::uint32_t write_descriptor_4695e5(std::uint32_t, std::uint32_t, std::uint32_t) final;
    // Same descriptor/handle/error owners; text conversion is not implied.
    std::uint32_t read_descriptor_binary_4690b9(std::uint32_t, std::uint32_t, std::uint32_t) final;
    void allocate_buffer_46b45e(std::uint32_t file);
    std::uint32_t fill_buffer_binary_468fe0(std::uint32_t file);
    bool read_file(std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t &) final;
    std::uint32_t commit_descriptor_46b968(std::uint32_t) final;
    bool write_file(std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t &) final;
    bool write_file_local(std::uint32_t, std::span<const std::uint8_t>, std::uint32_t &) final;
    bool flush_file_buffers(std::uint32_t) final;
    std::uint32_t set_file_pointer(std::uint32_t, std::uint32_t, std::uint32_t) final;
    std::uint32_t last_error() final;
    void map_os_error_4699d7(std::uint32_t) final;
    std::uint32_t allocate_462fcd(std::uint32_t) final;
    std::uint32_t calloc_46d0c0(std::uint32_t, std::uint32_t) final;
    void fatal_46412d(std::uint32_t) final;
    SourceCrtStartupInfo get_startup_info_a() final;
    std::uint32_t get_std_handle(std::uint32_t) final;
    std::uint32_t get_file_type(std::uint32_t) final;
    void set_handle_count(std::uint32_t) final;
    void invoke_startup_callback(std::uint32_t) final;
    void invoke_registered_callback(std::uint32_t) final;
    void close_streams_46d13d() final;
    // NEW HOST: only the default-ANSI request (-3) maps ACP65001 to the
    // actual system locale's available legacy ANSI page. No UTF-8 byte table,
    // fixed1252 fallback, process ACP change, or636840 ownership change.
    void set_multibyte_codepage_46c823(std::uint32_t) final;
};
} // namespace sgn::retail
