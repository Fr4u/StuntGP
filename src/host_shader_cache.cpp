#include "sgn/host_shader_cache.hpp"
#include "sgn/host_parallel.hpp"
#include "sgn/host_texture_disk_cache.hpp"
#include <windows.h>
#include <wrl/client.h>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <vector>

// NEW HOST (R35). See host_shader_cache.hpp.
namespace sgn::host_shader_cache
{
namespace
{
using Microsoft::WRL::ComPtr;
constexpr std::uint64_t file_magic = 0x31304853'4E47534Eull; // "NSGNSH01"
constexpr std::uintmax_t folder_budget = 64ull * 1024 * 1024;

struct State
{
    std::mutex mutex;
    std::map<std::string, std::vector<std::uint8_t>> memory; // key material -> bytecode
    Statistics stats;
    bool disk_checked{}, disk_disabled{};
    std::uint64_t compiler{};
};
State &state()
{
    static State *const s = new State; // never destroyed: background writes may run at exit
    return *s;
}
std::uint64_t compiler_identity() noexcept
{
    const auto *base = reinterpret_cast<const std::uint8_t *>(GetModuleHandleW(L"d3dcompiler_47.dll"));
    if (!base)
        return 0;
    const auto *dos = reinterpret_cast<const IMAGE_DOS_HEADER *>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0)
        return 0;
    const auto *nt = reinterpret_cast<const IMAGE_NT_HEADERS *>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE)
        return 0;
    const std::uint32_t words[]{nt->FileHeader.TimeDateStamp, nt->OptionalHeader.SizeOfImage,
                                nt->OptionalHeader.CheckSum, nt->OptionalHeader.AddressOfEntryPoint};
    const auto hash = host_texture_disk_cache::hash_bytes(
        std::span(reinterpret_cast<const std::uint8_t *>(words), sizeof words), 0xD3DC0DE5ull);
    return hash ? hash : 1;
}
void append(std::string &out, const void *data, std::size_t size)
{
    const auto length = static_cast<std::uint64_t>(size);
    out.append(reinterpret_cast<const char *>(&length), sizeof length);
    out.append(static_cast<const char *>(data), size);
}
void append_text(std::string &out, const char *text)
{
    append(out, text ? text : "", text ? std::strlen(text) : 0);
    out.push_back(text ? '\1' : '\0'); // null and "" differ
}
// Everything that decides the bytecode, as one byte string.
std::string key_material(std::uint64_t compiler, LPCVOID source, SIZE_T size, LPCSTR name,
                         const D3D_SHADER_MACRO *defines, LPCSTR entry, LPCSTR target, UINT flags1, UINT flags2)
{
    std::string key;
    append(key, &compiler, sizeof compiler);
    append(key, &flags1, sizeof flags1);
    append(key, &flags2, sizeof flags2);
    append_text(key, target);
    append_text(key, entry);
    append_text(key, name);
    std::uint64_t macros = 0;
    for (auto *d = defines; d && d->Name; ++d)
    {
        append_text(key, d->Name);
        append_text(key, d->Definition);
        ++macros;
    }
    append(key, &macros, sizeof macros);
    append(key, source, size);
    return key;
}
std::filesystem::path folder() noexcept
{
    try
    {
        const auto settings = host_texture_disk_cache::settings_folder();
        return settings.empty() ? std::filesystem::path{} : settings / "shader-cache";
    }
    catch (...)
    {
        return {};
    }
}
bool disk_disabled(State &s) noexcept
{
    if (!s.disk_checked)
    {
        char value[8]{};
        std::size_t size{};
        s.disk_disabled = getenv_s(&size, value, sizeof value, "SGN_SHADER_CACHE") == 0 && size > 1 && value[0] == '0';
        s.disk_checked = true;
    }
    return s.disk_disabled;
}
std::string file_name(const std::string &key)
{
    char text[32]{};
    std::snprintf(text, sizeof text, "%016llx.sgnsh",
                  static_cast<unsigned long long>(host_texture_disk_cache::hash_bytes(
                      std::span(reinterpret_cast<const std::uint8_t *>(key.data()), key.size()), 0x5AD3C0DEull)));
    return text;
}
// A valid file for exactly this key, else nothing.
std::vector<std::uint8_t> load(const std::filesystem::path &path, const std::string &key, bool &rejected)
{
    rejected = false;
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in)
        return {};
    const auto size = static_cast<std::uint64_t>(in.tellg());
    in.seekg(0);
    std::uint64_t head[4]{}; // magic, key bytes, code bytes, code checksum
    rejected = true;
    if (size < sizeof head || !in.read(reinterpret_cast<char *>(head), sizeof head) || head[0] != file_magic ||
        head[1] != key.size() || size != sizeof head + head[1] + head[2] || head[2] == 0 || head[2] > (64u << 20))
        return {};
    std::string stored(static_cast<std::size_t>(head[1]), '\0');
    if (!in.read(stored.data(), static_cast<std::streamsize>(stored.size())) || stored != key)
        return {};
    std::vector<std::uint8_t> code(static_cast<std::size_t>(head[2]));
    if (!in.read(reinterpret_cast<char *>(code.data()), static_cast<std::streamsize>(code.size())) ||
        host_texture_disk_cache::hash_bytes(code, 0xB17EC0DEull) != head[3])
        return {};
    rejected = false;
    return code;
}
void store(std::filesystem::path dir, std::string name, std::string key, std::vector<std::uint8_t> code) noexcept
{
    (void)host_parallel::post_background([dir, name, key, code] {
        std::error_code error;
        std::filesystem::create_directories(dir, error);
        const auto final_path = dir / name;
        if (std::filesystem::exists(final_path, error))
            return;
        auto temporary = final_path;
        temporary += "." + std::to_string(GetCurrentThreadId()) + ".tmp";
        const std::uint64_t head[4]{file_magic, key.size(), code.size(),
                                    host_texture_disk_cache::hash_bytes(code, 0xB17EC0DEull)};
        bool ok = false;
        {
            std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
            out.write(reinterpret_cast<const char *>(head), sizeof head);
            out.write(key.data(), static_cast<std::streamsize>(key.size()));
            out.write(reinterpret_cast<const char *>(code.data()), static_cast<std::streamsize>(code.size()));
            ok = static_cast<bool>(out.flush());
        }
        if (ok)
            std::filesystem::rename(temporary, final_path, error); // replaces an existing file
        if (!ok || error)
            std::filesystem::remove(temporary, error);
        else
        {
            auto &s = state();
            const std::lock_guard lock(s.mutex);
            ++s.stats.written;
        }
        // Keep the folder bounded (oldest entries first; only this cache's files).
        std::uintmax_t total = 0;
        std::vector<std::pair<std::filesystem::file_time_type, std::filesystem::path>> files;
        for (std::filesystem::directory_iterator it(dir, error), end; !error && it != end; it.increment(error))
            if (it->path().extension() == ".sgnsh")
            {
                std::error_code e;
                total += it->file_size(e);
                files.emplace_back(it->last_write_time(e), it->path());
            }
        if (total > folder_budget)
        {
            std::sort(files.begin(), files.end());
            for (const auto &[time, path] : files)
            {
                if (total <= folder_budget * 3 / 4)
                    break;
                std::error_code e;
                const auto bytes = std::filesystem::file_size(path, e);
                if (!e && std::filesystem::remove(path, e))
                    total -= bytes;
            }
        }
    });
}
HRESULT make_blob(const std::vector<std::uint8_t> &bytes, ID3DBlob **code) noexcept
{
    ComPtr<ID3DBlob> blob;
    const auto status = D3DCreateBlob(bytes.size(), &blob);
    if (FAILED(status))
        return status;
    std::memcpy(blob->GetBufferPointer(), bytes.data(), bytes.size());
    *code = blob.Detach();
    return S_OK;
}
} // namespace

HRESULT compile(LPCVOID source, SIZE_T size, LPCSTR name, const D3D_SHADER_MACRO *defines, LPCSTR entry, LPCSTR target,
                UINT flags1, UINT flags2, ID3DBlob **code, ID3DBlob **errors) noexcept
{
    const auto direct = [&] {
        return D3DCompile(source, size, name, defines, nullptr, entry, target, flags1, flags2, code, errors);
    };
    if (!code)
        return direct();
    auto &s = state();
    bool compiled = false; // D3DCompile ran: its result is returned whatever the cache does afterwards
    HRESULT status = E_FAIL;
    try
    {
        std::uint64_t compiler{};
        {
            const std::lock_guard lock(s.mutex);
            if (!s.compiler)
                s.compiler = compiler_identity();
            compiler = s.compiler;
        }
        if (!compiler)
            return direct(); // unknown compiler identity: never cache
        const auto key = key_material(compiler, source, size, name, defines, entry, target, flags1, flags2);
        {
            const std::lock_guard lock(s.mutex);
            if (const auto found = s.memory.find(key); found != s.memory.end())
            {
                const auto made = make_blob(found->second, code);
                if (SUCCEEDED(made))
                {
                    ++s.stats.memory_hits;
                    if (errors)
                        *errors = nullptr;
                    return made;
                }
            }
        }
        const auto dir = folder();
        const bool disk = !dir.empty() && !disk_disabled(s);
        const auto name_on_disk = disk ? file_name(key) : std::string{};
        if (disk)
        {
            bool rejected = false;
            auto bytes = load(dir / name_on_disk, key, rejected);
            if (!bytes.empty())
            {
                const auto made = make_blob(bytes, code);
                if (SUCCEEDED(made))
                {
                    if (errors)
                        *errors = nullptr;
                    const std::lock_guard lock(s.mutex);
                    ++s.stats.disk_hits;
                    s.memory.emplace(key, std::move(bytes));
                    return made;
                }
            }
            if (rejected)
            {
                const std::lock_guard lock(s.mutex);
                ++s.stats.disk_rejected;
            }
        }
        const auto start = std::chrono::steady_clock::now();
        status = direct();
        compiled = true;
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        if (SUCCEEDED(status) && *code)
        {
            const auto *data = static_cast<const std::uint8_t *>((*code)->GetBufferPointer());
            std::vector<std::uint8_t> bytes(data, data + (*code)->GetBufferSize());
            {
                const std::lock_guard lock(s.mutex);
                ++s.stats.compiled;
                s.stats.compile_ms += ms;
                s.memory.emplace(key, bytes);
            }
            if (disk)
                store(dir, name_on_disk, key, std::move(bytes));
        }
        return status;
    }
    catch (...)
    {
        return compiled ? status : direct(); // a cache failure never changes the compile result
    }
}
Statistics statistics() noexcept
{
    auto &s = state();
    const std::lock_guard lock(s.mutex);
    return s.stats;
}
} // namespace sgn::host_shader_cache
