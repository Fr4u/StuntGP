#include "sgn/source_frontend_resource_memory.hpp"
#include <algorithm>
#include <bit>
#include <cstdlib>
#include <limits>
#include <list>
#include <thread>
#include <utility>

namespace sgn::retail
{
namespace detail
{
struct FrontendResourceIdentity
{
};
} // namespace detail
SourceFrontendResourceRef::SourceFrontendResourceRef(std::weak_ptr<const detail::FrontendResourceIdentity> owner,
                                                     std::uint32_t id) noexcept
    : owner_(std::move(owner)), identity_(id)
{
}

struct SourceFrontendResourceMemory::Impl
{
    struct Record
    {
        std::uint32_t identity{}, size{};
        SourceBootRawAllocation allocation;
        std::size_t initialized{};
        bool terminated{}, returned{};
        std::optional<std::uint32_t> read_return;
    };
    SourceBootConfigState &config;
    std::uint32_t &source_identity;
    const SourceFrontendResourceObserver observer;
    const std::thread::id owner_thread{std::this_thread::get_id()};
    std::shared_ptr<const detail::FrontendResourceIdentity> identity =
        std::make_shared<const detail::FrontendResourceIdentity>();
    std::list<Record> records;
    std::uint64_t next_identity = 1;
    bool active{};
    SourceFrontendResourceAttempt attempt;
    Impl(SourceBootConfigState &c, std::uint32_t &word, SourceFrontendResourceObserver o)
        : config(c), source_identity(word), observer(o)
    {
        if (source_identity != 0)
            throw SourceFrontendResourceError(0x440b26u,
                                              "catalog memory binding requires its actual initial null source cell");
    }
    void owner() const
    {
        if (std::this_thread::get_id() != owner_thread)
            throw SourceFrontendResourceError(0x43c7f0u, "catalog memory belongs to its creating thread");
    }
    struct Operation
    {
        Impl &impl;
        Operation(Impl &value, std::uint32_t site) : impl(value)
        {
            impl.owner();
            if (impl.active)
                throw SourceFrontendResourceError(site, "catalog memory operation cannot reenter its owner");
            impl.active = true;
        }
        ~Operation()
        {
            impl.active = false;
        }
    };
    static SourceFrontendResourceInfo info(const Record &r) noexcept
    {
        return {r.identity,    r.size,       r.size + 1u, reinterpret_cast<std::uintptr_t>(r.allocation.get()),
                r.initialized, r.terminated, r.returned,  r.read_return};
    }
    void event(SourceFrontendResourceEventKind kind, std::uint32_t site,
               SourceFrontendResourceInfo resource = {}) const noexcept
    {
        if (observer.observe)
            observer.observe(observer.context, {kind, site, source_identity, resource});
    }
    const Record &find(std::uint32_t id, std::uint32_t site) const
    {
        for (const auto &r : records)
            if (r.identity == id && id != 0 && r.allocation)
                return r;
        throw SourceFrontendResourceError(
            site, "catalog reference is null, stale or does not belong to a live raw allocation");
    }
    Record &find(std::uint32_t id, std::uint32_t site)
    {
        return const_cast<Record &>(std::as_const(*this).find(id, site));
    }
    void free_record(Record &r, bool source) noexcept
    {
        const auto captured = info(r);
        event(source ? SourceFrontendResourceEventKind::before_source_free
                     : SourceFrontendResourceEventKind::before_host_free,
              source ? 0x440c2au : 0, captured);
        r.allocation.reset(); // Existing raw malloc/free family, never a bank release.
        event(source ? SourceFrontendResourceEventKind::after_source_free
                     : SourceFrontendResourceEventKind::after_host_free,
              source ? 0x440c2fu : 0, captured);
    }
    ~Impl()
    {
        if (std::this_thread::get_id() != owner_thread)
            std::terminate();
        active = true;
        // Host final cleanup, separate from440C20. Source identity is untouched.
        for (auto it = records.rbegin(); it != records.rend(); ++it)
            if (it->allocation)
                free_record(*it, false);
    }
};
SourceFrontendResourceMemory::SourceFrontendResourceMemory(SourceBootConfigState &config, std::uint32_t &identity,
                                                           SourceFrontendResourceObserver observer)
    : impl_(std::make_unique<Impl>(config, identity, observer))
{
}
SourceFrontendResourceMemory::~SourceFrontendResourceMemory() = default;

SourceFrontendResourceRef SourceFrontendResourceMemory::load_43c7f0(SourceBootFilePlatform &files,
                                                                    std::string_view name)
{
    auto &p = *impl_;
    Impl::Operation operation(p, 0x43c7f0u);
    p.attempt = {};
    const auto end = std::find(p.config.base_path.begin(), p.config.base_path.end(), '\0');
    if (end == p.config.base_path.end() || name.find('\0') != name.npos)
        throw SourceFrontendResourceError(0x43c80cu, "source catalog path requires bounded C strings");
    const auto root_size = static_cast<std::size_t>(end - p.config.base_path.begin());
    if (name.size() > 254u - root_size)
        throw SourceFrontendResourceError(0x43c80cu, "catalog sprintf exceeds its256-byte source local");
    std::array<char, 256> path; // Only the actual sprintf output bytes are written.
    std::copy_n(p.config.base_path.begin(), root_size, path.begin());
    path[root_size] = '\\';
    std::copy(name.begin(), name.end(), path.begin() + static_cast<std::ptrdiff_t>(root_size + 1));
    const auto path_size = root_size + 1 + name.size();
    path[path_size] = 0;
    const std::string_view spelling(path.data(), path_size);
    const auto size = source_boot_file_size_44b3b0(p.config, files, spelling); // 43C81A
    p.attempt.size_return = size;
    p.event(SourceFrontendResourceEventKind::size_returned, 0x43c81fu);
    if (std::bit_cast<std::int32_t>(size) <= 0)
    {
        p.attempt.loader_returned = true;
        return {};
    }
    if (p.next_identity > std::numeric_limits<std::uint32_t>::max())
        throw SourceFrontendResourceError(0x43c831u, "catalog host identities exhausted");
    // Metadata node exists before native allocation/read; partial output cannot
    // be lost to a later container allocation or an exception unwinding a local.
    auto &record = p.records.emplace_back();
    record.size = size;
    record.allocation = files.allocate_wad_raw(size + 1u); // 43C831: existing RAW primitive
    if (!record.allocation)
        throw SourceFrontendResourceError(0x43c83bu,
                                          "source catalog malloc returned null before an unchecked read/NUL write");
    record.identity = static_cast<std::uint32_t>(p.next_identity++);
    const SourceFrontendResourceRef reference(p.identity, record.identity);
    p.attempt.resource = reference;
    p.event(SourceFrontendResourceEventKind::allocated, 0x43c839u, Impl::info(record));
    const auto returned = source_boot_file_read_44b260(p.config, files, spelling, size, {record.allocation.get(), size},
                                                       record.initialized); // 43C844; may throw after partial stores
    record.read_return = returned;
    if (record.initialized > size)
        throw SourceFrontendResourceError(0x43c844u, "file provider reported an impossible initialized prefix");
    p.event(SourceFrontendResourceEventKind::read_returned, 0x43c849u, Impl::info(record));
    record.allocation.get()[size] = 0;
    record.terminated = true; // 43C849, even short read
    p.event(SourceFrontendResourceEventKind::terminator_written, 0x43c84du, Impl::info(record));
    record.returned = true;
    p.attempt.loader_returned = true;
    return reference;
}
void SourceFrontendResourceMemory::publish_440b26(const SourceFrontendResourceRef &ref)
{
    auto &p = *impl_;
    Impl::Operation operation(p, 0x440b26u);
    SourceFrontendResourceInfo info;
    if (ref)
    {
        if (ref.owner_.lock() != p.identity)
            throw SourceFrontendResourceError(0x440b26u, "catalog publication uses another or expired owner");
        const auto &record = p.find(ref.identity_, 0x440b26u);
        if (!record.returned)
            throw SourceFrontendResourceError(0x440b26u, "catalog allocation did not return from43C7F0");
        info = Impl::info(record);
    }
    p.source_identity = ref.identity_; // No free or other frontend mutation.
    p.event(SourceFrontendResourceEventKind::published, 0x440b26u, info);
}
void SourceFrontendResourceMemory::dispose_440c20()
{
    auto &p = *impl_;
    Impl::Operation operation(p, 0x440c20u);
    const auto current = p.source_identity;
    if (current == 0)
        return;
    auto &record = p.find(current, 0x440c2au);
    p.free_record(record, true);
    p.source_identity = 0;
    p.event(SourceFrontendResourceEventKind::source_null, 0x440c32u);
}
SourceFrontendResourceRef SourceFrontendResourceMemory::current_reference() const
{
    impl_->owner();
    return impl_->source_identity ? SourceFrontendResourceRef(impl_->identity, impl_->source_identity)
                                  : SourceFrontendResourceRef{};
}
SourceFrontendResourceAttempt SourceFrontendResourceMemory::last_attempt() const
{
    impl_->owner();
    return impl_->attempt;
}
SourceFrontendResourceInfo SourceFrontendResourceMemory::inspect(const SourceFrontendResourceRef &ref) const
{
    impl_->owner();
    if (!ref)
        return {};
    if (ref.owner_.lock() != impl_->identity)
        throw SourceFrontendResourceError(0, "catalog diagnostic uses another or expired owner");
    return Impl::info(impl_->find(ref.identity_, 0));
}
std::size_t SourceFrontendResourceMemory::live_allocations() const
{
    impl_->owner();
    return static_cast<std::size_t>(std::count_if(impl_->records.begin(), impl_->records.end(),
                                                  [](const auto &r) { return static_cast<bool>(r.allocation); }));
}
std::uint8_t SourceFrontendResourceMemory::read_byte(const SourceFrontendResourceRef &ref, std::uint32_t at,
                                                     std::uint32_t site) const
{
    impl_->owner();
    if (!ref || ref.owner_.lock() != impl_->identity)
        throw SourceFrontendResourceError(site, "source catalog read uses a null, foreign or expired resource");
    const auto &r = impl_->find(ref.identity_, site);
    if (at < r.initialized && at < r.size)
        return r.allocation.get()[at];
    if (r.terminated && at == r.size)
        return r.allocation.get()[at];
    throw SourceFrontendResourceError(site,
                                      "source catalog read reaches uninitialized or outside raw allocation bytes");
}
std::span<const std::uint8_t> SourceFrontendResourceMemory::initialized_prefix(
    const SourceFrontendResourceRef &ref) const
{
    impl_->owner();
    if (!ref)
        return {};
    if (ref.owner_.lock() != impl_->identity)
        throw SourceFrontendResourceError(0, "catalog prefix uses another or expired owner");
    const auto &r = impl_->find(ref.identity_, 0);
    if (r.initialized > r.size)
        throw SourceFrontendResourceError(0, "invalid initialized-prefix observation");
    return {r.allocation.get(), r.initialized};
}
FrontendLanguageInput SourceFrontendResourceMemory::complete_input(const SourceFrontendResourceRef &ref,
                                                                   std::uint32_t site) const
{
    impl_->owner();
    if (!ref || ref.owner_.lock() != impl_->identity)
        throw SourceFrontendResourceError(site, "catalog full borrow uses a null, foreign or expired resource");
    const auto &r = impl_->find(ref.identity_, site);
    if (!r.returned || !r.terminated || r.initialized != r.size)
        throw SourceFrontendResourceError(site,
                                          "catalog full borrow includes an uninitialized gap or an unfinished load");
    return {{r.allocation.get(), static_cast<std::size_t>(r.size) + 1}, r.identity};
}
} // namespace sgn::retail

namespace sgn::retail
{
void SourceFrontendResourceMemory::release_462f9e(const SourceFrontendResourceRef &ref, std::uint32_t site)
{
    switch (site)
    {
    case 0x434ba5:
    case 0x434ccf:
    case 0x434cf1:
    case 0x434d13:
    case 0x434d35:
    case 0x434d57:
    case 0x434d79:
    case 0x434d9b:
    case 0x434dbd:
    case 0x434ddf:
    case 0x434e01:
        break;
    default:
        throw SourceFrontendResourceError(site, "unbound temporary CSV free callsite");
    }
    auto &p = *impl_;
    Impl::Operation operation(p, site);
    if (!ref)
        return; // free(NULL)
    if (ref.owner_.lock() != p.identity)
        throw SourceFrontendResourceError(site, "temporary CSV free uses another owner");
    if (ref.identity_ == p.source_identity)
        throw SourceFrontendResourceError(site, "temporary CSV free cannot free the published language catalog");
    auto &record = p.find(ref.identity_, site);
    const auto captured = Impl::info(record);
    p.event(SourceFrontendResourceEventKind::before_source_free, site, captured);
    record.allocation.reset();
    p.event(SourceFrontendResourceEventKind::after_source_free, site + 5, captured);
}
} // namespace sgn::retail
