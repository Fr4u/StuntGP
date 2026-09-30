#include "sgn/host_texture_disk_cache.hpp"
#include "sgn/host_parallel.hpp"
#include <algorithm>
#include <atomic>
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <mutex>
#include <string>
#include <system_error>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#endif

// NEW HOST (R30). See host_texture_disk_cache.hpp.
namespace sgn::host_texture_disk_cache
{
namespace
{
constexpr std::uint64_t magic = 0x31305843'54544E47ull; // "GNTTCX01"
constexpr std::uint32_t format_version = 1;
constexpr std::uintmax_t folder_budget = 512ull * 1024 * 1024;
constexpr std::uintmax_t folder_target = 384ull * 1024 * 1024;
constexpr std::size_t pending_budget = 256u * 1024u * 1024u;
constexpr std::size_t header_words = 17; // u64 fields below

std::uint64_t mix(std::uint64_t h, std::uint64_t v) noexcept
{
    h ^= v * 0x9E3779B97F4A7C15ull;
    return std::rotl(h, 27) * 0x94D049BB133111EBull + 0x632BE59BD9B4E019ull;
}
std::uint64_t finish(std::uint64_t h) noexcept
{
    h ^= h >> 31;
    h *= 0xBF58476D1CE4E5B9ull;
    h ^= h >> 29;
    return h;
}

struct State
{
    std::mutex mutex;
    bool configured{};
    bool enabled{};
    std::filesystem::path folder; // <configured>/<identity hex>
    std::filesystem::path root;
    std::filesystem::path settings; // NEW HOST (R35) the configured folder's parent (settings_folder)
    std::uint64_t identity{};
    std::size_t pending_bytes{};
    std::uintmax_t written_since_prune{};
    Statistics stats;
};
State &state()
{
    // NEW HOST (R35): never destroyed. Background prewarm and cache-write tasks may still run while the
    // process exits; a destroyed state under them was an access violation at exit.
    static State *const s = new State;
    return *s;
}

std::uint64_t module_identity() noexcept
{
#ifdef _WIN32
    const auto *base = reinterpret_cast<const std::uint8_t *>(GetModuleHandleW(nullptr));
    if (!base)
        return 0;
    const auto *dos = reinterpret_cast<const IMAGE_DOS_HEADER *>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0)
        return 0;
    const auto *nt = reinterpret_cast<const IMAGE_NT_HEADERS *>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE)
        return 0;
    std::uint64_t h = 0x243F6A8885A308D3ull;
    h = mix(h, nt->FileHeader.TimeDateStamp);
    h = mix(h, nt->FileHeader.Machine);
    h = mix(h, nt->OptionalHeader.SizeOfImage);
    h = mix(h, nt->OptionalHeader.CheckSum);
    h = mix(h, nt->OptionalHeader.SizeOfCode);
    h = mix(h, nt->OptionalHeader.AddressOfEntryPoint);
    h = mix(h, format_version);
    const auto identity = finish(h);
    return identity ? identity : 1;
#else
    return 0; // no executable identity: the cache stays disabled
#endif
}
std::string hex(std::uint64_t value)
{
    char text[17]{};
    std::snprintf(text, sizeof text, "%016llx", static_cast<unsigned long long>(value));
    return text;
}
bool is_hex_name(const std::string &name)
{
    return name.size() == 16 && std::all_of(name.begin(), name.end(),
                                            [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); });
}
bool environment_disables() noexcept
{
#ifdef _WIN32
    char value[8]{};
    std::size_t size{};
    if (getenv_s(&size, value, sizeof value, "SGN_TEXTURE_CACHE") != 0 || size <= 1)
        return false;
    return value[0] == '0';
#else
    const char *text = std::getenv("SGN_TEXTURE_CACHE");
    return text && text[0] == '0';
#endif
}
std::uint64_t key_hash(const Key &key, std::uint64_t identity) noexcept
{
    std::uint64_t h = mix(identity, key.kind);
    for (const auto p : key.params)
        h = mix(h, p);
    h = mix(h, (std::uint64_t(key.width) << 32) | key.height);
    return hash_words(key.source, finish(h));
}
std::array<std::uint64_t, header_words> header(const Key &key, std::uint64_t identity, std::uint64_t payload_bytes,
                                               std::uint64_t payload_hash) noexcept
{
    return {magic,
            format_version,
            key.kind,
            identity,
            key.params[0],
            key.params[1],
            key.params[2],
            key.params[3],
            key.width,
            key.height,
            key.source.size(),
            payload_bytes,
            payload_hash,
            hash_words(key.source, 0x51ED270B27A1F3C5ull),
            0,
            0,
            0};
}
// Removes the files this cache writes: other builds' folders entirely, and the oldest entries of the
// current build beyond the budget. Background thread only.
void prune(const std::filesystem::path &root, const std::filesystem::path &current) noexcept
{
    try
    {
        std::error_code error;
        const auto ours = [](const std::filesystem::path &p) {
            const auto extension = p.extension().string();
            return extension == ".sgntex" || extension == ".sgntmp";
        };
        for (std::filesystem::directory_iterator it(root, error), end; !error && it != end; it.increment(error))
        {
            const auto &entry = *it;
            if (!entry.is_directory(error) || entry.path() == current || !is_hex_name(entry.path().filename().string()))
                continue;
            for (std::filesystem::directory_iterator file(entry.path(), error), last; !error && file != last;
                 file.increment(error))
                if (ours(file->path()))
                    std::filesystem::remove(file->path(), error);
            error.clear();
            std::filesystem::remove(entry.path(), error); // only when now empty
            error.clear();
        }
        error.clear();
        struct File
        {
            std::filesystem::path path;
            std::filesystem::file_time_type time;
            std::uintmax_t size;
        };
        std::vector<File> files;
        std::uintmax_t total = 0;
        for (std::filesystem::directory_iterator file(current, error), last; !error && file != last;
             file.increment(error))
        {
            if (!ours(file->path()))
                continue;
            std::error_code e;
            const auto size = file->file_size(e);
            const auto time = file->last_write_time(e);
            if (e)
                continue;
            if (file->path().extension() == ".sgntmp" &&
                std::filesystem::file_time_type::clock::now() - time > std::chrono::minutes(10)) // abandoned write
            {
                std::filesystem::remove(file->path(), e);
                continue;
            }
            files.push_back({file->path(), time, size});
            total += size;
        }
        if (total <= folder_budget)
            return;
        std::sort(files.begin(), files.end(), [](const File &a, const File &b) { return a.time < b.time; });
        for (const auto &file : files)
        {
            if (total <= folder_target)
                break;
            std::error_code e;
            if (file.path.extension() == ".sgntex" && std::filesystem::remove(file.path, e))
                total -= file.size;
        }
    }
    catch (...)
    {
    }
}
} // namespace

std::uint64_t hash_words(std::span<const std::uint16_t> words, std::uint64_t seed) noexcept
{
    std::uint64_t h = mix(seed, words.size());
    std::size_t i = 0;
    for (; i + 4 <= words.size(); i += 4)
    {
        std::uint64_t v;
        std::memcpy(&v, words.data() + i, sizeof v);
        h = mix(h, v);
    }
    std::uint64_t tail = 0;
    for (unsigned shift = 0; i < words.size(); ++i, shift += 16)
        tail |= std::uint64_t(words[i]) << shift;
    return finish(mix(h, tail));
}
std::uint64_t hash_bytes(std::span<const std::uint8_t> bytes, std::uint64_t seed) noexcept
{
    std::uint64_t h = mix(seed, bytes.size());
    std::size_t i = 0;
    for (; i + 8 <= bytes.size(); i += 8)
    {
        std::uint64_t v;
        std::memcpy(&v, bytes.data() + i, sizeof v);
        h = mix(h, v);
    }
    std::uint64_t tail = 0;
    for (unsigned shift = 0; i < bytes.size(); ++i, shift += 8)
        tail |= std::uint64_t(bytes[i]) << shift;
    return finish(mix(h, tail));
}
void configure(const std::filesystem::path &folder) noexcept
{
    try
    {
        auto &s = state();
        std::filesystem::path root, current;
        {
            const std::lock_guard lock(s.mutex);
            if (s.configured)
                return;
            s.configured = true;
            s.settings = folder.parent_path(); // NEW HOST (R35): for the shader cache, whatever the switch
            if (folder.empty() || environment_disables())
                return;
            s.identity = module_identity();
            if (!s.identity)
                return;
            s.root = folder;
            s.folder = folder / hex(s.identity);
            s.enabled = true;
            root = s.root;
            current = s.folder;
        }
        host_parallel::post_background([root, current] { prune(root, current); });
    }
    catch (...)
    {
    }
}
std::filesystem::path settings_folder()
{
    auto &s = state();
    const std::lock_guard lock(s.mutex);
    return s.settings;
}
bool enabled() noexcept
{
    auto &s = state();
    const std::lock_guard lock(s.mutex);
    return s.enabled;
}
std::optional<std::vector<std::uint8_t>> load(const Key &key) noexcept
{
    auto &s = state();
    std::filesystem::path folder;
    std::uint64_t identity{};
    {
        const std::lock_guard lock(s.mutex);
        if (!s.enabled)
            return std::nullopt;
        folder = s.folder;
        identity = s.identity;
    }
    const auto count = [&](std::uint64_t Statistics::*field) {
        const std::lock_guard lock(s.mutex);
        ++(s.stats.*field);
    };
    try
    {
        const auto path = folder / (hex(key_hash(key, identity)) + ".sgntex");
        std::ifstream in(path, std::ios::binary | std::ios::ate);
        if (!in)
        {
            count(&Statistics::misses);
            return std::nullopt;
        }
        const auto file_size = static_cast<std::uint64_t>(in.tellg());
        in.seekg(0);
        std::array<std::uint64_t, header_words> stored{};
        const auto header_bytes = sizeof stored;
        if (file_size < header_bytes || !in.read(reinterpret_cast<char *>(stored.data()), header_bytes))
        {
            count(&Statistics::rejected);
            return std::nullopt;
        }
        const auto payload_bytes = stored[11];
        const auto expected = header(key, identity, payload_bytes, stored[12]);
        const auto source_bytes = std::uint64_t(key.source.size()) * sizeof(std::uint16_t);
        if (stored != expected || payload_bytes > (1ull << 32) ||
            file_size != header_bytes + source_bytes + payload_bytes)
        {
            count(&Statistics::rejected);
            return std::nullopt;
        }
        std::vector<std::uint16_t> source(key.source.size());
        if (!in.read(reinterpret_cast<char *>(source.data()), static_cast<std::streamsize>(source_bytes)) ||
            !std::equal(source.begin(), source.end(), key.source.begin(), key.source.end()))
        {
            count(&Statistics::rejected);
            return std::nullopt;
        }
        std::vector<std::uint8_t> payload(static_cast<std::size_t>(payload_bytes));
        if (!in.read(reinterpret_cast<char *>(payload.data()), static_cast<std::streamsize>(payload_bytes)) ||
            hash_bytes(payload, 0x2545F4914F6CDD1Dull) != stored[12])
        {
            count(&Statistics::rejected);
            return std::nullopt;
        }
        count(&Statistics::hits);
        return payload;
    }
    catch (...)
    {
        count(&Statistics::rejected);
        return std::nullopt;
    }
}
void store(const Key &key, std::vector<std::uint8_t> payload) noexcept
{
    auto &s = state();
    try
    {
        std::filesystem::path folder, root;
        std::uint64_t identity{};
        const std::size_t bytes = payload.size() + key.source.size_bytes();
        {
            const std::lock_guard lock(s.mutex);
            if (!s.enabled)
                return;
            if (s.pending_bytes + bytes > pending_budget)
            {
                ++s.stats.dropped;
                return;
            }
            s.pending_bytes += bytes;
            folder = s.folder;
            root = s.root;
            identity = s.identity;
        }
        std::vector<std::uint16_t> source(key.source.begin(), key.source.end());
        Key stored_key = key;
        auto task = [&s, folder, root, identity, stored_key, bytes, source = std::move(source),
                     payload = std::move(payload)]() mutable {
            Key k = stored_key;
            k.source = source;
            bool ok = false;
            try
            {
                std::error_code error;
                std::filesystem::create_directories(folder, error);
                const auto name = hex(key_hash(k, identity));
                const auto final_path = folder / (name + ".sgntex");
                if (std::filesystem::exists(final_path, error))
                    ok = true; // another run or request wrote it
                else
                {
                    auto temporary = folder / (name + "-" +
                                               hex(std::hash<std::thread::id>{}(std::this_thread::get_id()) ^
                                                   reinterpret_cast<std::uintptr_t>(&k)) +
                                               ".sgntmp");
                    const auto head = header(k, identity, payload.size(), hash_bytes(payload, 0x2545F4914F6CDD1Dull));
                    {
                        std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
                        out.write(reinterpret_cast<const char *>(head.data()), sizeof head);
                        out.write(reinterpret_cast<const char *>(source.data()),
                                  static_cast<std::streamsize>(source.size() * sizeof(std::uint16_t)));
                        out.write(reinterpret_cast<const char *>(payload.data()),
                                  static_cast<std::streamsize>(payload.size()));
                        ok = static_cast<bool>(out.flush());
                    }
                    if (ok)
                    {
                        std::filesystem::rename(temporary, final_path, error); // replaces an existing file
                        ok = !error;
                    }
                    if (!ok)
                        std::filesystem::remove(temporary, error);
                }
            }
            catch (...)
            {
                ok = false;
            }
            bool prune_now = false;
            {
                const std::lock_guard lock(s.mutex);
                s.pending_bytes -= bytes;
                ++(ok ? s.stats.writes : s.stats.write_failures);
                if (ok)
                    s.written_since_prune += bytes;
                if (s.written_since_prune > folder_budget / 4)
                {
                    s.written_since_prune = 0;
                    prune_now = true;
                }
            }
            if (prune_now)
                prune(root, folder);
        };
        if (!host_parallel::post_background(std::move(task)))
        {
            const std::lock_guard lock(s.mutex);
            s.pending_bytes -= bytes;
            ++s.stats.dropped;
        }
    }
    catch (...)
    {
    }
}
Statistics statistics() noexcept
{
    auto &s = state();
    const std::lock_guard lock(s.mutex);
    return s.stats;
}
} // namespace sgn::host_texture_disk_cache
