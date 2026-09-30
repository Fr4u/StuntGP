#include "sgn/source_boot_config.hpp"
#include "sgn/source_boot_config_data.hpp"
#include "sgn/source_wad_transfer.hpp"
#include <algorithm>
#include <bit>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <optional>
#include <set>
#include <string>
#include <utility>

namespace sgn::retail
{
void SourceBootRawFree::operator()(std::uint8_t *memory) const noexcept
{
    std::free(memory);
}
namespace
{
using Word = std::uint32_t;
void check(bool ok, Word at, const char *why)
{
    if (!ok)
        throw SourceBootConfigError(at, why);
}
std::string_view text(std::span<const char> bytes, Word at)
{
    const auto end = std::find(bytes.begin(), bytes.end(), '\0');
    check(end != bytes.end(), at, "Source file string lacks bounded terminator");
    return {bytes.data(), static_cast<std::size_t>(end - bytes.begin())};
}
char lower(char c)
{
    return c >= 'A' && c <= 'Z' ? static_cast<char>(c + 32) : c;
}
bool equal_folded(std::string_view a, std::string_view b)
{
    if (a.size() != b.size())
        return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (lower(a[i]) != lower(b[i]))
            return false;
    return true;
}
template <std::size_t N> void store(std::array<char, N> &out, std::string_view value, Word site)
{
    for (std::size_t i = 0; i < value.size(); ++i)
    {
        check(i < N, site, "Source file name exceeds destination owner");
        out[i] = value[i];
    }
    check(value.size() < N, site, "Source file name terminator exceeds destination owner");
    out[value.size()] = 0;
}
struct SplitPath
{
    std::string directory, filename;
};
SplitPath split_44b050(std::string_view path, const SourceBootConfigState &state)
{
    check(!path.empty() && path.size() < 128 && path.find('\0') == std::string_view::npos, 0x44b063u,
          "Source path exceeds admitted local stack string");
    std::array<char, 128> copy{};
    store(copy, path, 0x44b07bu);
    const auto byte = [&](std::size_t i, Word site) {
        check(i <= path.size(), site, "Source path splitter reads unwritten local stack tail");
        return copy[i];
    };
    std::size_t last = path.size() - 1u;
    while (byte(last, 0x44b09du) != '\\' && last > 0)
        --last;
    auto skipped = last;
    while (byte(skipped, 0x44b0c4u) == '\\')
        ++skipped;
    // 44B170 overwrites EAX: source ROOT LENGTH, not the last-slash position.
    //  Do not replace this with filesystem::parent_path or a canonical basename.
    auto start = text(state.base_path, 0x44b17du).size();
    while (byte(start, 0x44b185u) == '\\')
        ++start;
    const auto collapse = [&](std::size_t from) {
        std::string value;
        for (auto i = from; byte(i, 0x44b140u) != 0; ++i)
        {
            if (byte(i, 0x44b140u) == '\\' && byte(i + 1u, 0x44b140u) == '\\')
                ++i;
            check(value.size() < 63, 0x44b140u, "Source split result exceeds64-byte stack output");
            value.push_back(lower(byte(i, 0x44b140u)));
        }
        return value;
    };
    auto filename = collapse(start);
    copy[last] = 0;                   // 44B10D; source BL is nonnegative for the admitted nonempty path
    auto directory = collapse(start); // same ESI root-offset pointer, not beginning of local buffer
    return {std::move(directory), std::move(filename)};
}
bool prefix_44b380(std::string_view a, std::string_view b)
{
    const auto count = std::min(a.size(), b.size());
    return a.substr(0, count) == b.substr(0, count); // either ending counts as a match; empty matches every row
}
void load_archive_44b1d0(SourceBootConfigState &config, SourceBootFilePlatform &files, std::string_view name)
{
    auto &state = config.files;
    release_source_archive_44b1a0(state);
    const std::string path = std::string(text(config.base_path, 0x44b1eeu)) + "\\wads\\" + std::string(name);
    check(path.size() < 64, 0x44b1eeu, "Source WAD path sprintf exceeds64-byte stack output");
    const Word length = files.size_loose_44af60(path);
    if (length == 0)
        return;
    store(state.archive_name, name, 0x44b223u);
    state.archive = files.allocate_wad_raw(length); // 44B239: raw malloc; no bank entry/zero fill
    state.archive_size = length;                    // 44B23F, even malloc null
    state.readable_prefix = 0;
    check(static_cast<bool>(state.archive), 0x44b245u, "Original WAD caller would pass failed malloc to file read");
    (void)files.read_loose_44afa0(path, length, {state.archive.get(), length}, state.readable_prefix);
    check(state.readable_prefix <= length, 0x44b245u, "File boundary published an impossible write count");
}
class ArchiveReader
{
    const SourceBootFileState &state_;

  public:
    explicit ArchiveReader(const SourceBootFileState &state) : state_(state) {}
    std::uint8_t byte(std::size_t at, Word site) const
    {
        check(at < state_.archive_size && at < state_.readable_prefix, site,
              "Source WAD read reaches uninitialized or outside payload");
        return state_.archive.get()[at];
    }
    Word word(std::size_t at, Word site) const
    {
        Word value = 0;
        for (unsigned i = 0; i < 4; ++i)
            value |= static_cast<Word>(byte(at + i, site)) << (i * 8u);
        return value;
    }
    bool equal(std::size_t at, std::string_view name) const
    {
        for (std::size_t i = 0;; ++i)
        {
            const auto a = byte(at + i, 0x44b570u);
            const auto b = i < name.size() ? static_cast<unsigned char>(name[i]) : 0u;
            if (a != b)
                return false;
            if (a == 0)
                return true;
        }
    }
};
Word lookup_archive_44b4e0(SourceBootFileState &state, std::string_view filename, Word &length)
{
    length = 0;
    if (!state.archive)
        return 0;
    const ArchiveReader reader(state);
    if (reader.byte(0, 0x44b4f5u) != 'D' || reader.byte(1, 0x44b4fau) != 'I' || reader.byte(2, 0x44b500u) != 'R' ||
        reader.byte(3, 0x44b506u) != 0x1a)
        return 0;
    const Word table = reader.word(8, 0x44b50cu);
    std::string folded(filename);
    for (auto &c : folded)
        c = lower(c);
    store(state.searched_name, folded, 0x44b514u);
    Word hash = 0;
    for (const unsigned char c : folded)
    {
        hash = (((hash & 0x1ffu) << 1u) | (hash >> 9u)) + c;
        hash &= 0x3ffu;
    }
    const std::size_t table_offset = table;
    Word link = reader.word(table_offset + 4u + static_cast<std::size_t>(hash) * 4u, 0x44b527u);
    std::set<Word> visited; // host rejection of a cyclic source chain, not a success cutoff
    while (link != 0)
    {
        check(visited.insert(link).second, 0x44b542u, "Cyclic source WAD hash chain");
        const std::size_t node = table_offset + link;
        if (reader.equal(node + 12u, folded))
        {
            length = reader.word(node + 8u, 0x44b555u);
            return reader.word(node + 4u, 0x44b55bu);
        }
        link = reader.word(node, 0x44b542u);
    }
    return 0;
}

// Pinned PE4a1d234ce1f095a427bb7f2f0174294b8c85a8fdbbc29b62a3863156b2f16a1b.
// 44B33E/340 unsigned min;44B354 REP MOVSD;44B35D REP MOVSB.
// Whole-width host admission preserves the prior logical accesses. This is
// not a claim about hardware atomicity on an unmapped x86 page.
static_assert(sizeof(Word) == 4);
static_assert(std::endian::native == std::endian::little || std::endian::native == std::endian::big);
Word wad_transfer_read(const SourceBootFileState &cache, Word offset, Word width, Word site)
{
    check(cache.archive && offset <= cache.archive_size && width <= cache.archive_size - offset &&
              offset <= cache.readable_prefix && width <= cache.readable_prefix - offset,
          site, "WAD transfer read reaches uninitialized or outside payload");
    if (width == 1)
        return cache.archive.get()[offset];
    Word value;
    std::memcpy(&value, cache.archive.get() + offset, sizeof(value));
    if constexpr (std::endian::native == std::endian::big)
        value = (value >> 24) | ((value >> 8) & 0xff00u) | ((value << 8) & 0xff0000u) | (value << 24);
    return value;
}
void wad_transfer_write(std::span<std::uint8_t> destination, Word offset, Word width, Word value, Word site)
{
    check(offset <= destination.size() && width <= destination.size() - offset, site,
          "WAD transfer write exceeds actual destination payload");
    if (width == 1)
    {
        destination[offset] = static_cast<std::uint8_t>(value);
        return;
    }
    if constexpr (std::endian::native == std::endian::big)
        value = (value >> 24) | ((value >> 8) & 0xff00u) | ((value << 8) & 0xff0000u) | (value << 24);
    std::memcpy(destination.data() + offset, &value, sizeof(value));
}
struct WadSpanDestination
{
    std::span<std::uint8_t> payload;
    void write(Word offset, Word width, Word value, Word site)
    {
        wad_transfer_write(payload, offset, width, value, site);
    }
};
struct WadBankDestination
{
    SourceAllocationBank &bank;
    const SourceAllocationRef &reference;
    std::optional<std::span<std::uint8_t>> payload;
    void write(Word offset, Word width, Word value, Word site)
    {
        // First borrow occurs AFTER the completed source read, never at
        // construction/path routing. Its lifetime is this synchronous copy;
        // no callback may mutate/free/reenter either owner.
        if (!payload)
        {
            try
            {
                payload = bank.bytes(reference);
            }
            catch (const SourceAllocationError &error)
            {
                throw SourceBootConfigError(site, error.what());
            }
        }
        wad_transfer_write(*payload, offset, width, value, site);
    }
};
template <class Destination>
Word transfer_wad(const SourceBootFileState &cache, Destination &destination, Word source_offset, Word requested,
                  Word member_length, std::size_t &copied, SourceWadTransferObserver *observer)
{
    copied = 0; // NEW diagnostic; never used as a source loop counter.
    Word remaining = std::min(requested, member_length), destination_offset = 0;
    std::uint64_t ordinal = 0;
    const auto access = [&](SourceWadAccessKind kind, Word site, Word width, Word value) {
        ++ordinal;
        if (observer)
            observer->after_access({kind, site, source_offset, destination_offset, width, value, ordinal}, cache);
    };
    const auto move = [&](Word width, Word site) {
        const Word value = wad_transfer_read(cache, source_offset, width, site);
        access(SourceWadAccessKind::read, site, width, value);
        destination.write(destination_offset, width, value, site);
        copied = static_cast<std::size_t>(destination_offset) + width;
        access(SourceWadAccessKind::write, site, width, value);
        source_offset += width;
        destination_offset += width;
    };
    for (; remaining >= 4; remaining -= 4)
        move(4, 0x44b354u);
    for (; remaining != 0; --remaining)
        move(1, 0x44b35du);
    return member_length; // 44B358, not the copied count.
}
template <class Wad, class Loose>
Word route_read_44b260(SourceBootConfigState &config, SourceBootFilePlatform &files, std::string_view path, Wad &&wad,
                       Loose &&loose)
{
    const auto split = split_44b050(path, config);
    const boot_config_data::FilePrefix *selected = nullptr;
    for (const auto &row : boot_config_data::file_prefixes)
        if (prefix_44b380(split.directory, row.directory))
        {
            if (row.directory.size() == split.directory.size())
            {
                selected = &row;
                break;
            }
            if (selected == nullptr || row.directory.size() > selected->directory.size())
                selected = &row;
        }
    if (selected != nullptr)
    {
        if (!equal_folded(text(config.files.archive_name, 0x44b307u), selected->wad))
            load_archive_44b1d0(config, files, selected->wad);
        if (config.files.archive)
        {
            Word length = 0;
            const Word offset = lookup_archive_44b4e0(config.files, split.filename, length);
            if (offset != 0)
            {
                return wad(offset, length);
            }
        }
    }
    return loose();
}

} // namespace
std::uint32_t source_boot_file_size_44b3b0(SourceBootConfigState &config, SourceBootFilePlatform &files,
                                           std::string_view path)
{
    const auto split = split_44b050(path, config);
    for (const auto &row : boot_config_data::file_prefixes)
        if (equal_folded(split.directory, row.directory))
        {
            if (!equal_folded(text(config.files.archive_name, 0x44b434u), row.wad))
                load_archive_44b1d0(config, files, row.wad);
            if (config.files.archive)
            {
                Word length = 0;
                (void)lookup_archive_44b4e0(config.files, split.filename, length);
                return length; // offset/result ignored; cached archive miss does NOT loose-fallback for size
            }
            break;
        }
    return files.size_loose_44af60(path);
}
std::uint32_t source_boot_file_read_44b260(SourceBootConfigState &config, SourceBootFilePlatform &files,
                                           std::string_view path, Word requested, std::span<std::uint8_t> destination)
{
    std::size_t initialized_prefix = 0;
    return source_boot_file_read_44b260(config, files, path, requested, destination, initialized_prefix);
}
std::uint32_t source_boot_file_read_44b260(SourceBootConfigState &config, SourceBootFilePlatform &files,
                                           std::string_view path, Word requested, std::span<std::uint8_t> destination,
                                           std::size_t &initialized_prefix)
{
    initialized_prefix = 0;
    return route_read_44b260(
        config, files, path,
        [&](Word offset, Word length) {
            WadSpanDestination target{destination};
            return transfer_wad(config.files, target, offset, requested, length, initialized_prefix, nullptr);
        },
        [&] { return files.read_loose_44afa0(path, requested, destination, initialized_prefix); });
}
std::uint32_t detail::transfer_source_wad_to_bank_44b354(const SourceBootFileState &cache, SourceAllocationBank &bank,
                                                         const SourceAllocationRef &reference, Word member_offset,
                                                         Word requested, Word member_length, std::size_t &copied,
                                                         SourceWadTransferObserver *observer)
{
    WadBankDestination target{bank, reference, std::nullopt};
    return transfer_wad(cache, target, member_offset, requested, member_length, copied, observer);
}
std::uint32_t SourceBootFilePlatform::read_loose_bank_44afa0(std::string_view, std::uint32_t, SourceAllocationBank &,
                                                             const SourceAllocationRef &, std::size_t &copied)
{
    copied = 0;
    throw SourceBootConfigError(0x44b372, "File platform has no bank-destination I/O binding");
}
std::uint32_t source_boot_file_read_bank_44b260(SourceBootConfigState &config, SourceBootFilePlatform &files,
                                                SourceAllocationBank &bank, std::string_view path, Word requested,
                                                const SourceAllocationRef &destination, std::size_t &copied)
{
    copied = 0;
    return route_read_44b260(
        config, files, path,
        [&](Word offset, Word length) {
            WadBankDestination target{bank, destination, std::nullopt};
            return transfer_wad(config.files, target, offset, requested, length, copied, nullptr);
        },
        [&] { return files.read_loose_bank_44afa0(path, requested, bank, destination, copied); });
}

} // namespace sgn::retail
