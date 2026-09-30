#include "sgn/source_camera_startup.hpp"
#include "sgn/source_cold_camera_initialization.hpp"
#include "sgn/source_script_path_formatter.hpp"
#include "sgn/source_script_search.hpp"
#include "sgn/source_script_number.hpp"
#include "sgn/source_decimal_conversion.hpp"
#include "sgn/source_native_crt.hpp"
#include "sgn/race_session.hpp"
#include <bit>
#include <cstring>
#include <type_traits>
#include <limits>

extern "C" void sgn_source_camera_store_4349a2(const std::uint64_t *, float *);
namespace sgn::retail
{
namespace
{
using Word = std::uint32_t;
static_assert(std::is_trivially_copyable_v<AuthoredCamera>);
static_assert(sizeof(AuthoredCamera) == 64 && offsetof(AuthoredCamera, position) == 0 &&
              offsetof(AuthoredCamera, target) == 16 && offsetof(AuthoredCamera, up) == 32 &&
              offsetof(AuthoredCamera, lens) == 48 && offsetof(AuthoredCamera, tracking) == 52 &&
              offsetof(AuthoredCamera, twist) == 56 && offsetof(AuthoredCamera, opaque) == 60);
auto platform_for(const std::weak_ptr<RaceSessionPlatform> &borrow, Word site)
{
    auto platform = borrow.lock();
    if (!platform)
        throw SourceAllocationError(site, "camera startup platform expired");
    platform->require_owner_thread();
    if (!platform->source_construction_only_entry())
        throw SourceAllocationError(site, "camera startup requires the actual source platform");
    return platform;
}
Word bank_word(RaceSessionPlatform &platform, SourcePmdLocation where, std::uint64_t relative, Word site)
{
    try
    {
        const auto bytes = platform.source_allocations().bytes(where.allocation);
        if (relative > std::uint64_t(std::numeric_limits<Word>::max()) - where.offset)
            throw SourceAllocationError(site, "embedded camera normalized offset exceeds source32-bit domain");
        const auto at = std::uint64_t(where.offset) + relative;
        if (at > bytes.size() || bytes.size() - at < 4)
            throw SourceAllocationError(site, "embedded camera source DWORD outside live PMD allocation");
        Word value;
        std::memcpy(&value, bytes.data() + at, 4);
        if constexpr (std::endian::native == std::endian::big)
            value = std::byteswap(value);
        return value;
    }
    catch (const SourceAllocationError &error)
    {
        throw SourceAllocationError(site, error.what());
    }
}
AuthoredCamera &record(CameraBank &bank, Word index, Word site)
{
    if (index >= bank.records.size())
        throw SourceAllocationError(site, "reached camera record outside actual32-slot region");
    return bank.records[index];
}
void store(CameraBank &bank, Word index, Word offset, Word value, Word site)
{
    auto &output = record(bank, index, site);
    // All reached destinations are four-byte float/integer fields. Copying
    // representation avoids aliasing UB and retains each source MOV's bits.
    static_assert(std::endian::native == std::endian::little);
    std::memcpy(reinterpret_cast<std::uint8_t *>(&output) + offset, &value, 4);
}
void fallback_401e50(RaceSessionPlatform &platform)
{
    const auto &track = platform.source_scene_resources().models().source_loading_state().track;
    auto remaining = track.opaque58_count; // 401E50 captures6244A4 before pointer
    if (remaining == 0)
        return;                         // 401E5B: no bank/count/cycle writes and no pointer read
    const auto source = track.opaque58; // 401E67: actual624478, embedded96-byte records
    auto &world = platform.state();
    Word index = 0;
    std::uint64_t relative = 0;
    constexpr Word reads[3][4] = {{0x401e7d, 0x401e81, 0x401e87, 0x401e8d},
                                  {0x401e97, 0x401e9b, 0x401ea1, 0x401ea7},
                                  {0x401eb6, 0x401ebd, 0x401ec3, 0x401ec9}};
    constexpr Word writes[3][4] = {{0x401e7f, 0x401e84, 0x401e8a, 0x401e90},
                                   {0x401e99, 0x401e9e, 0x401ea4, 0x401eaa},
                                   {0x401ebb, 0x401ec0, 0x401ec6, 0x401ecc}};
    constexpr Word starts[3] = {0x10, 0x30, 0x40};
    do
    {
        for (Word vector = 0; vector < 3; ++vector)
            for (Word lane = 0; lane < 4; ++lane)
            {
                const auto value =
                    bank_word(platform, source, relative + starts[vector] + lane * 4, reads[vector][lane]);
                store(world.camera_bank, index, vector * 16 + lane * 4, value, writes[vector][lane]);
            }
        const auto lens = bank_word(platform, source, relative + 4, 0x401ecf);
        store(world.camera_bank, index, 48, lens, 0x401ed5);
        store(world.camera_bank, index, 52, 0, 0x401eda);
        store(world.camera_bank, index, 56, 0, 0x401edd); // opaque+60 deliberately retained
        --remaining;
        ++index;
        relative += 96u;
    } while (remaining != 0);
    world.camera_bank.count = track.opaque58_count; // 401EE3 reread, then401EEB store
    world.camera_session.authored_cycle_index = 0;  // 401EF1
}
SourceBootSearchBoundary search(RaceSessionPlatform &platform, SourceBootSearchBoundary input, Word literal)
{
    const auto app = platform.source_application_literals();
    const auto bytes = app.camera_startup_literal(literal);
    return find_source_script_462890(platform, input,
                                     {{reinterpret_cast<const char *>(bytes.data()), bytes.size()}, 0, false});
}
SourceBootSearchBoundary after_equal(RaceSessionPlatform &platform, SourceBootSearchBoundary input, Word key,
                                     Word equals)
{
    auto found = search(platform, input, key); // 434985 or434A45
    found = search(platform, found, equals);   // 434990 or434A50, even after a null result
    ++found.offset;                            // 434995/434A55; unresolved NULL+1 faults on the reached read
    return found;
}
Word integer_434a40(RaceSessionPlatform &platform, SourceBootSearchBoundary input, Word key)
{
    return parse_source_bank_integer_4631f4(platform, after_equal(platform, input, key, 0x478b64));
}
void real_434980(RaceSessionPlatform &platform, NativeSourceMenuCrt &crt, SourceBootSearchBoundary input, Word key,
                 CameraBank &bank, Word index, Word destination)
{
    const auto first = after_equal(platform, input, key, 0x478b5c);
    // Same bank payload, no string copy/pre-clear. The existing bounded CRT
    // decoder preserves495DA8/6365E0 result writes before this FSTP destination.
    const auto returned = convert_source_camera_atof_bounded_4631a7(platform, crt, first);
    auto &output = record(bank, index, 0x4349a2); // after all CRT writes, not on argument evaluation
    float *field = nullptr;
    switch (destination)
    {
    case 0:
        field = &output.position.x;
        break;
    case 4:
        field = &output.position.y;
        break;
    case 8:
        field = &output.position.z;
        break;
    case 16:
        field = &output.target.x;
        break;
    case 20:
        field = &output.target.y;
        break;
    case 24:
        field = &output.target.z;
        break;
    case 32:
        field = &output.up.x;
        break;
    case 36:
        field = &output.up.y;
        break;
    case 40:
        field = &output.up.z;
        break;
    case 48:
        field = &output.lens;
        break;
    case 56:
        field = &output.twist;
        break;
    default:
        throw std::logic_error("unbound434980 camera destination");
    }
    sgn_source_camera_store_4349a2(&returned, field);
}
} // namespace
void load_source_camera_resources_401f00(const std::weak_ptr<RaceSessionPlatform> &borrow,
                                         SourceBootFilePlatform &files, NativeSourceMenuCrt &crt)
{
    const auto platform = platform_for(borrow, 0x401f00);
    auto &world = platform->state();
    if (world.scene_mode != 1)
    {
        world.camera_bank.count = 0;        // 401F21
        world.camera_bank.records.fill({}); // 401F26 exact512 DWORDs, cycle retained
        return;
    }
    const auto name =
        platform->source_scene_resources().models().source_loading_state().requests.track_name(); // 401F2E
    SourceScriptPathCall path;
    const auto app = platform->source_application_literals();
    format_camera_path_401f45(world.boot_config, app, name, path);
    const auto size = source_boot_file_size_44b3b0(world.boot_config, files, path.completed_path()); // 401F53
    if (size == 0)
    {
        fallback_401e50(*platform);
        return;
    }
    auto &allocations = platform->source_allocations();
    const auto allocation = allocations.allocate_42e8d0(size + 32u, 0); // 401F6F, DWORD wrap preserved
    std::size_t copied = 0;                                             // host observation, source ignores EAX at401F87
    (void)source_boot_file_read_bank_44b260(world.boot_config, files, allocations, path.completed_path(), size,
                                            allocation, copied); // 401F82
    SourceBootSearchBoundary cursor{allocation, 0, !static_cast<bool>(allocation)};
    const auto header = search(*platform, cursor, 0x472af0);
    world.camera_bank.count = integer_434a40(*platform, header, 0x472afc); // 434A62 real499F90
    Word index = 0;
    if (std::bit_cast<std::int32_t>(world.camera_bank.count) > 0)
    {
        for (;;)
        {
            auto section = search(*platform, cursor, 0x472b0c); // 401FC5 before defaults
            store(world.camera_bank, index, 0, 0, 0x401fd7);
            store(world.camera_bank, index, 4, 0, 0x401fdd);
            store(world.camera_bank, index, 8, 0, 0x401fe6);
            real_434980(*platform, crt, section, 0x472b1c, world.camera_bank, index, 0);
            real_434980(*platform, crt, section, 0x472b20, world.camera_bank, index, 4);
            real_434980(*platform, crt, section, 0x472b24, world.camera_bank, index, 8);
            section = search(*platform, cursor, 0x472b28);
            store(world.camera_bank, index, 16, 0, 0x402027);
            store(world.camera_bank, index, 20, 0, 0x40202d);
            store(world.camera_bank, index, 24, 0, 0x402037);
            real_434980(*platform, crt, section, 0x472b34, world.camera_bank, index, 16);
            real_434980(*platform, crt, section, 0x472b38, world.camera_bank, index, 20);
            real_434980(*platform, crt, section, 0x472b3c, world.camera_bank, index, 24);
            section = search(*platform, cursor, 0x472b40);
            store(world.camera_bank, index, 32, 0, 0x40207b);
            store(world.camera_bank, index, 36, 0, 0x402081);
            store(world.camera_bank, index, 40, 0, 0x40208b);
            real_434980(*platform, crt, section, 0x472b48, world.camera_bank, index, 32);
            real_434980(*platform, crt, section, 0x472b4c, world.camera_bank, index, 36);
            real_434980(*platform, crt, section, 0x472b50, world.camera_bank, index, 40);
            real_434980(*platform, crt, section, 0x472b54, world.camera_bank, index, 48);
            const auto tracking = integer_434a40(*platform, section, 0x472b5c);
            store(world.camera_bank, index, 52, tracking, 0x434a62);
            real_434980(*platform, crt, section, 0x472b68, world.camera_bank, index, 56);
            const auto twist = search(*platform, cursor, 0x472b70); // 4020ED from old EBX
            const auto count = world.camera_bank.count;             // 4020F2 before advancing cursor/index
            cursor = twist;
            cursor.offset += 5u; // 4020F8/FB, sourceNULL+5 also retained until read
            ++index;
            if (std::bit_cast<std::int32_t>(index) >= std::bit_cast<std::int32_t>(count))
                break;
        }
    }
    (void)allocations.release_42e980(allocation);  // 402117 original allocation, only on normal path
    world.camera_session.authored_cycle_index = 0; // 40211C, after release
    // Failures deliberately leave allocations and every reached bank/CRT write.
}
void initialize_source_camera_402420(const std::weak_ptr<RaceSessionPlatform> &platform, SourceBootFilePlatform &files,
                                     NativeSourceMenuCrt &crt)
{
    load_source_camera_resources_401f00(platform, files, crt); // 402422
    initialize_source_camera_controllers_402427(platform);
}
} // namespace sgn::retail
