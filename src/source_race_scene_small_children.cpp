#include "sgn/source_race_scene_small_children.hpp"
#include "sgn/source_cold_language.hpp"
#include "sgn/source_script_path_formatter.hpp"
#include "sgn/source_script_search.hpp"
#include "sgn/source_script_number.hpp"
#include "sgn/source_decimal_conversion.hpp"
#include "sgn/source_native_crt.hpp"
#include "sgn/retail_frontend_state.hpp"
#include "sgn/race_session.hpp"
#include "sgn/race_session_music.hpp"
#include <bit>
#include <cstring>

namespace sgn::retail
{
namespace
{
using W = std::uint32_t;
using Pointer = SourceBootSearchBoundary;
extern "C" void sgn_source_camera_store_4349a2(const std::uint64_t *, float *);
extern "C" void sgn_source_light_ambient_42d7e0(void *) noexcept;
extern "C" void sgn_scene_ambient_4468b4(const W *, RetailAmbientLight *);
extern "C" void sgn_scene_pow_capture64(SourceScenePowerArguments *, const std::uint64_t *, const std::uint64_t *);
extern "C" void sgn_scene_pow_capture32(SourceScenePowerArguments *, const std::uint64_t *, const float *);
extern "C" void sgn_scene_pow_reciprocal(const SourceSceneExtended80 *, float *);
extern "C" void sgn_scene_pow_raster(const SourceSceneExtended80 *, const float *, W *, std::uint64_t *);
extern "C" void sgn_scene_pow_curve_first(const SourceSceneExtended80 *, const float *, float *, std::uint64_t *);
extern "C" void sgn_scene_pow_curve_second(const SourceSceneExtended80 *, const float *, float *, std::uint64_t *);
static_assert(sizeof(SourceSceneExtended80) == 10 && sizeof(SourceScenePowerArguments) == 24);
static_assert(offsetof(SourceScenePowerArguments, control) == 20 && offsetof(SourceScenePowerArguments, status) == 22);
[[noreturn]] void gap(W at, W child, const char *reason)
{
    throw SourceSceneSmallBoundary(at, child, reason);
}
bool null(Pointer p)
{
    return p.is_null && p.offset == 0;
}
class Configuration final
{
    FrontendState &f;
    RaceSessionPlatform &p;
    RaceSessionState &w;
    SourceBootFilePlatform &files;
    NativeSourceMenuCrt &crt;
    SourceApplicationLiteralRef app;
    SourceScriptNeedle needle(W address)
    {
        std::span<const std::uint8_t> bytes;
        if (address >= 0x484f38 && address < 0x485110)
            bytes = f.source.source_bytes(address, 0x485110u - address);
        else
            bytes = app.vehicle_configuration_literal(address); // same existing478B5C..6F owner
        return {{reinterpret_cast<const char *>(bytes.data()), bytes.size()}, 0, false};
    }
    Pointer search(Pointer input, W address)
    {
        return find_source_script_462890(p, input, needle(address));
    }
    Pointer after_equal(Pointer input, W key, W equals)
    {
        auto at = search(input, key);
        at = search(at, equals);
        ++at.offset;
        return at;
    }
    std::uint8_t byte(Pointer input, W site)
    {
        return read_source_script_byte(p, input, site);
    }
    W integer(Pointer input, W key)
    {
        return parse_source_bank_integer_4631f4(p, after_equal(input, key, 0x478b64));
    }
    std::uint64_t scalar(Pointer input, W key)
    {
        return convert_source_camera_atof_bounded_4631a7(p, crt, after_equal(input, key, 0x478b5c));
    }
    void to_space(Pointer &cursor, std::uint8_t character, W site)
    {
        while (character != 0x20)
        {
            auto next = cursor;
            ++next.offset;
            character = byte(next, site);
            cursor = next;
        }
    }
    void skip_spaces(Pointer &cursor, W site)
    {
        for (;;)
        {
            const auto c = byte(cursor, site);
            if (c != 0x20 && c != 0x0d && c != 0x0a)
                return;
            ++cursor.offset;
        }
    }
    void array(Pointer input, W key, std::span<float> actual)
    {
        auto at = search(search(input, key), 0x478b68);
        to_space(at, byte(at, 0x434a8b), 0x434a90);
        skip_spaces(at, 0x434a98);
        for (auto &destination : actual)
        {
            const auto value = convert_source_camera_atof_bounded_4631a7(p, crt, at);
            const auto character = byte(at, 0x434abd); // before the destination FSTP
            sgn_source_camera_store_4349a2(&value, &destination);
            to_space(at, character, 0x434ac8);
            skip_spaces(at, 0x434ad0);
        }
    }
    void ambience_array(Pointer input)
    {
        auto at = search(search(input, 0x48508c), 0x478b68);
        to_space(at, byte(at, 0x434a8b), 0x434a90);
        skip_spaces(at, 0x434a98);
        for (auto &destination : w.curves.ambience)
        {
            const auto value = convert_source_camera_atof_bounded_4631a7(p, crt, at);
            const auto character = byte(at, 0x434abd);
            float stored;
            sgn_source_camera_store_4349a2(&value, &stored);
            destination = std::bit_cast<W>(stored); // same FLOAT32 store, not a copied table
            to_space(at, character, 0x434ac8);
            skip_spaces(at, 0x434ad0);
        }
    }
    void string_434af0(Pointer input)
    {
        auto at = search(search(input, 0x484fd0), 0x478b6c);
        at.offset += 2;
        auto c = byte(at, 0x434b04);
        W output = 0;
        while (c != 0x0a && c != 0x0d)
        {
            c = byte(at, 0x434b22);
            if (c == 0x20)
                break;
            if (output >= f.source.configuration_string_5da264.size())
                gap(0x434b29, 0x434af0, "434AF0 byte store outside owned128-byte scratch");
            f.source.configuration_string_5da264[output] = c;
            auto next = at;
            ++next.offset;
            c = byte(next, 0x434b2c);
            ++output;
            at = next;
        }
        if (output >= f.source.configuration_string_5da264.size())
            gap(0x434b36, 0x434af0, "434AF0 terminator outside owned128-byte scratch");
        f.source.configuration_string_5da264[output] = 0; // No clear of retained tail.
    }
    AudioEnvironmentRegion &region(W index, W site)
    {
        if (index >= w.audio_environment.regions.size())
            gap(site, 0x446700, "source level box store/read outside actual32-record owner");
        return w.audio_environment.regions[index];
    }
    float &vector_lane(W index, W vector, W lane, W site)
    {
        auto &r = region(index, site);
        VehicleVector *v = vector == 0 ? &r.position : vector == 1 ? &r.scale : vector == 2 ? &r.v : &r.n;
        return lane == 0 ? v->x : lane == 1 ? v->y : v->z;
    }
    Pointer next_vector(Pointer at, std::uint8_t c, W leading_site, W scan_site)
    {
        if (c == 0x20)
        {
            do
            {
                auto next = at;
                ++next.offset;
                c = byte(next, leading_site);
                at = next;
            } while (c == 0x20);
        }
        while (c != 0x20)
        {
            auto next = at;
            ++next.offset;
            c = byte(next, scan_site);
            at = next;
        }
        ++at.offset;
        return at;
    }
    void vector(Pointer input, W key, W index, W which)
    {
        auto at = after_equal(input, key, 0x478b60);
        auto value = convert_source_camera_atof_bounded_4631a7(p, crt, at);
        auto c = byte(at, 0x4349d8); // before first FSTP
        sgn_source_camera_store_4349a2(&value, &vector_lane(index, which, 0, 0x4349da));
        at = next_vector(at, c, 0x4349e1, 0x4349ef);
        value = convert_source_camera_atof_bounded_4631a7(p, crt, at);
        sgn_source_camera_store_4349a2(&value, &vector_lane(index, which, 1, 0x434a01));
        c = byte(at, 0x434a09); // after second FSTP
        at = next_vector(at, c, 0x434a0e, 0x434a1c);
        value = convert_source_camera_atof_bounded_4631a7(p, crt, at);
        sgn_source_camera_store_4349a2(&value, &vector_lane(index, which, 2, 0x434a2c)); // W unchanged
    }
    void audio(Pointer original)
    {
        auto start = search(original, 0x484f38);
        start.offset += 0x10;
        auto at = search(start, 0x484f4c);
        W index = 0;
        while (!null(at))
        {
            const auto type = integer(at, 0x484f54);
            region(index, 0x434a62).type = type;
            vector(at, 0x484f5c, index, 0);
            vector(at, 0x484f68, index, 1);
            vector(at, 0x484f70, index, 2);
            vector(at, 0x484f74, index, 3);
            at.offset += 0x10;
            ++index;
            at = search(at, 0x484f4c);
        }
        region(index, 0x44679a).type = 0xffffffffu;
        // 4460E0 captures the first type before scanning. Count publishes only
        // after the sentinel; no preflight or all-or-nothing table transaction.
        const auto first = region(0, 0x4460e0).type;
        W count = 0;
        if (first != 0xffffffffu)
        {
            do
            {
                const auto next = region(count + 1u, 0x4460f3).type;
                ++count;
                if (next == 0xffffffffu)
                    break;
            } while (true);
        }
        w.audio_environment.count = count;
        if (count != 0)
        {
            const auto result = select_source_audio_environment_460af0(p, first);
            if (result.status != SourceAudioEnvironmentStatus::completed)
                gap(result.blocked_address, 0x460af0,
                    "source level audio requires retained native spatial/property owner");
        }
    }
    AiStuntKnot &knot(W index, W at)
    {
        if (index >= w.stunt_knots.size())
            gap(at, 0x4467b0, "source stunt store outside actual33-record owner");
        return w.stunt_knots[index];
    }
    void stunts(Pointer original)
    {
        auto at = search(original, 0x484f78);
        at.offset += 0x10;
        static_assert(sizeof(w.stunt_knots) == 0x63 * 4);
        // 4467D3 exact99 DWORD clear, after the first search, before second.
        for (auto &k : w.stunt_knots)
        {
            k.progress = 0;
            k.width = 0;
            k.grade = 0;
        }
        at = search(at, 0x484f94);
        W index = 0;
        while (!null(at))
        {
            auto value = scalar(at, 0x484fa4);
            sgn_source_camera_store_4349a2(&value, &knot(index, 0x4349a2).progress);
            value = scalar(at, 0x484fb4);
            sgn_source_camera_store_4349a2(&value, &knot(index, 0x4349a2).width);
            const auto grade = integer(at, 0x484fbc);
            knot(index, 0x434a62).grade = grade;
            at.offset += 0x10;
            ++index;
            at = search(at, 0x484f94);
        }
    }
    void parse(Pointer original)
    {
        (void)search(original, 0x484fc4); // 446850 result deliberately ignored, reads are real.
        string_434af0(original);
        const auto ambient = search(original, 0x484fdc);
        const std::array<W, 4> values{integer(ambient, 0x484fec), integer(ambient, 0x484ff0),
                                      integer(ambient, 0x484ff4), integer(ambient, 0x484ff8)};
        sgn_scene_ambient_4468b4(values.data(), &w.ambient_light);
        sgn_source_light_ambient_42d7e0(&w.ambient_light);
        const auto fog = search(original, 0x484ffc);
        w.fog_color[0] = integer(fog, 0x48500c);
        w.fog_color[1] = integer(fog, 0x485010);
        w.fog_color[2] = integer(fog, 0x485014);
        array(fog, 0x485018, w.curves.fog);
        array(fog, 0x485028, w.curves.restitution);
        array(fog, 0x485040, w.curves.far_clip);
        array(fog, 0x48505c, w.curves.respawn);
        array(fog, 0x485078, w.curves.lights);
        ambience_array(fog);
        array(fog, 0x4850a0, w.curves.width_left);
        array(fog, 0x4850bc, w.curves.width_right);
        audio(original);
        stunts(original);
    }

  public:
    Configuration(FrontendState &frontend, RaceSessionPlatform &platform, SourceBootFilePlatform &file,
                  NativeSourceMenuCrt &runtime)
        : f(frontend), p(platform), w(platform.state()), files(file), crt(runtime),
          app(platform.source_application_literals())
    {
    }
    void load()
    {
        std::array<std::uint8_t, 128> local;
        SourceScriptPathCall first;
        format_source_level_configuration_4469f0(f, false, local, first);
        auto path = std::string_view(reinterpret_cast<const char *>(local.data()), first.output_count);
        W size = source_boot_file_size_44b3b0(w.boot_config, files, path);
        if (size == 0)
        {
            SourceScriptPathCall second;
            format_source_level_configuration_4469f0(f, true, local, second);
            path = std::string_view(reinterpret_cast<const char *>(local.data()), second.output_count);
            size = source_boot_file_size_44b3b0(w.boot_config, files, path);
        }
        auto &bank = p.source_allocations();
        const auto allocation = bank.allocate_42e8d0(size + 32u, 0);
        std::size_t copied = 0;
        (void)source_boot_file_read_bank_44b260(w.boot_config, files, bank, path, size, allocation, copied);
        parse({allocation, 0, !static_cast<bool>(allocation)});
        (void)bank.release_42e980(allocation); // 446A64 only after successful446840; never finally.
    }
};
void colors(FrontendState &f, NativeSourceMenuCrt &crt, SourceSceneSmallRequired &required)
{
    auto &t = f.world().color_tables;
    SourceScenePowerArguments args;
    constexpr std::uint64_t one = 0x3ff0000000000000ull, two = 0x4000000000000000ull;
    constexpr std::uint64_t n256 = 0x4070000000000000ull, n128 = 0x4060000000000000ull;
    std::uint64_t outer_iteration = one; // 422427/2E BEFORE422435 child
    std::uint64_t iteration = one;
    sgn_scene_pow_capture64(&args, &n256, &two);
    auto value = required.power_462bb0(f, crt, 0x4223a1, args);
    float reciprocal;
    sgn_scene_pow_reciprocal(&value, &reciprocal);
    for (auto &target : t.raster_gamma)
    {
        sgn_scene_pow_capture64(&args, &iteration, &two);
        value = required.power_462bb0(f, crt, 0x4223bd, args);
        sgn_scene_pow_raster(&value, &reciprocal, &target, &iteration);
    }
    sgn_scene_pow_capture32(&args, &n128, &t.exponent_4754c4);
    value = required.power_462bb0(f, crt, 0x422446, args);
    sgn_scene_pow_reciprocal(&value, &reciprocal);
    for (auto &target : t.curve_5717d4)
    {
        sgn_scene_pow_capture32(&args, &outer_iteration, &t.exponent_4754c4);
        value = required.power_462bb0(f, crt, 0x422462, args);
        sgn_scene_pow_curve_first(&value, &reciprocal, &target, &outer_iteration);
    }
    // 422484/8A load operands BEFORE422490/97 resets the source counter.
    sgn_scene_pow_capture32(&args, &n128, &t.exponent_4754c8);
    outer_iteration = one;
    value = required.power_462bb0(f, crt, 0x42249e, args);
    sgn_scene_pow_reciprocal(&value, &reciprocal);
    for (auto &target : t.curve_5715d4)
    {
        sgn_scene_pow_capture32(&args, &outer_iteration, &t.exponent_4754c8);
        value = required.power_462bb0(f, crt, 0x4224ba, args);
        sgn_scene_pow_curve_second(&value, &reciprocal, &target, &outer_iteration);
    }
}
struct UnavailablePower final : SourceSceneSmallRequired
{
    SourceSceneExtended80 power_462bb0(FrontendState &, NativeSourceMenuCrt &, W callsite,
                                       const SourceScenePowerArguments &) override
    {
        gap(callsite, 0x462bb0,
            "source color tables require actual462BB0 CRT; native integer/finite pow proxy prohibited");
    }
};
} // namespace
bool perform_source_race_scene_small_child(SceneOperation operation, FrontendState &f, SourceBootFilePlatform &files,
                                           NativeSourceMenuCrt &crt, SourceSceneSmallRequired &required)
{
    switch (operation)
    {
    case SceneOperation::prepare_race_state:
    case SceneOperation::configure_level:
    case SceneOperation::initialize_vehicle_numeric:
    case SceneOperation::initialize_color_tables:
    case SceneOperation::reset_recordings:
    case SceneOperation::leave_menu_for_race:
        break;
    default:
        return false;
    }
    const auto p = f.shared_platform();
    if (!p || !p->source_construction_only_entry() || &p->state() != &f.world())
        throw FormatError("source scene children require same Core frontend/platform");
    p->require_owner_thread();
    switch (operation)
    {
    case SceneOperation::prepare_race_state:
        refresh_session_source_language_445f80(*p, files);
        return true;
    case SceneOperation::configure_level:
        crt.require_platform(*p);
        Configuration(f, *p, files, crt).load();
        return true;
    case SceneOperation::initialize_vehicle_numeric:
        return true; // PE40E050 C3, actual complete RET.
    case SceneOperation::initialize_color_tables:
        crt.require_platform(*p);
        colors(f, crt, required);
        return true;
    case SceneOperation::reset_recordings:
        reset_replays(f.world().replays, f.world().vehicles.race);
        return true;
    case SceneOperation::leave_menu_for_race:
        f.source.menu_state = 1;         // 43D1B0
        f.source.menu_result_5fd24c = 0; // 43D420 XOR EAX,EAX/RET ->43D1BF
        return true;
    default:
        throw std::logic_error("unreachable source small-child dispatch");
    }
}
bool perform_source_race_scene_small_child(SceneOperation op, FrontendState &f, SourceBootFilePlatform &files,
                                           NativeSourceMenuCrt &crt)
{
    UnavailablePower required;
    return perform_source_race_scene_small_child(op, f, files, crt, required);
}
} // namespace sgn::retail
