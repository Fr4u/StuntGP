#include "sgn/persistence_file_service.hpp"
#include <algorithm>
#include <fstream>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace sgn
{
using namespace retail;
namespace
{
void native_error(const char *what)
{
    throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), what);
}
void no_reparse(const std::filesystem::path &path)
{
    const auto attributes = GetFileAttributesW(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES)
    {
        const auto error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND)
            return;
        native_error("persistence path attributes");
    }
    if (attributes & FILE_ATTRIBUTE_REPARSE_POINT)
        throw std::runtime_error("persistence path is a reparse point");
}
void no_reparse_chain(std::filesystem::path path)
{
    for (;;)
    {
        no_reparse(path);
        const auto parent = path.parent_path();
        if (parent.empty() || parent == path)
            break;
        path = parent;
    }
}
void replace_file(const std::filesystem::path &path, std::span<const std::uint8_t> bytes, std::uint64_t &sequence)
{
    no_reparse(path.parent_path());
    no_reparse(path);
    HANDLE handle = INVALID_HANDLE_VALUE;
    std::filesystem::path temporary;
    for (unsigned attempt = 0; attempt < 256; ++attempt)
    {
        temporary =
            path.parent_path() / (L"." + path.filename().wstring() + L".sgn." + std::to_wstring(GetCurrentProcessId()) +
                                  L"." + std::to_wstring(++sequence) + L".tmp");
        handle =
            CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_TEMPORARY, nullptr);
        if (handle != INVALID_HANDLE_VALUE)
            break;
        if (GetLastError() != ERROR_FILE_EXISTS)
            native_error("create persistence temporary file");
    }
    if (handle == INVALID_HANDLE_VALUE)
        throw std::runtime_error("persistence temporary name exhaustion");
    try
    {
        std::size_t offset{};
        while (offset < bytes.size())
        {
            const auto wanted =
                static_cast<DWORD>(std::min<std::size_t>(bytes.size() - offset, std::numeric_limits<DWORD>::max()));
            DWORD count{};
            if (!WriteFile(handle, bytes.data() + offset, wanted, &count, nullptr))
                native_error("write persistence temporary file");
            if (count == 0)
                throw std::runtime_error("zero progress writing persistence file");
            offset += count;
        }
        if (!FlushFileBuffers(handle))
            native_error("flush persistence temporary file");
        if (!CloseHandle(handle))
        {
            handle = INVALID_HANDLE_VALUE;
            native_error("close persistence temporary file");
        }
        handle = INVALID_HANDLE_VALUE;
        no_reparse(path.parent_path());
        no_reparse(path);
        if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            native_error("replace persistence file");
    }
    catch (...)
    {
        if (handle != INVALID_HANDLE_VALUE)
            CloseHandle(handle);
        DeleteFileW(temporary.c_str());
        throw;
    }
}
} // namespace
struct PersistenceFileService::Impl
{
    struct File
    {
        std::filesystem::path path;
        std::vector<std::uint8_t> bytes;
        std::size_t offset{}, limit{};
        bool writing{}, text{};
    };
    std::filesystem::path root;
    std::map<PersistenceFileHandle, File> files;
    PersistenceFileHandle next{1};
    std::uint64_t sequence{};
    explicit Impl(std::filesystem::path value) : root(std::move(value))
    {
        if (!root.is_absolute() || root == root.root_path())
            throw std::invalid_argument("persistence root must name an explicit absolute directory");
        root = root.lexically_normal();
        no_reparse_chain(root);
        if (!std::filesystem::is_directory(root))
            throw std::invalid_argument("persistence root must already exist");
    }
    std::filesystem::path card_path(std::uint32_t card) const
    {
        if (card > 8)
            throw std::out_of_range("persistence card selector outside host range0..8");
        no_reparse_chain(root);
        const auto parent = root / L"save";
        no_reparse(parent);
        const auto result = parent / (L"card0" + std::to_wstring(card));
        no_reparse(result);
        return result;
    }
    std::filesystem::path path(PersistenceFile file) const
    {
        no_reparse_chain(root);
        std::filesystem::path result;
        switch (file.kind)
        {
        case PersistenceFileKind::setup:
            result = card_path(file.card) / L"setup.bin";
            break;
        case PersistenceFileKind::detail:
            result = root / L"detail.cfg";
            break;
        case PersistenceFileKind::language:
            result = root / L"language.cfg";
            break;
        default:
            throw std::invalid_argument("unknown persistence file kind");
        }
        no_reparse(result);
        return result;
    }
    std::filesystem::path replay_path(std::uint32_t card, std::string_view name) const
    {
        // Same original NEW host admission shared by existence and CRT route.
        if (name.size() < 10 || name.size() > 20 || !name.starts_with("ghost") || !name.ends_with(".bin"))
            throw std::invalid_argument("replay basename leaves the source ghost file domain");
        auto digits = name.substr(5, name.size() - 9);
        if (digits.front() == '-')
            digits.remove_prefix(1);
        if (digits.empty() || !std::all_of(digits.begin(), digits.end(), [](char c) { return c >= '0' && c <= '9'; }))
            throw std::invalid_argument("replay basename contains non-decimal path bytes");
        const auto result = card_path(card) / std::wstring(name.begin(), name.end());
        no_reparse(result);
        return result;
    }
    File &get(PersistenceFileHandle handle)
    {
        const auto found = files.find(handle);
        if (found == files.end())
            throw std::logic_error("unknown persistence file handle");
        return found->second;
    }
};
PersistenceFileService::PersistenceFileService(std::filesystem::path root)
    : impl_(std::make_unique<Impl>(std::move(root)))
{
}
PersistenceFileService::~PersistenceFileService() = default;
void PersistenceFileService::format_card(std::uint32_t card)
{
    const auto path = impl_->card_path(card);
    std::filesystem::create_directories(path);
    (void)impl_->card_path(card);
}
bool PersistenceFileService::card_directory_exists(std::uint32_t card)
{
    return std::filesystem::is_directory(impl_->card_path(card));
}
std::int32_t PersistenceFileService::file_size(PersistenceFile file)
{
    const auto path = impl_->path(file);
    if (!std::filesystem::is_regular_file(path))
        return -1;
    const auto size = std::filesystem::file_size(path);
    if (size > std::numeric_limits<std::int32_t>::max())
        throw std::runtime_error("persistence file exceeds source signed-size domain");
    return static_cast<std::int32_t>(size);
}
PersistenceFileHandle PersistenceFileService::open(PersistenceFile name, PersistenceAccess access)
{
    const auto path = impl_->path(name);
    const bool writing = access == PersistenceAccess::write_binary || access == PersistenceAccess::write_text;
    const bool text = access == PersistenceAccess::read_text || access == PersistenceAccess::write_text;
    const std::size_t limit = name.kind == PersistenceFileKind::setup ? persistence_size : 4096;
    if (!std::filesystem::is_directory(path.parent_path()) || std::filesystem::is_directory(path))
        return 0;
    for (const auto &[id, file] : impl_->files)
    {
        (void)id;
        if (file.path == path)
            throw std::logic_error("persistence file already open");
    }
    Impl::File file{path, {}, 0, limit, writing, text};
    if (!writing)
    {
        if (!std::filesystem::is_regular_file(path))
            return 0;
        const auto size = std::filesystem::file_size(path);
        if (size > limit)
            throw std::runtime_error("persistence input exceeds bounded host file size");
        std::ifstream input(path, std::ios::binary);
        if (!input)
            return 0;
        file.bytes.resize(static_cast<std::size_t>(size));
        input.read(reinterpret_cast<char *>(file.bytes.data()), static_cast<std::streamsize>(file.bytes.size()));
        file.bytes.resize(static_cast<std::size_t>(input.gcount()));
        if (input.bad())
            throw std::runtime_error("persistence input read failed");
    }
    const auto handle = impl_->next++;
    impl_->files.emplace(handle, std::move(file));
    return handle;
}
std::size_t PersistenceFileService::read(PersistenceFileHandle handle, std::span<std::uint8_t> output)
{
    auto &file = impl_->get(handle);
    if (file.writing)
        throw std::logic_error("read from persistence writer");
    const auto count = std::min(output.size(), file.bytes.size() - file.offset);
    std::copy_n(file.bytes.begin() + static_cast<std::ptrdiff_t>(file.offset), count, output.begin());
    file.offset += count;
    return count;
}
std::size_t PersistenceFileService::write(PersistenceFileHandle handle, std::span<const std::uint8_t> input)
{
    auto &file = impl_->get(handle);
    if (!file.writing)
        throw std::logic_error("write to persistence reader");
    if (input.size() > file.limit - file.bytes.size())
        throw std::length_error("persistence write exceeds host bound");
    file.bytes.insert(file.bytes.end(), input.begin(), input.end());
    return input.size();
}
void PersistenceFileService::scan_decimal(PersistenceFileHandle handle, std::int32_t &value)
{
    auto &file = impl_->get(handle);
    if (file.writing || !file.text)
        throw std::logic_error("decimal scan requires persistence text reader");
    (void)scan_persistence_decimal(std::span<const std::uint8_t>(file.bytes).subspan(file.offset), value);
    file.offset = file.bytes.size();
}
void PersistenceFileService::print_decimal(PersistenceFileHandle handle, std::int32_t value)
{
    const auto text = format_persistence_decimal(value);
    (void)write(handle, {reinterpret_cast<const std::uint8_t *>(text.data()), text.size()});
}
void PersistenceFileService::close(PersistenceFileHandle handle)
{
    auto file = std::move(impl_->get(handle));
    impl_->files.erase(handle);
    if (file.writing)
    {
        no_reparse_chain(impl_->root);
        if (file.path.parent_path() != impl_->root)
            no_reparse(impl_->root / L"save");
        replace_file(file.path, file.bytes, impl_->sequence);
    }
}
bool PersistenceFileService::source_setup_exists(std::uint32_t card)
{
    const auto path = impl_->path({PersistenceFileKind::setup, card});
    const auto attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}
bool PersistenceFileService::source_replay_exists(std::uint32_t card, std::string_view name)
{
    const auto path = impl_->replay_path(card, name);
    const auto attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}
std::vector<std::uint8_t> PersistenceFileService::source_replay_native_path(std::uint32_t card, std::string_view name)
{
    // NEW transport must not leak path-admission/encoding calls into the next
    // source import's last-error observation. Source errno/doserrno are not touched.
    struct RestoreError
    {
        DWORD value;
        ~RestoreError()
        {
            SetLastError(value);
        }
    } restore{GetLastError()};
    const auto path = impl_->replay_path(card, name).wstring();
    if (path.size() >= static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
        std::find(path.begin(), path.end(), wchar_t{0}) != path.end())
        throw std::invalid_argument("private replay path leaves the bounded native string domain");
    const auto codepage = AreFileApisANSI() ? GetACP() : GetOEMCP();
    const auto count = WideCharToMultiByte(codepage, 0, path.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (count <= 0)
        native_error("encode private replay path");
    std::vector<std::uint8_t> result(static_cast<std::size_t>(count));
    if (WideCharToMultiByte(codepage, 0, path.c_str(), -1, reinterpret_cast<char *>(result.data()), count, nullptr,
                            nullptr) != count)
        native_error("encode private replay path bytes");
    const auto restored_count =
        MultiByteToWideChar(codepage, 0, reinterpret_cast<const char *>(result.data()), count, nullptr, 0);
    if (restored_count <= 0)
        native_error("decode private replay path");
    std::wstring restored(static_cast<std::size_t>(restored_count), wchar_t{0});
    if (MultiByteToWideChar(codepage, 0, reinterpret_cast<const char *>(result.data()), count, restored.data(),
                            restored_count) != restored_count)
        native_error("decode private replay path bytes");
    if (restored.size() != path.size() + 1u || restored.back() != 0 ||
        !std::equal(path.begin(), path.end(), restored.begin()))
        throw std::runtime_error("private replay path cannot round-trip through current Win32 A encoding");
    return result;
}
std::uint32_t PersistenceFileService::source_create_card(std::uint32_t card,
                                                         const std::function<void(std::uint32_t)> &report_os_error)
{
    const auto path = impl_->card_path(card);
    if (CreateDirectoryW(path.c_str(), nullptr) != 0)
        return 0;
    const auto error = GetLastError();
    report_os_error(error);
    return 8;
}
std::uint32_t PersistenceFileService::source_card_used_bytes(std::uint32_t card,
                                                             const std::function<void(std::uint32_t)> &report_os_error)
{
    const auto directory = impl_->card_path(card);
    const auto attributes = GetFileAttributesW(directory.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
        return 0xffffffffu;
    WIN32_FIND_DATAW data{};
    const auto pattern = directory / L"*.*";
    const auto handle = FindFirstFileW(pattern.c_str(), &data);
    if (handle == INVALID_HANDLE_VALUE)
    {
        report_os_error(GetLastError());
        return 0;
    }
    std::uint32_t total = 0;
    for (;;)
    {
        if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
            total += data.nFileSizeLow;
        if (FindNextFileW(handle, &data) == 0)
        {
            report_os_error(GetLastError());
            break;
        }
    }
    if (FindClose(handle) == 0)
        report_os_error(GetLastError());
    return total; // source32-bit sum wraps; no recursion or filesystem free-space substitution
}

} // namespace sgn
