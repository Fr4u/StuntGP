#include "sgn/source_music_timer_entry_prefix.hpp"
#include "sgn/source_music_drive_type_prefix.hpp"
#include "sgn/source_music_disk_space_prefix.hpp"
#include "sgn/source_music_wave_entry.hpp"
#include "sgn/source_music_period_host.hpp"
#include "source_music_root_scratch.hpp"
#include "sgn/race_session_platform.hpp"
#include "sgn/race_session.hpp"
#include <thread>
#include <utility>

namespace sgn::retail
{
struct SourceMusicTimerCapsAccess::Scope
{
    const std::weak_ptr<RaceSessionPlatform> owner;
    const std::thread::id thread;
    bool active{true};
    explicit Scope(const std::shared_ptr<RaceSessionPlatform> &value) : owner(value), thread(std::this_thread::get_id())
    {
    }
};

SourceMusicTimerCapsAccess::SourceMusicTimerCapsAccess(std::shared_ptr<Scope> scope) : scope_(std::move(scope)) {}

std::shared_ptr<RaceSessionPlatform> SourceMusicTimerCapsAccess::checked_owner() const
{
    // The immutable thread identity is checked before the non-atomic active
    // flag or a weak-owner lock. Wrong-thread use cannot mutate source state.
    if (std::this_thread::get_id() != scope_->thread)
        throw std::logic_error("music timer capability access from a foreign thread");
    if (!scope_->active)
        throw std::logic_error("music timer capability access outside the synchronous call");
    auto owner = scope_->owner.lock();
    if (!owner)
        throw std::logic_error("music timer capability owner expired");
    owner->require_owner_thread();
    return owner;
}

std::uint32_t SourceMusicTimerCapsAccess::minimum() const
{
    return checked_owner()->state().audio_stream.timer_capabilities.minimum();
}
std::uint32_t SourceMusicTimerCapsAccess::maximum() const
{
    return checked_owner()->state().audio_stream.timer_capabilities.maximum();
}
void SourceMusicTimerCapsAccess::minimum(std::uint32_t value) const
{
    checked_owner()->state().audio_stream.timer_capabilities.minimum() = value;
}
void SourceMusicTimerCapsAccess::maximum(std::uint32_t value) const
{
    checked_owner()->state().audio_stream.timer_capabilities.maximum() = value;
}

struct SourceMusicDriveRootAccess::Scope
{
    const std::weak_ptr<RaceSessionPlatform> owner;
    const std::thread::id thread;
    const SourceMusicRootScratch *root{};
    bool active{};
    explicit Scope(const std::shared_ptr<RaceSessionPlatform> &value) : owner(value), thread(std::this_thread::get_id())
    {
    }
};
SourceMusicDriveRootAccess::SourceMusicDriveRootAccess(std::shared_ptr<Scope> scope) : scope_(std::move(scope)) {}
std::shared_ptr<RaceSessionPlatform> SourceMusicDriveRootAccess::checked_owner() const
{
    // Check immutable thread identity before active/root or a weak-owner lock.
    if (std::this_thread::get_id() != scope_->thread)
        throw std::logic_error("music drive root access from a foreign thread");
    if (!scope_->active || !scope_->root)
        throw std::logic_error("music drive root access outside the synchronous call");
    auto owner = scope_->owner.lock();
    if (!owner)
        throw std::logic_error("music drive root owner expired");
    owner->require_owner_thread();
    return owner;
}
std::uint8_t SourceMusicDriveRootAccess::byte(std::size_t offset) const
{
    const auto owner = checked_owner();
    (void)owner; // retain through the actual byte read
    if (offset >= byte_extent)
        throw std::out_of_range("music drive root byte offset");
    return scope_->root->bytes_[offset];
}
const char *SourceMusicDriveRootAccess::checked_native_root() const
{
    const auto owner = checked_owner();
    (void)owner;
    // Character access aliases the original array; no temporary or copy-back.
    // This private pointer is issued only to the TU-private installed bridge.
    return reinterpret_cast<const char *>(scope_->root->bytes_.data());
}

struct SourceMusicDiskDwordAccess::Cell
{
    SourceMusicDiskWordState state;
};
struct SourceMusicDiskDwordAccess::Scope
{
    const std::weak_ptr<RaceSessionPlatform> owner;
    const std::thread::id thread;
    Cell *argument{};
    Cell *bytes_per_sector{};
    bool active{};
    explicit Scope(const std::shared_ptr<RaceSessionPlatform> &value) : owner(value), thread(std::this_thread::get_id())
    {
    }
};
SourceMusicDiskDwordAccess::SourceMusicDiskDwordAccess(std::shared_ptr<Scope> scope, Slot slot)
    : scope_(std::move(scope)), slot_(slot)
{
}
std::shared_ptr<RaceSessionPlatform> SourceMusicDiskDwordAccess::checked_owner() const
{
    if (std::this_thread::get_id() != scope_->thread)
        throw std::logic_error("music disk output access from a foreign thread");
    if (!scope_->active || !scope_->argument || !scope_->bytes_per_sector)
        throw std::logic_error("music disk output access outside the synchronous call");
    auto owner = scope_->owner.lock();
    if (!owner)
        throw std::logic_error("music disk output owner expired");
    owner->require_owner_thread();
    return owner;
}
SourceMusicDiskWordState SourceMusicDiskDwordAccess::read() const
{
    const auto owner = checked_owner();
    (void)owner;
    return (slot_ == Slot::argument ? scope_->argument : scope_->bytes_per_sector)->state;
}
void SourceMusicDiskDwordAccess::write(std::uint32_t bits) const
{
    const auto owner = checked_owner();
    (void)owner;
    auto &cell = *(slot_ == Slot::argument ? scope_->argument : scope_->bytes_per_sector);
    cell.state = {SourceMusicDiskWordOrigin::provider_write, bits};
}
bool SourceMusicDiskDwordAccess::aliases(const SourceMusicDiskDwordAccess &other) const
{
    const auto owner = checked_owner();
    const auto other_owner = other.checked_owner();
    (void)owner;
    (void)other_owner;
    return scope_.get() == other.scope_.get() && slot_ == other.slot_;
}

struct SourceMusicPrefixExecution final
{
    struct Result
    {
        std::uint32_t minimum, maximum, selected;
        std::optional<SourceMusicDriveTypePrefixObservation> drive;
        std::optional<SourceMusicDiskFreeSpacePrefixObservation> disk;
        std::optional<std::uint32_t> wave_result;
    };
    static Result run(std::shared_ptr<RaceSessionPlatform> owner, SourceMusicTimerCapsReceiving &provider,
                      SourceMusicDriveTypeReceiving *drive, SourceMusicDiskFreeSpaceReceiving *disk = nullptr,
                      SourceMusicWaveIO *wave = nullptr)
    {
        if (!owner)
            throw std::invalid_argument("music timer prefix requires a retained platform");
        owner->require_owner_thread();
        // Host metadata allocation happens before the source call and any write.
        // The scope holds only a weak owner; escaped access cannot prolong the
        // platform lifetime or destroy its native owners on another thread.
        auto scope = std::make_shared<SourceMusicTimerCapsAccess::Scope>(owner);
        // All new borrow metadata is allocated before the source prologue writes.
        // The old two-argument entry has no drive scope and makes no drive call.
        auto drive_scope = drive ? std::make_shared<SourceMusicDriveRootAccess::Scope>(owner) : nullptr;
        // Separate scope: never reactivate an escaped GetDriveType borrow.
        auto disk_root_scope = disk ? std::make_shared<SourceMusicDriveRootAccess::Scope>(owner) : nullptr;
        auto disk_scope = disk ? std::make_shared<SourceMusicDiskDwordAccess::Scope>(owner) : nullptr;
        SourceMusicTimerCapsAccess output(scope);
        struct Expire
        {
            bool &active;
            ~Expire()
            {
                active = false;
            }
        } expire{scope->active};

        // 45FDAC/45FDB3/45FDB8: the five-byte local exists and is zeroed
        //  before45FDBB, including the capability-failure path. It never escapes.
        SourceMusicRootScratch root;
        struct ExpireDrive
        {
            std::shared_ptr<SourceMusicDriveRootAccess::Scope> scope;
            ~ExpireDrive()
            {
                if (scope)
                {
                    scope->active = false;
                    scope->root = nullptr;
                }
            }
        } expire_drive{drive_scope}; // destroyed before root, including unwind
        ExpireDrive expire_disk_root{disk_root_scope};
        // Source slots are unknown bits, not convenient zero-initialized DWORDs.
        // The first slot initially carries the original32-bit stream argument;
        // its new64-bit host owner is deliberately not truncated into that word.
        SourceMusicDiskDwordAccess::Cell disk_argument{
            {SourceMusicDiskWordOrigin::original_argument_pointer, std::nullopt}};
        SourceMusicDiskDwordAccess::Cell disk_sector{{SourceMusicDiskWordOrigin::unwritten_stack, std::nullopt}};
        struct ExpireDiskOutputs
        {
            std::shared_ptr<SourceMusicDiskDwordAccess::Scope> scope;
            ~ExpireDiskOutputs()
            {
                if (scope)
                {
                    scope->active = false;
                    scope->argument = nullptr;
                    scope->bytes_per_sector = nullptr;
                }
            }
        } expire_disk_outputs{disk_scope}; // destroyed before either cell

        // PE4a1d234c...,45FDAE size8;45FDB0/45FDB7 actual ESI+3C;
        // 45FDBB IAT46E1AC=timeGetDevCaps. Partial provider writes are immediately
        // visible in the actual state, including on nonzero return or exception.
        const auto result = provider.time_get_dev_caps(output, 8u);
        scope->active = false;
        if (result != 0)
        {
            // 45FDC1..45FDC5. NEW host policy stops at INT3; no debugger resume,
            //  fallback HRESULT return or stream.result store is manufactured.
            throw SourceMusicTimerCapabilityTrap(result);
        }

        auto &stream = owner->state().audio_stream;
        const auto minimum = stream.timer_capabilities.minimum(); // 45FDD4, ESI+3C
        // Literal unsigned JA/JNC decisions45FDD6..45FDF5, including abnormal
        // zero/inverted/high-bit ranges supplied by an explicit receiving provider.
        const auto lower = minimum > 1u ? minimum : 1u;
        const auto maximum = stream.timer_capabilities.maximum(); // 45FDE2, ESI+40
        const auto selected = lower >= maximum ? maximum : lower;
        stream.timer_period = selected; // 45FDF8, ESI+44, before external call
        // 45FDFB: the actual same-platform owner reserves its obligation before
        //  dispatch. Source ignores every Begin result, including nonzero values.
        //  NEW host precondition/allocation failures propagate without a fake MMRESULT.
        (void)begin_owned_music_period(owner, selected);

        // 45FE01..45FE0B ->463390 count3. Read the live filename only now, after
        //  both preceding providers. No filename snapshot, public scratch or resume.
        root.copy_filename(stream);
        if (!drive)
            return {minimum, maximum, selected, std::nullopt}; // old98f boundary

        // 45FE13 LEA EDX,[EBP-18];45FE16 PUSH EDX;45FE17 IAT46E074.
        //  This is the same local just filled by the count3 copy, never the filename
        //  field, a snapshot at entry, a default drive or a null-pointer substitute.
        drive_scope->root = &root;
        drive_scope->active = true;
        const auto drive_type = drive->get_drive_type_a(SourceMusicDriveRootAccess{drive_scope});
        drive_scope->active = false;
        drive_scope->root = nullptr;
        // 45FE1D CMP EAX,2;45FE20 JNZ45FE30. No source/result-state store or End.
        if (drive_type == 2u)
        {
            // 45FE22 MOV EAX,80004005; ordinary epilogue and RET4 at45FE2D.
            return {minimum, maximum, selected,
                    SourceMusicDriveTypePrefixObservation{minimum, maximum, selected, drive_type, 0x80004005u,
                                                          std::nullopt}};
        }
        if (disk)
        {
            // 45FE30..43: PUSH total/free/bytes/sectors/root. In API argument order
            //  the output addresses are +8,-4,+8,+8 relative to the same EBP.
            //  These are two cells, never four independent locals plus copy-back.
            disk_scope->argument = &disk_argument;
            disk_scope->bytes_per_sector = &disk_sector;
            disk_scope->active = true;
            disk_root_scope->root = &root;
            disk_root_scope->active = true;
            using Slot = SourceMusicDiskDwordAccess::Slot;
            const auto api_bits = disk->get_disk_free_space_a(
                SourceMusicDriveRootAccess{disk_root_scope}, SourceMusicDiskDwordAccess{disk_scope, Slot::argument},
                SourceMusicDiskDwordAccess{disk_scope, Slot::bytes_per_sector},
                SourceMusicDiskDwordAccess{disk_scope, Slot::argument},
                SourceMusicDiskDwordAccess{disk_scope, Slot::argument});
            disk_scope->active = false;
            disk_scope->argument = nullptr;
            disk_scope->bytes_per_sector = nullptr;
            disk_root_scope->active = false;
            disk_root_scope->root = nullptr;
            const SourceMusicDriveTypePrefixObservation drive_observation{minimum,    maximum,      selected,
                                                                          drive_type, std::nullopt, 0x0045fe30u};
            SourceMusicDiskFreeSpacePrefixObservation out{drive_observation,
                                                          api_bits,
                                                          disk_argument.state,
                                                          disk_sector.state,
                                                          SourceMusicDiskSpaceStop::bytes_per_sector_unknown,
                                                          0x0045fe4bu,
                                                          std::nullopt};
            // 45FE4A saves raw EAX; there is no BOOL branch or success-only filter.
            //  Unwritten [EBP-4] cannot be read through a C++ indeterminate DWORD.
            if (!disk_sector.state.bits)
                return {minimum, maximum, selected, drive_observation, out};
            if (*disk_sector.state.bits == 0)
            {
                // Historical retail BSF(0) destination has no qualified oracle.
                // Do not substitute countr_zero(0)=32 or current-CPU EAX retention.
                out.stop = SourceMusicDiskSpaceStop::zero_bsf_unqualified;
                return {minimum, maximum, selected, drive_observation, out};
            }
            const auto shift = static_cast<std::uint8_t>(std::countr_zero(*disk_sector.state.bits));
            // 45FE4F gives only the low byte of [EBP-8]; high24 bits stay unknown.
            // 45FE52 restores api_bits in EAX.45FE53 loads a partly unknown EBX,
            //  but45FE56 consumes only known BL. Write exactly that source byte.
            std::as_writable_bytes(std::span{&stream.field_df18, std::size_t{1}})[0] = static_cast<std::byte>(shift);
            out.written_shift = shift;
            out.stop = SourceMusicDiskSpaceStop::before_helper_45ffd0;
            out.next_source_site = 0x0045fe59u;
            if (wave)
            {
                const auto wave_result = source_music_wave_open_45ffd0(stream, *wave);
                return {minimum, maximum, selected, drive_observation, out, wave_result};
            }
            return {minimum, maximum, selected, drive_observation, out};
        }
        // All other uint32 values, including unknown/high bits, take this boundary.
        // No45FE30 stack argument setup,45FE44 GetDiskFreeSpaceA or later music.
        return {
            minimum, maximum, selected,
            SourceMusicDriveTypePrefixObservation{minimum, maximum, selected, drive_type, std::nullopt, 0x0045fe30u}};
    }
};

SourceMusicTimerPrefixObservation receive_source_music_timer_prefix_45fda0(std::shared_ptr<RaceSessionPlatform> owner,
                                                                           SourceMusicTimerCapsReceiving &provider)
{
    const auto result = SourceMusicPrefixExecution::run(std::move(owner), provider, nullptr);
    return {result.minimum, result.maximum, result.selected};
}
SourceMusicDriveTypePrefixObservation receive_source_music_drive_type_prefix_45fda0(
    std::shared_ptr<RaceSessionPlatform> owner, SourceMusicTimerCapsReceiving &caps,
    SourceMusicDriveTypeReceiving &drive)
{
    const auto result = SourceMusicPrefixExecution::run(std::move(owner), caps, &drive);
    return *result.drive; // non-null explicit drive always produces the leaf or throws
}
SourceMusicDiskFreeSpacePrefixObservation receive_source_music_disk_free_space_prefix_45fda0(
    std::shared_ptr<RaceSessionPlatform> owner, SourceMusicTimerCapsReceiving &caps,
    SourceMusicDriveTypeReceiving &drive, SourceMusicDiskFreeSpaceReceiving &disk)
{
    const auto result = SourceMusicPrefixExecution::run(std::move(owner), caps, &drive, &disk);
    if (result.disk)
        return *result.disk;
    // Only the already-proved result2 source return bypasses the disk callback.
    return {*result.drive,
            std::nullopt,
            {SourceMusicDiskWordOrigin::original_argument_pointer, std::nullopt},
            {SourceMusicDiskWordOrigin::unwritten_stack, std::nullopt},
            SourceMusicDiskSpaceStop::drive_return,
            std::nullopt,
            std::nullopt};
}

SourceMusicWaveEntryResult receive_source_music_wave_entry_45fda0(std::shared_ptr<RaceSessionPlatform> owner,
                                                                  SourceMusicTimerCapsReceiving &caps,
                                                                  SourceMusicDriveTypeReceiving &drive,
                                                                  SourceMusicDiskFreeSpaceReceiving &disk,
                                                                  SourceMusicWaveIO &wave)
{
    const auto result = SourceMusicPrefixExecution::run(std::move(owner), caps, &drive, &disk, &wave);
    if (!result.disk)
    {
        SourceMusicDiskFreeSpacePrefixObservation prior{
            *result.drive,
            std::nullopt,
            {SourceMusicDiskWordOrigin::original_argument_pointer, std::nullopt},
            {SourceMusicDiskWordOrigin::unwritten_stack, std::nullopt},
            SourceMusicDiskSpaceStop::drive_return,
            std::nullopt,
            std::nullopt};
        return {prior, std::nullopt, 0x80004005u, std::nullopt};
    }
    if (!result.wave_result)
        return {*result.disk, std::nullopt, std::nullopt, result.disk->next_source_site};
    // 45FE5E..45FE6A: any nonzero helper HRESULT returns ZERO, not that HRESULT.
    if (*result.wave_result != 0)
        return {*result.disk, result.wave_result, 0u, std::nullopt};
    return {*result.disk, result.wave_result, std::nullopt, 0x0045fe6du};
}
} // namespace sgn::retail
