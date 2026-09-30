#include "sgn/source_boot_config.hpp"
#include <algorithm>
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <share.h>
#include <string>
#include <utility>

namespace sgn::retail
{
namespace
{
struct CloseFile
{
    void operator()(std::FILE *file) const noexcept
    {
        if (file)
            (void)std::fclose(file);
    }
};
using File = std::unique_ptr<std::FILE, CloseFile>;
File open_binary(const std::filesystem::path &path)
{
    static_assert(_SH_DENYNO == 0x40); // 462AC3 pushes40 before462AA3
    return File(::_wfsopen(path.c_str(), L"rb", _SH_DENYNO));
}
} // namespace
NativeSourceBootFilePlatform::NativeSourceBootFilePlatform(std::filesystem::path working_directory)
    : source_working_directory_(std::move(working_directory))
{
    if (!source_working_directory_.is_absolute())
        throw std::invalid_argument("Source boot working directory must be explicit and absolute");
}
NativeSourceBootFilePlatform::NativeSourceBootFilePlatform(std::filesystem::path local_retail_directory,
                                                           const SourceBootConfigState &source_state)
    : NativeSourceBootFilePlatform(std::move(local_retail_directory))
{
    relocated_source_state_ = &source_state;
}
std::filesystem::path NativeSourceBootFilePlatform::resolve(std::string_view spelling) const
{
    if (relocated_source_state_)
    {
        const auto &bytes = relocated_source_state_->base_path;
        const auto end = std::find(bytes.begin(), bytes.end(), '\0');
        if (end == bytes.end())
            throw SourceBootConfigError(0x462ac3u, "Relocated source base lacks terminator");
        std::string_view prefix(bytes.data(), static_cast<std::size_t>(end - bytes.begin()));
        const auto slash = [](char c) { return c == '\\' || c == '/'; };
        const auto folded = [&](char c) {
            if (slash(c))
                return '\\';
            return c >= 'A' && c <= 'Z' ? static_cast<char>(c + ('a' - 'A')) : c;
        };
        bool matches = !prefix.empty() && spelling.size() >= prefix.size();
        for (std::size_t i = 0; matches && i < prefix.size(); ++i)
            matches = folded(spelling[i]) == folded(prefix[i]);
        if (matches && (slash(prefix.back()) || spelling.size() == prefix.size() || slash(spelling[prefix.size()])))
        {
            spelling.remove_prefix(prefix.size());
            while (!spelling.empty() && slash(spelling.front()))
                spelling.remove_prefix(1);
            // Prefix membership is established before stripping. No suffix
            // search or remapping of unrelated drives and folders.
        }
    }
    std::wstring wide;
    for (const unsigned char c : spelling)
    {
        if (c == 0 || c >= 128u)
            throw SourceBootConfigError(0x462ac3u, "Source file spelling outside admitted ASCII path domain");
        wide.push_back(static_cast<wchar_t>(c));
    }
    std::filesystem::path path(wide);
    return path.is_absolute() ? path : source_working_directory_ / path;
}
std::uint32_t NativeSourceBootFilePlatform::size_loose_44af60(std::string_view spelling)
{
    static_assert(sizeof(long) == 4, "Source fseek/ftell requires Windows32-bit long semantics");
    auto file = open_binary(resolve(spelling));
    if (!file)
        return 0;
    (void)std::fseek(file.get(), 0, SEEK_END); // source ignores seek return
    const auto size = std::ftell(file.get());
    return static_cast<std::uint32_t>(size); // source ftell error(-1) remainsFFFFFFFF
}
std::uint32_t NativeSourceBootFilePlatform::read_loose_44afa0(std::string_view spelling, std::uint32_t requested,
                                                              std::span<std::uint8_t> destination, std::size_t &copied)
{
    copied = 0;
    auto file = open_binary(resolve(spelling));
    if (!file)
        return 0;
    const auto count = std::bit_cast<std::int32_t>(requested) > 0 ? requested : 0x200000u;
    const auto safe = std::min<std::size_t>(count, destination.size());
    if (safe != 0)
        copied = std::fread(destination.data(), 1, safe, file.get());
    if (copied == safe && safe < count)
    {
        // Size/read race or count<=0 may request2MiB into a smaller real bank
        // payload. Preserve the safe prefix, reject only if another byte exists.
        const int next = std::fgetc(file.get());
        if (next != EOF)
            throw SourceBootConfigError(0x44afccu, "Original fread would write beyond supplied allocation");
    }
    return static_cast<std::uint32_t>(copied); // binary fread(size1), actual byte count
}
// NEW host implementation of44AFA0's file read boundary. Uses actual CRT
// FILE and bank storage, preserving open failure/EOF without destination access.
// fgetc+fread transport does not emulate private retail CRT buffering/syscalls.
std::uint32_t NativeSourceBootFilePlatform::read_loose_bank_44afa0(std::string_view spelling, std::uint32_t requested,
                                                                   SourceAllocationBank &bank,
                                                                   const SourceAllocationRef &destination,
                                                                   std::size_t &copied)
{
    copied = 0;
    auto file = open_binary(resolve(spelling));
    if (!file)
        return 0; // 44AFB5: no bank borrow on open failure.
    const auto count = std::bit_cast<std::int32_t>(requested) > 0 ? requested : 0x200000u;
    const int first = std::fgetc(file.get());
    if (first == EOF)
        return 0; // No destination write if no byte was read.
    std::span<std::uint8_t> bytes;
    try
    {
        bytes = bank.bytes(destination);
    }
    catch (const SourceAllocationError &error)
    {
        throw SourceBootConfigError(0x44afcc, error.what());
    }
    if (bytes.empty())
        throw SourceBootConfigError(0x44afcc, "Loose read reaches a zero-size bank destination");
    bytes[0] = static_cast<std::uint8_t>(first);
    copied = 1;
    const auto safe = std::min<std::size_t>(count, bytes.size());
    if (safe > 1)
        copied += std::fread(bytes.data() + 1, 1, safe - 1, file.get());
    if (copied == safe && safe < count)
    {
        const int next = std::fgetc(file.get());
        if (next != EOF)
            throw SourceBootConfigError(0x44afcc, "Loose read exceeds actual bank destination after written prefix");
    }
    return static_cast<std::uint32_t>(copied);
}
SourceBootRawAllocation NativeSourceBootFilePlatform::allocate_wad_raw(std::uint32_t bytes)
{
    return SourceBootRawAllocation(static_cast<std::uint8_t *>(std::malloc(bytes)));
}
} // namespace sgn::retail
