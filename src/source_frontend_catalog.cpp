#include "sgn/source_frontend_catalog.hpp"
#include "retail_frontend_catalog_parser.hpp"
#include <bit>
#include <cstring>

namespace sgn::retail
{
namespace
{
struct InvocationGuard
{
    FrontendSourceStorage *storage;
    SourceFrontendResourceMemory *memory;
    InvocationGuard *previous;
    static thread_local InvocationGuard *active;
    InvocationGuard(FrontendSourceStorage &s, SourceFrontendResourceMemory &m)
        : storage(&s), memory(&m), previous(active)
    {
        for (auto *p = active; p; p = p->previous)
            if (p->storage == storage || p->memory == memory)
                throw SourceFrontendCatalogError(0x440ae0, "frontend catalog reentry");
        active = this;
    }
    ~InvocationGuard()
    {
        active = previous;
    }
    InvocationGuard(const InvocationGuard &) = delete;
    InvocationGuard &operator=(const InvocationGuard &) = delete;
};
thread_local InvocationGuard *InvocationGuard::active = nullptr;
struct Reporter
{
    SourceFrontendCatalogObserver observer;
    void store(std::uint32_t pc, std::uint32_t address, std::uint32_t width, std::uint32_t value) const noexcept
    {
        if (observer.observe)
            observer.observe(observer.context,
                             {SourceFrontendCatalogEventKind::after_store, pc, address, width, value, 0, 0});
    }
    void read(std::uint32_t pc, std::uint32_t identity, std::uint32_t offset) const noexcept
    {
        if (observer.observe)
            observer.observe(observer.context,
                             {SourceFrontendCatalogEventKind::before_read, pc, 0, 0, 0, identity, offset});
    }
};
struct SourceInput
{
    SourceFrontendResourceMemory &memory;
    SourceFrontendResourceRef reference;
    Reporter reporter;
    std::uint8_t read(std::uint32_t offset, std::uint32_t pc)
    {
        reporter.read(pc, reference.identity(), offset);
        return memory.read_byte(reference, offset, pc);
    }
    [[noreturn]] void fail(std::uint32_t pc, const char *why) const
    {
        throw SourceFrontendCatalogError(pc, why);
    }
};
struct SourceWriter
{
    FrontendLanguageCatalogViews views;
    Reporter reporter;
    [[noreturn]] void fail(std::uint32_t pc, const char *why) const
    {
        throw SourceFrontendCatalogError(pc, why);
    }
    void name(unsigned index, std::span<const std::uint8_t> bytes)
    {
        views.language_names[index + 1] = index + 1;
        reporter.store(0x440b79, 0x60683c + index * 4, 4, index + 1);
        auto *destination = reinterpret_cast<std::uint8_t *>(views.catalog.data()) + 4 + index * 36;
        const auto address = 0x5e37bcu + index * 36;
        std::size_t offset = 0;
        for (; offset + 4 <= bytes.size(); offset += 4)
        {
            std::uint32_t value{};
            std::memcpy(&value, bytes.data() + offset, 4);
            std::memcpy(destination + offset, &value, 4);
            reporter.store(0x440b7e, address + static_cast<std::uint32_t>(offset), 4, value);
        }
        for (; offset < bytes.size(); ++offset)
        {
            destination[offset] = bytes[offset];
            reporter.store(0x440b8b, address + static_cast<std::uint32_t>(offset), 1, bytes[offset]);
        }
    }
    void count(unsigned value)
    {
        views.catalog.back() = value;
        reporter.store(0x440bb9, 0x5e38dc, 4, value);
    }
    void flag(unsigned index, std::uint32_t value)
    {
        views.catalog[9 + index * 9] = value;
        reporter.store(0x440c0a, 0x5e37dc + index * 36, 4, value);
    }
};
} // namespace
std::uint32_t detail::source_frontend_catalog_440ae0(FrontendSourceStorage &storage,
                                                     SourceFrontendResourceMemory &memory,
                                                     SourceBootFilePlatform &files,
                                                     SourceFrontendCatalogObserver observer)
{
    static_assert(std::endian::native == std::endian::little);
    InvocationGuard guard(storage, memory);
    (void)memory.current_reference(); // Host thread admission only; never resolves/dereferences raw data.
    Reporter reporter{observer};
    auto views = storage.language_catalog_view();
    for (unsigned i = 0; i < views.text_handles.size(); ++i)
    {
        views.text_handles[i] = 0;
        reporter.store(0x440af8, 0x605cc8 + i * 4, 4, 0);
    }
    const auto reference = memory.load_43c7f0(files, "Setup\\languagePC.csv"); // 440B04, same persistent owner/config
    for (unsigned i = 0; i < views.language_names.size(); ++i)
    {
        views.language_names[i] = 0;
        reporter.store(0x440b1a, 0x606838 + i * 4, 4, 0);
    }
    memory.publish_440b26(reference);
    reporter.store(0x440b26, 0x6087a0, 4, views.input_identity);
    views.catalog.back() = 0;
    reporter.store(0x440b2c, 0x5e38dc, 4, 0);
    SourceInput input{memory, reference, reporter};
    SourceWriter writer{views, reporter};
    return detail::parse_catalog_contents(input, writer, false);
}
} // namespace sgn::retail
