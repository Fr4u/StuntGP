#include "sgn/assets.hpp"
#include "sgn/clock.hpp"
#include "sgn/renderer.hpp"
#include "sgn/pmd_scene.hpp"
#include "sgn/telemetry.hpp"
#include "sgn/race_application.hpp"
#include "sgn/front_recipe_bundle.hpp"
#include "sgn/source_application_literals.hpp"
#include "sgn/source_application_core.hpp"
#include "sgn/source_application_surfaces_late_bound.hpp"
#include "sgn/source_application_menu_services.hpp"
#include "sgn/source_menu_navigation_binding.hpp"
#include "sgn/source_application_menu_composition.hpp"
#include "sgn/source_application_messages_win32.hpp"
#include "sgn/source_host_pacing.hpp"
#include "sgn/host_presentation_presenter.hpp"
#include "sgn/host_frame_trace.hpp"
#include "sgn/source_application_draw_start_win32.hpp"
#include "sgn/source_frontend_initialization.hpp"
#include "sgn/source_save_native_persistence.hpp"
#include "sgn/source_native_crt.hpp"
#include "sgn/host_wide_layout.hpp"
#include "sgn/host_graphics_settings.hpp"
#include "sgn/host_graphics_page.hpp"
#include "sgn/host_display_mode.hpp"
#include "sgn/source_menu_presentation_backend.hpp"
#include <cstdlib>
#include <exception>
#include <charconv>
#include <bit>
#include <windowsx.h>
#include <shellapi.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace
{
using namespace sgn;
struct Options
{
    RenderOptions renderer;
    std::filesystem::path root, report, capture, camera_fixture, displayed_recipe, displayed_world_join;
    std::filesystem::path displayed_front_bundle, camera_recovery_witness;
    std::optional<diagnostics::WorldJoinSelection> world_join_range;
    std::string model = "car16";
    std::string track_set;
    unsigned width = 1440, height = 900, frames = 0;
    std::string level = "TRACK1";
    RaceApplicationMode play_mode{RaceApplicationMode::normal_race};
    unsigned drive_ticks = 1200;
    std::optional<unsigned> pause_at_tick;
    unsigned challenge_ticks{};
    bool interpolate{false}, level_explicit{}, model_explicit{}; // R19: high-refresh interpolation is opt-in
    // NEW HOST (R23) session overrides of the graphics settings file (source menu path).
    std::optional<bool> interpolation_flag;
    bool aspect_4x3_session{}, vsync_off_session{};
    bool race{true}, drive_smoke{}, muted{}, race_explicit{}, source_menu{};
    bool exercise{}, smoke{}, scene_check{}, scene_mirror{}, device_loss_check{}, debug_error_check{};
    bool zoom_out_check{}, reload_loss_check{}, focus_check{}, rig_check{};
};
unsigned unsigned_argument(const std::wstring &raw)
{
    std::string value;
    for (wchar_t c : raw)
    {
        if (c < L'0' || c > L'9')
            throw std::invalid_argument("expected an unsigned decimal argument");
        value += static_cast<char>(c);
    }
    unsigned result{};
    const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), result);
    if (error != std::errc{} || end != value.data() + value.size())
        throw std::invalid_argument("unsigned argument outside range");
    return result;
}
std::string utf8(const std::wstring &s)
{
    int n = WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<std::size_t>(n), 0);
    WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), n, nullptr, nullptr);
    return out;
}
std::string json_string(const std::string &s)
{
    std::string out = "\"";
    for (unsigned char c : s)
    {
        if (c == '"' || c == '\\')
        {
            out += '\\';
            out += static_cast<char>(c);
        }
        else if (c >= 32)
            out += static_cast<char>(c);
        else
            out += ' ';
    }
    return out + '"';
}
Options arguments()
{
    Options o;
    std::vector<wchar_t> module(32768);
    DWORD n = GetModuleFileNameW(nullptr, module.data(), static_cast<DWORD>(module.size()));
    if (!n || n == module.size())
        throw std::runtime_error("cannot resolve executable path");
    auto path = std::filesystem::path(std::wstring(module.data(), n)).parent_path();
    for (unsigned i = 0; i < 5; ++i)
    {
        if (std::filesystem::exists(path / "data/retail/wads"))
        {
            o.root = path;
            break;
        }
        path = path.parent_path();
    }
    o.aspect_4x3_session =
        sgn::host_wide::configure({}); // NEW R20: SGN_ASPECT=4x3 selects the original 4:3 presentation
    int argc{};
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv)
        throw std::runtime_error("command line unavailable");
    try
    {
        for (int i = 1; i < argc; ++i)
        {
            std::wstring key = argv[i];
            auto value = [&]() -> std::wstring {
                if (++i >= argc)
                    throw std::runtime_error("missing command argument");
                return argv[i];
            };
            if (key == L"--root")
                o.root = value();
            else if (key == L"--model")
            {
                o.model = utf8(value());
                o.model_explicit = true;
                if (!o.race_explicit)
                    o.race = false;
            }
            else if (key == L"--source-menu")
                o.source_menu = true;
            // R18 unified game: the former direct preview race is retired. These
            // legacy launch flags now enter the one game (startup and main menu).
            else if (key == L"--race" || key == L"--normal-race" || key == L"--time-trial" || key == L"--challenge")
                o.source_menu = true;
            else if (key == L"--challenge-ticks")
                o.challenge_ticks = unsigned_argument(value());
            else if (key == L"--no-interpolation")
            {
                o.interpolate = false;
                o.interpolation_flag = false;
            }
            // NEW R20 host presentation: original 4:3 canvas with bars (host_wide_layout.hpp).
            else if (key == L"--aspect-4x3")
            {
                sgn::host_wide::set_original_4x3(true);
                o.aspect_4x3_session = true;
            }
            else if (key == L"--interpolation")
            {
                o.interpolate = true;
                o.interpolation_flag = true;
            }
            // NEW HOST (R23): session-only vsync off (not saved to the graphics settings file).
            else if (key == L"--no-vsync")
            {
                o.renderer.vsync = false;
                o.vsync_off_session = true;
            }
            else if (key == L"--inspect")
            {
                o.race = false;
                o.race_explicit = true;
            }
            else if (key == L"--drive-smoke")
            {
                o.race = true;
                o.race_explicit = true;
                o.drive_smoke = true;
                o.smoke = true;
            }
            else if (key == L"--ticks")
                o.drive_ticks = unsigned_argument(value());
            else if (key == L"--pause-at-tick")
                o.pause_at_tick = unsigned_argument(value());
            else if (key == L"--level")
            {
                o.level = utf8(value());
                o.level_explicit = true;
            }
            else if (key == L"--muted")
                o.muted = true;
            else if (key == L"--track-set")
                o.track_set = utf8(value());
            else if (key == L"--report")
                o.report = value();
            else if (key == L"--capture")
                o.capture = value();
            else if (key == L"--displayed-front-bundle" || key == L"--camera-recovery-witness")
            {
                auto &output =
                    key == L"--displayed-front-bundle" ? o.displayed_front_bundle : o.camera_recovery_witness;
                if (!output.empty())
                    throw std::invalid_argument("duplicate front/camera diagnostic output");
                output = value();
                if (output.empty())
                    throw std::invalid_argument("empty front/camera diagnostic output");
            }
            else if (key == L"--displayed-recipe")
            {
                o.displayed_recipe = value();
                if (o.displayed_recipe.empty())
                    throw std::invalid_argument("displayed recipe output path is empty");
            }
            else if (key == L"--displayed-world-join")
            {
                if (!o.displayed_world_join.empty())
                    throw std::invalid_argument("duplicate displayed world join output");
                o.displayed_world_join = value();
                if (o.displayed_world_join.empty())
                    throw std::invalid_argument("displayed world join output path is empty");
            }
            else if (key == L"--world-join-range")
            {
                if (o.world_join_range)
                    throw std::invalid_argument("duplicate world join range");
                const auto first = value();
                const auto count = value();
                o.world_join_range = diagnostics::parse_world_join_selection(first, count);
            }
            else if (key == L"--camera-fixture")
                o.camera_fixture = std::filesystem::absolute(value());
            else if (key == L"--width")
                o.width = unsigned_argument(value());
            else if (key == L"--height")
                o.height = unsigned_argument(value());
            else if (key == L"--frames")
                o.frames = unsigned_argument(value());
            else if (key == L"--debug")
                o.renderer.debug = true;
            else if (key == L"--warp")
                o.renderer.warp = true;
            else if (key == L"--uncapped")
            {
                o.renderer.vsync = false;
                o.vsync_off_session = true;
            }
            else if (key == L"--exercise-window")
                o.exercise = true;
            else if (key == L"--smoke")
                o.smoke = true;
            else if (key == L"--scene-check")
                o.scene_check = true;
            else if (key == L"--rig-check")
                o.rig_check = true;
            else if (key == L"--scene-mirror")
            {
                o.scene_check = true;
                o.scene_mirror = true;
            }
            else if (key == L"--device-loss-check")
                o.device_loss_check = true;
            else if (key == L"--debug-error-check")
                o.debug_error_check = true;
            else if (key == L"--zoom-out-check")
                o.zoom_out_check = true;
            else if (key == L"--reload-loss-check")
            {
                o.reload_loss_check = true;
                o.device_loss_check = true;
            }
            else if (key == L"--focus-check")
                o.focus_check = true;
            else
                throw std::runtime_error("unknown option: " + utf8(key));
        }
    }
    catch (...)
    {
        LocalFree(argv);
        throw;
    }
    LocalFree(argv);
    if (o.root.empty())
        throw std::runtime_error("Retail assets missing. Run IMPORT.ps1 or pass --root.");
    if (o.width < 320 || o.height < 240 || o.width > 16384 || o.height > 16384)
        throw std::runtime_error("window size outside 320x240..16384x16384");
    if (!o.race_explicit && (o.rig_check || o.scene_check || !o.camera_fixture.empty()))
        o.race = false;
    // R18 unified game: a plain launch (no mode, model or diagnostic control)
    // enters the game itself, i.e. the recovered startup and main menu.
    if (!o.source_menu && !o.race_explicit && !o.model_explicit && !o.smoke && !o.exercise && !o.rig_check &&
        !o.scene_check && !o.focus_check && !o.zoom_out_check && !o.device_loss_check && !o.debug_error_check &&
        !o.muted && !o.frames && o.capture.empty() && o.camera_fixture.empty() && o.track_set.empty())
        o.source_menu = true;
    if (o.source_menu)
    {
        if (o.race_explicit || o.smoke || o.exercise || o.rig_check || o.scene_check || o.focus_check ||
            o.zoom_out_check || o.device_loss_check || o.debug_error_check || o.muted || o.frames ||
            !o.capture.empty() || !o.camera_fixture.empty() || !o.track_set.empty())
            throw std::invalid_argument("--source-menu cannot share race/inspection or diagnostic controls");
        o.race = false;
        o.root = std::filesystem::absolute(o.root).lexically_normal();
    }
    o.renderer.race_interface = o.race;
    if (o.play_mode == RaceApplicationMode::challenge && !o.level_explicit)
        o.level = "CHALL1";
    if (o.drive_ticks == 0 || o.drive_ticks > 600000)
        throw std::invalid_argument("drive ticks outside 1..600000");
    if (o.pause_at_tick && (!o.drive_smoke || *o.pause_at_tick < 2 || *o.pause_at_tick >= o.drive_ticks ||
                            o.drive_ticks - *o.pause_at_tick < 4 || o.frames != 0))
        // clang-format off: keep the message one string literal (BreakStringLiterals would split it)
        throw std::invalid_argument("pause diagnostic requires --drive-smoke, pause tick >= 2, at least four following ticks and no --frames cap");
    // clang-format on
    if (o.smoke && !o.frames && !o.drive_smoke)
        o.frames = 120;
    if (o.rig_check && o.scene_check)
        throw std::invalid_argument("rig and two-mesh diagnostics are separate scenes");
    if (o.device_loss_check && (!o.smoke || !o.renderer.warp || o.frames <= 20))
        throw std::invalid_argument("device loss diagnostic requires --smoke --warp --frames greater than 20");
    if (o.debug_error_check && (!o.smoke || !o.renderer.warp || !o.renderer.debug || o.frames <= 20))
        throw std::invalid_argument("debug error injection requires --smoke --warp --debug --frames greater than 20");
    if ((o.zoom_out_check || o.focus_check) && (!o.smoke || o.frames <= 20))
        throw std::invalid_argument("inspection diagnostics require --smoke --frames greater than 20");
    if (o.report.empty())
        o.report = o.root / "reports/last-runtime.json";
    o.report = std::filesystem::absolute(o.report);
    if (!o.capture.empty())
        o.capture = std::filesystem::absolute(o.capture);
    if (!o.displayed_recipe.empty())
    {
        if (!o.race)
            throw std::invalid_argument("displayed recipe export requires a race scene");
        o.displayed_recipe = std::filesystem::absolute(o.displayed_recipe).lexically_normal();
    }
    if (o.displayed_front_bundle.empty() && (!o.displayed_world_join.empty() || o.world_join_range))
    {
        if (!o.race || o.displayed_recipe.empty() || o.displayed_world_join.empty() || !o.world_join_range)
            // clang-format off: keep the message one string literal (BreakStringLiterals would split it)
            throw std::invalid_argument("world join requires a race, --displayed-recipe, --displayed-world-join and --world-join-range FIRST COUNT");
        // clang-format on
        o.displayed_world_join = std::filesystem::absolute(o.displayed_world_join).lexically_normal();
        const auto same = [](const std::filesystem::path &a, const std::filesystem::path &b) {
            if (a.empty() || b.empty())
                return false;
            const auto x = a.lexically_normal().native(), y = b.lexically_normal().native();
            return CompareStringOrdinal(x.c_str(), -1, y.c_str(), -1, TRUE) == CSTR_EQUAL;
        };
        if (same(o.displayed_world_join, o.displayed_recipe) || same(o.displayed_world_join, o.report) ||
            same(o.displayed_world_join, o.capture) || same(o.displayed_recipe, o.report) ||
            same(o.displayed_recipe, o.capture))
            throw std::invalid_argument("world join and recipe outputs must be distinct from report and capture paths");
    }
    if (!o.displayed_front_bundle.empty())
    {
        if (!o.race || !o.displayed_recipe.empty() || !o.displayed_world_join.empty())
            throw std::invalid_argument("front bundle requires a race and cannot share legacy export options");
        o.displayed_front_bundle = std::filesystem::absolute(o.displayed_front_bundle).lexically_normal();
        const std::array other{o.report, o.capture, o.camera_recovery_witness};
        diagnostics::validate_diagnostic_bundle_path(o.displayed_front_bundle, other);
    }
    if (!o.camera_recovery_witness.empty())
    {
#if !defined(SGN_CAMERA_RECOVERY_RECEIVING) || SGN_CAMERA_RECOVERY_RECEIVING != 1
        throw std::invalid_argument("camera recovery witness requires the explicit receiving build");
#else
        if (!o.race)
            throw std::invalid_argument("camera recovery witness requires a race");
        o.camera_recovery_witness = std::filesystem::absolute(o.camera_recovery_witness).lexically_normal();
        const std::array other{o.report, o.capture, o.displayed_recipe, o.displayed_world_join,
                               o.displayed_front_bundle};
        diagnostics::validate_diagnostic_bundle_path(o.camera_recovery_witness, other);
#endif
    }
    return o;
}
class App
{
  public:
    Options options;
    // NEW application lifetime: exact PE literal bytes survive race/session
    // and renderer recreation. Destroyed after all their weak consumers.
    retail::SourceApplicationLiteralData source_application_literals;
    HWND window{};
    std::unique_ptr<Renderer> renderer;
    std::unique_ptr<RaceApplication> race_application;
    std::array<bool, 256> held_keys{};
    bool race_reset_requested{};
    bool race_restart_requested{};
    unsigned race_sessions_started{};
    AssetStore assets;
    Model model;
    std::vector<std::string> names;
    unsigned selection{}, lod{};
    bool running = true, minimized{}, preview_active = true, dragging{};
    bool reload_requested{}, reload_reset{}, rebase_time{};
    bool focus_resume_pending{};
    std::uint64_t focus_before_ticks{}, focus_resume_ticks{};
    InspectionFrustum last_frustum{};
    // NEW HOST (R24): windowed / borderless / exclusive owner of this window
    // (replaces the former F11 borderless toggle and its saved rectangle).
    host_display::Controller display_modes;
    POINT mouse{};
    float yaw = 0.65f, pitch = 0.28f, distance = 10, radius = 1;
    Vec3 center{};
    TickClock clock;
    std::uint64_t tick_checksum{};
    void suspend_clock()
    {
        clock.suspend();
        if (race_application)
            race_application->invalidate_presentation();
    }
    SampleWindow<> frame_ms, interval_ms, wait_ms;
    std::vector<std::string> events;
    std::string display_title, display_detail;
    unsigned detail_width{}, detail_height{}, detail_samples{};
    bool detail_vsync{};
    std::array<SceneInstance, 2> scene_instances{};
    std::vector<SceneInstance::Part> model_parts;
    SceneInstance rig_instance{};
    std::optional<View> fixed_camera;
    unsigned device_recreations{};
    std::uint64_t previous_presents{}, previous_debug_errors{};
    std::vector<GpuDiagnostic> previous_diagnostics;
    std::string failure;
    unsigned pending_width{}, pending_height{};
    explicit App(Options o) : options(std::move(o)), assets(options.root / "data/retail")
    {
        if (options.source_menu)
            return; // Core loads the actual menu assets through source startup.
        // PMD 1.82 files are retained as evidence, never routed through the 1.83 decoder.
        for (auto &name : assets.models())
        {
            auto b = read_file(options.root / "data/retail/meshdata" / (name + ".PMD"));
            if (b.size() >= 9 && std::string(reinterpret_cast<const char *>(b.data()), 9) == "PMD V1.83")
            {
                if (options.track_set.empty() && name.starts_with("track") && name.ends_with("m"))
                    continue;
                names.push_back(name);
            }
        }
        auto it = std::find(names.begin(), names.end(), options.model);
        if (it == names.end())
            throw std::runtime_error("unsupported model: " + options.model);
        selection = static_cast<unsigned>(it - names.begin());
        if (!options.camera_fixture.empty())
        {
            const auto bytes = read_file(options.camera_fixture);
            if (bytes.size() != 96 || std::string(bytes.begin(), bytes.begin() + 8) != "SGNVW001")
                throw FormatError("invalid source-camera diagnostic fixture");
            const auto real = [&](std::size_t index) {
                auto at = 8 + index * 4;
                auto word = std::uint32_t(bytes[at]) | (std::uint32_t(bytes[at + 1]) << 8) |
                            (std::uint32_t(bytes[at + 2]) << 16) | (std::uint32_t(bytes[at + 3]) << 24);
                auto value = std::bit_cast<float>(word);
                if (!std::isfinite(value))
                    throw FormatError("nonfinite camera fixture");
                return value;
            };
            fixed_camera.emplace();
            fixed_camera->world_to_view.emplace();
            for (unsigned i = 0; i < 16; ++i)
                (*fixed_camera->world_to_view)[i] = real(i);
            fixed_camera->retail_viewport =
                View::RetailViewport{{real(16), real(17), real(18), real(19), real(20)}, real(21), true};
        }
    }
    // NEW HOST staged association. These references exist before HWND and
    // remain the sole source words while the real input/menu owners attach.
    struct MenuWindowBinding
    {
        retail::SourceWindowMessageWords words;
        retail::SourceApplicationSurfaceServices &surfaces;
        retail::SourceWindowMessagePlatform &platform;
        retail::SourceApplicationMenuServices *application{};
        retail::SourceMenuNavigationBindingChildren *navigation{};
    };
    std::optional<MenuWindowBinding> menu_window_binding;
    retail::Win32SourceWindowMessagePlatform menu_window_platform;
    std::exception_ptr window_failure;
    bool window_tearing_down{};
    bool source_callbacks_retired{}; // NEW terminal host phase, not a source word
    std::size_t window_callback_depth{};
    // NEW HOST owner: destroyed before its native slot, renderer, literal owner
    // and exception latch. Entry/shutdown receivers are external lifetime borrows.
    std::unique_ptr<retail::SourceApplicationCore> source_application_core;

    retail::SourceApplicationCore &prepare_source_core_before_window(HINSTANCE instance,
                                                                     PolygonSubmissionCaptureLimits limits,
                                                                     retail::SourceDrawStartupFailure &startup_failure,
                                                                     std::optional<std::string> host_tz_snapshot)
    {
        rethrow_window_failure();
        if (!instance || window || renderer || race_application || source_application_core || menu_window_binding ||
            window_tearing_down || source_callbacks_retired || window_callback_depth != 0)
            throw std::logic_error("Source core requires an unused pre-HWND App on its owner thread");
        auto render_options = options.renderer;
        render_options.capture_frames = !options.capture.empty();
        auto candidate = std::make_unique<retail::SourceApplicationCore>(
            source_application_literals, options.root / "data/retail",
            retail::NewHostNativeWindowBorrow{instance, window}, renderer, window_failure, render_options, limits,
            startup_failure, std::move(host_tz_snapshot));
        // No native dispatch between validation, staging and no-throw ownership
        // publication. Construction failure leaves no dangling callback binding.
        bind_source_window_words(candidate->window_words(), candidate->surface_bridge(), menu_window_platform);
        source_application_core = std::move(candidate);
        return *source_application_core;
    }

    void bind_source_window_words(retail::SourceWindowMessageWords words,
                                  retail::SourceApplicationSurfaceServices &surfaces,
                                  retail::SourceWindowMessagePlatform &platform)
    {
        if (window || window_tearing_down || source_callbacks_retired || menu_window_binding)
            throw std::logic_error("Source window words must attach before native window creation");
        menu_window_binding.emplace(MenuWindowBinding{words, surfaces, platform});
    }
    void bind_source_menu_window(retail::SourceApplicationMenuServices &application,
                                 retail::SourceMenuNavigationBindingChildren &navigation, std::uint32_t &active,
                                 const std::uint32_t &mode)
    {
        if (!window || window_tearing_down || source_callbacks_retired || window_callback_depth != 0 ||
            window_failure || !menu_window_binding || menu_window_binding->application)
            throw std::logic_error("Full menu attachment requires its live staged window");
        auto &staged = *menu_window_binding;
        const auto words = navigation.window_message_words(active, mode);
        if (&words.active_61c374 != &staged.words.active_61c374 || &words.mode_61c364 != &staged.words.mode_61c364 ||
            &words.character_62ddfc != &staged.words.character_62ddfc)
            throw std::logic_error("Full menu attachment must retain all three staged source word identities");
        application.require_window_message_binding(navigation, words, staged.surfaces);
        // Publish only after validation. No input byte clear, copy, queue or replay.
        staged.application = &application;
        staged.navigation = &navigation;
    }
    void withdraw_source_menu_callbacks()
    {
        // A copied binding remains borrowed until the outermost callback returns.
        // Unpublishing pointers cannot authorize destruction inside that callback.
        if (window_callback_depth != 0)
            throw std::logic_error("Menu callbacks must be withdrawn outside native dispatch");
        if (menu_window_binding)
        {
            menu_window_binding->application = nullptr;
            menu_window_binding->navigation = nullptr;
        }
        // Keep the same words/surface bridge for44E6D0 until native destruction.
    }
    void retire_source_callbacks_for_final_shutdown()
    {
        withdraw_source_menu_callbacks(); // rejects a live copied callback borrow
        source_callbacks_retired = true;
        // Terminal NEW HOST policy: no mode/restore/character callbacks may
        // enter the bridge after final shutdown detaches its GPU receivers.
        // WM_DESTROY still reaches44E6D0's PostQuitMessage/default processing.
    }
    void detach_source_menu_window() noexcept
    {
        menu_window_binding.reset();
    }
    bool window_active() const noexcept
    {
        return menu_window_binding ? menu_window_binding->words.active_61c374 != 0u : preview_active;
    }
    bool window_mode_ready() const noexcept
    {
        return !menu_window_binding || menu_window_binding->words.mode_61c364 == 0u;
    }
    void capture_window_failure() noexcept
    {
        if (!window_failure)
            window_failure = std::current_exception();
        running = false;
    }
    void rethrow_window_failure() const
    {
        if (window_failure)
            std::rethrow_exception(window_failure);
    }
    LRESULT message(HWND receiver, UINT msg, WPARAM wp, LPARAM lp)
    {
        struct CallbackBorrowScope
        {
            std::size_t &depth;
            explicit CallbackBorrowScope(std::size_t &value) noexcept : depth(value)
            {
                ++depth;
            }
            ~CallbackBorrowScope() noexcept
            {
                --depth;
            }
        } borrowed{window_callback_depth};
        if (source_callbacks_retired && msg != WM_DESTROY)
            return DefWindowProcA(receiver, msg, wp, lp);
        // Only 44E6D0's five source message cases have source side effects.
        // Attached menu input must not also run preview camera/model/exit keys.
        // WM_CLOSE must reach the source default procedure: native destruction
        // then sends WM_DESTROY, whose source case posts WM_QUIT for44EA60.
        // Remaining native sizing/DPI policy stays in the D3D12 host.
        // ANSI native registration preserves the retail WM_CHAR byte contract.
        // The window, DPI handling and renderer remain the D3D12 host.
        if (menu_window_binding &&
            (msg == WM_CLOSE || msg == WM_DESTROY || msg == WM_MOVE || msg == WM_SIZE || msg == WM_ACTIVATEAPP ||
             msg == WM_CHAR || msg == WM_KEYDOWN || msg == WM_KEYUP || msg == WM_SYSKEYUP || msg == WM_LBUTTONDOWN ||
             msg == WM_LBUTTONUP || msg == WM_MOUSEMOVE || msg == WM_MOUSEWHEEL))
        {
            const auto binding = *menu_window_binding; // references, never word copies
            // Host bookkeeping first: a nested resize in the source mode child
            // must remain the newest pending D3D12 size.
            if (msg == WM_SIZE)
                (void)host_message(receiver, msg, wp, lp);
            if (window != receiver || window_failure)
                return 0;
            if (msg == WM_DESTROY)
                running = false;
            const retail::SourceWindowMessage native_message{reinterpret_cast<std::uintptr_t>(receiver), msg,
                                                             static_cast<std::uintptr_t>(wp),
                                                             static_cast<std::intptr_t>(lp)};
            const auto result = binding.application
                                    ? binding.application->dispatch_window_message(
                                          native_message, *binding.navigation, binding.words.active_61c374,
                                          binding.words.mode_61c364, binding.platform)
                                    : retail::source_window_proc_44e6d0(native_message, binding.words, binding.surfaces,
                                                                        binding.platform);
            // Source mode/default calls may synchronously destroy the HWND or
            // record a failure in a nested callback. Never resume stale work.
            if (window == receiver && !window_failure && msg == WM_ACTIVATEAPP)
                (void)host_message(receiver, msg, wp, lp);
            return static_cast<LRESULT>(result);
        }
        return host_message(receiver, msg, wp, lp);
    }
    void reset_view()
    {
        center = (model.minimum + model.maximum) * 0.5f;
        auto ext = model.maximum - model.minimum;
        radius = std::max(0.1f, std::sqrt(dot(ext, ext)) * 0.5f);
        distance = radius * 2.7f;
        yaw = 0.65f;
        pitch = model.kind == 1 ? 0.65f : 0.28f;
    }
    void load(bool reset)
    {
        if (options.race)
        {
            if (!race_application)
            {
                race_application = std::make_unique<RaceApplication>(
                    options.root / "data/retail",
                    RaceApplicationOptions{options.level, options.play_mode, options.muted, options.challenge_ticks,
                                           options.interpolate, options.world_join_range},
                    window, *renderer, source_application_literals);
                ++race_sessions_started;
            }
            else
                race_application->bind_renderer(*renderer);
            SetWindowTextW(window, options.play_mode == RaceApplicationMode::normal_race
                                       ? L"StuntGP NEW | Wyscig | DirectX 12"
                                   : options.play_mode == RaceApplicationMode::challenge
                                       ? L"StuntGP NEW | Wyzwanie | DirectX 12"
                                       : L"StuntGP NEW | Proba czasowa | DirectX 12");
            display_title =
                options.level + (options.play_mode == RaceApplicationMode::normal_race ? " / WYSCIG"
                                 : options.play_mode == RaceApplicationMode::challenge ? " / WYZWANIE"
                                                                                       : " / PROBA CZASOWA");
            events.push_back("race-scene-bound");
            return;
        }
        SetWindowTextW(window, L"StuntGP NEW | Loading original model...");
        auto next = assets.model(names[selection], lod, options.track_set);
        if (options.rig_check)
        {
            auto rig = assets.rig(names[selection], lod, options.track_set);
            auto poses = authored_scene_parts(rig);
            renderer->load(rig.geometry, assets);
            model_parts = std::move(poses);
            rig_instance = SceneInstance{0, {}, model_parts};
        }
        else if (options.scene_check)
        {
            auto other = assets.model("car1");
            std::array<Model, 2> scene{next, std::move(other)};
            renderer->load_scene(scene, assets);
        }
        else
            renderer->load(next, assets);
        model = std::move(next);
        if (reset)
            reset_view();
        if (options.scene_check && reset)
        {
            scene_instances = {SceneInstance{0, {}}, SceneInstance{1, {}}};
            scene_instances[0].pose.position.x = -radius;
            scene_instances[1].pose.position.x = radius;
            scene_instances[1].pose.right = {0, 0, -1};
            scene_instances[1].pose.forward = {1, 0, 0};
            distance = radius * 5;
        }
        if (options.scene_mirror && reset)
            scene_instances[0].pose.right = {-1, 0, 0};
        if (options.zoom_out_check)
            distance = radius * 30;
        display_title = names[selection] + "   /   LOD " + std::to_string(lod) + "   /   " +
                        (options.rig_check     ? "Original authored parts / GPU diagnostic"
                         : options.scene_check ? "Two-mesh platform check"
                                               : "Original mesh study");
        display_detail.clear();
        if (fixed_camera)
            display_title = names[selection] + " / Original camera projection diagnostic";
        SetWindowTextW(window, L"StuntGP NEW | Reconstruction foundation | DirectX 12");
        events.push_back("loaded:" + names[selection] + ":lod=" + std::to_string(lod));
    }
    void select(int delta)
    {
        selection = static_cast<unsigned>((static_cast<int>(selection) + delta + static_cast<int>(names.size())) %
                                          static_cast<int>(names.size()));
        lod = 0;
        reload_requested = reload_reset = true;
    }
    void choose(const char *name)
    {
        auto it = std::find(names.begin(), names.end(), name);
        if (it != names.end())
        {
            selection = static_cast<unsigned>(it - names.begin());
            lod = 0;
            reload_requested = reload_reset = true;
        }
    }
    // NEW HOST (R24): Alt+Enter (and F11 in the preview host) switch between
    // windowed and the last fullscreen mode through host_graphics::set (not
    // saved; saving belongs to the menus). The change itself is applied by
    // poll_display_mode() at the next safe point, like a menu change.
    void toggle_display_mode()
    {
        display_modes.toggle();
        events.push_back("display-mode-toggle");
    }
    // NEW HOST (R24) display-mode safe point (host_display_mode.hpp): never
    // inside a window callback, a renderer call or a source frame. The game
    // path runs it before every native PeekMessageA of 44EA60 (DisplayModeMessages),
    // the preview path at the top of each loop iteration.
    void poll_display_mode(bool defer_device_loss)
    {
        if (!window || window_tearing_down || source_callbacks_retired || window_callback_depth != 0)
            return;
        if (display_modes.poll(renderer.get(), defer_device_loss))
            events.push_back("display-mode:" + std::string(host_display::name(display_modes.applied())));
    }
    LRESULT host_message(HWND receiver, UINT msg, WPARAM wp, LPARAM lp)
    {
        switch (msg)
        {
        case WM_CLOSE:
            events.push_back("window-close-request");
            running = false;
            return 0;
        case WM_DESTROY:
            running = false;
            PostQuitMessage(0);
            return 0;
        case WM_SIZE:
            minimized = wp == SIZE_MINIMIZED;
            pending_width = LOWORD(lp);
            pending_height = HIWORD(lp);
            if (minimized)
            {
                suspend_clock();
                held_keys.fill(false);
                race_reset_requested = false;
                if (race_application)
                    race_application->release_input();
            }
            return 0;
        case WM_DPICHANGED: {
            events.push_back("dpi:" + std::to_string(HIWORD(wp)));
            // NEW HOST (R24): a fullscreen window keeps covering its monitor
            // (refitted at the next safe point), never the suggested rectangle,
            // which exclusive mode switches would otherwise shrink it to.
            if (display_modes.fullscreen_style())
            {
                display_modes.on_display_change();
                return 0;
            }
            auto *r = reinterpret_cast<RECT *>(lp);
            SetWindowPos(window, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
            return 0;
        }
        case WM_DISPLAYCHANGE:
            events.push_back("display-change");
            display_modes.on_display_change(); // NEW HOST (R24): borderless follows the monitor extent
            return 0;
        case WM_ACTIVATEAPP:
            display_modes.on_activate_app(wp !=
                                          0); // NEW HOST (R24): exclusive fullscreen follows focus at the next poll
            if (!menu_window_binding)
                preview_active = wp != 0;
            rebase_time = true;
            if (!window_active())
            {
                suspend_clock();
                dragging = false;
                held_keys.fill(false);
                race_reset_requested = false;
                if (race_application)
                    race_application->release_input();
                ReleaseCapture();
            }
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_GETMINMAXINFO: {
            auto *p = reinterpret_cast<MINMAXINFO *>(lp);
            UINT dpi = GetDpiForWindow(window);
            p->ptMinTrackSize = {MulDiv(480, static_cast<int>(dpi), 96), MulDiv(320, static_cast<int>(dpi), 96)};
            p->ptMaxTrackSize = {32768, 32768};
            return 0;
        }
        case WM_LBUTTONDOWN:
            if (options.race)
                return 0;
            dragging = true;
            mouse = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            SetCapture(window);
            return 0;
        case WM_LBUTTONUP:
            dragging = false;
            ReleaseCapture();
            return 0;
        case WM_CAPTURECHANGED:
            dragging = false;
            return 0;
        case WM_MOUSEMOVE:
            if (dragging)
            {
                POINT now{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
                float dpi = static_cast<float>(GetDpiForWindow(window));
                yaw -= static_cast<float>(now.x - mouse.x) * 0.005f * 96 / dpi;
                pitch = std::clamp(pitch + static_cast<float>(now.y - mouse.y) * 0.005f * 96 / dpi, -1.45f, 1.45f);
                mouse = now;
            }
            return 0;
        case WM_MOUSEWHEEL:
            if (options.race)
                return 0;
            distance = std::clamp(distance * std::exp(-static_cast<float>(GET_WHEEL_DELTA_WPARAM(wp)) * 0.001f),
                                  radius * 0.02f, radius * 30);
            return 0;
        case WM_SYSKEYUP:
            if (wp < held_keys.size())
                held_keys[wp] = false;
            return DefWindowProcA(receiver, msg, wp, lp);
        case WM_SYSKEYDOWN:
            // NEW HOST (R24): Alt+Enter has exactly one owner. DXGI's handling is
            // disabled (MakeWindowAssociation DXGI_MWA_NO_ALT_ENTER, renderer.cpp).
            // Bit 29: ALT held; bit 30: auto-repeat (one toggle per press).
            if (wp == VK_RETURN && (lp & (1ll << 29)))
            {
                if (!(lp & (1ll << 30)))
                    toggle_display_mode();
                return 0;
            }
            return DefWindowProcA(receiver, msg, wp, lp);
        case WM_SYSCHAR:
            if (wp == '\r')
                return 0; // Alt+Enter: no menu-accelerator beep
            return DefWindowProcA(receiver, msg, wp, lp);
        case WM_ENTERSIZEMOVE:
            suspend_clock();
            held_keys.fill(false);
            race_reset_requested = false;
            if (race_application)
                race_application->release_input();
            rebase_time = true;
            return 0;
        case WM_EXITSIZEMOVE:
            suspend_clock();
            rebase_time = true;
            return 0;
        case WM_KEYUP:
            if (wp < held_keys.size())
                held_keys[wp] = false;
            return 0;
        case WM_KEYDOWN:
            if (options.race && wp < held_keys.size())
                held_keys[wp] = true;
            if (lp & (1ll << 30))
                return 0;
            if (wp == VK_ESCAPE && !options.race)
            {
                if (display_modes.desired() != host_display::DisplayMode::windowed)
                    display_modes.request(host_display::DisplayMode::windowed); // NEW HOST (R24)
                else
                {
                    events.push_back("escape-close-request");
                    running = false;
                }
            }
            else if (wp == VK_F11)
                toggle_display_mode(); // NEW HOST (R24): same owner as Alt+Enter
            else if (options.race && wp == VK_RETURN && race_application && race_application->menu_requested())
                race_restart_requested = true;
            else if (options.race && wp == 'R')
                race_reset_requested = true;
            else if (options.race && wp != 'V')
                return 0;
            else if (wp == VK_RIGHT)
                select(1);
            else if (wp == VK_LEFT)
                select(-1);
            else if (wp == 'T')
                choose("track1");
            else if (wp == 'C')
                choose("car16");
            else if (wp == 'R')
                reset_view();
            else if (wp == 'L')
            {
                if (!reload_reset)
                    lod = (lod + 1) % model.lod_count;
                reload_requested = true;
            }
            else if (wp == 'V')
            {
                options.renderer.vsync = !options.renderer.vsync;
                renderer->vsync(options.renderer.vsync);
                events.push_back(options.renderer.vsync ? "vsync-on" : "vsync-off");
            }
            return 0;
        }
        return DefWindowProcA(receiver, msg, wp, lp);
    }
    void exercise(unsigned frame)
    {
        if (!options.exercise)
            return;
        if (frame == 15)
        {
            SetWindowPos(window, nullptr, 0, 0, 1920, 1080, SWP_NOMOVE | SWP_NOZORDER);
            events.push_back("resize-wide");
        }
        if (frame == 30)
        {
            SetWindowPos(window, nullptr, 0, 0, 700, 950, SWP_NOMOVE | SWP_NOZORDER);
            events.push_back("resize-portrait");
        }
        if (frame == 45)
            toggle_display_mode();
        if (frame == 60)
            toggle_display_mode();
        if (frame == 70)
        {
            ShowWindow(window, SW_MINIMIZE);
            events.push_back("minimize");
            SetTimer(window, 1, 150, nullptr);
        }
        if (frame == 85)
        {
            options.renderer.vsync = false;
            renderer->vsync(false);
            events.push_back("vsync-off");
        }
        if (frame == 100)
        {
            options.renderer.vsync = true;
            renderer->vsync(true);
            events.push_back("vsync-on");
        }
    }
    void report()
    {
        renderer->finish();
        const auto &s = renderer->stats();
        const auto cpu = frame_ms.summarize(), interval = interval_ms.summarize(), wait = wait_ms.summarize();
        auto percentile = [&](double p) { return p < 0.9 ? cpu.p50 : (p < 0.99 ? cpu.p95 : cpu.p99); };
        std::filesystem::create_directories(options.report.parent_path());
        std::ofstream out(options.report);
        out << std::boolalpha << std::setprecision(8) << "{\n  \"completed\": "
            << (failure.empty() && s.debug_errors == 0 && previous_debug_errors == 0 && requested_run_finished())
            << ",\n  \"interrupted\": " << (failure.empty() && !requested_run_finished())
            << ",\n  \"requested_simulation_ticks\": " << (options.drive_smoke ? options.drive_ticks : 0)
            << ",\n  \"race_sessions_started\": " << race_sessions_started
            << ",\n  \"adapter\": " << json_string(s.adapter)
            << ",\n  \"api\": \"D3D12\",\n  \"model\": " << json_string(options.race ? options.level : names[selection])
            << ",\n  \"width\": " << s.width << ", \"height\": " << s.height << ",\n  \"msaa_samples\": " << s.samples
            << ",\n  \"tearing_supported\": " << s.tearing_supported << ",\n  \"debug_layer\": " << s.debug_active
            << ", \"debug_warnings_or_errors\": " << s.debug_errors << ",\n  \"presents\": " << s.presents
            << ", \"draw_calls_last_frame\": " << s.draws << ",\n  \"simulation_clock_ticks\": " << clock.ticks()
            << ", \"tick_checksum\": " << tick_checksum << ",\n  \"discarded_stall_ns\": " << clock.discarded_ns()
            << ",\n  \"cpu_render_submit_ms_p50\": " << percentile(0.5)
            << ", \"cpu_render_submit_ms_p95\": " << percentile(0.95)
            << ", \"cpu_render_submit_ms_p99\": " << percentile(0.99)
            << ",\n  \"measurement_scope\": \"CPU recording, submission and Present; excludes frame-latency wait; not "
               "GPU time\",\n  \"physics_implemented\": "
            << bool(race_application)
            << ", \"retail_visual_parity_proven\": false,\n  \"failure\": " << json_string(failure)
            << ",\n  \"events\": [";
        for (std::size_t i = 0; i < events.size(); ++i)
        {
            if (i)
                out << ",";
            out << json_string(events[i]);
        }
        out << "],\n  \"occluded_presents\": " << s.occluded_presents
            << ",\n  \"gpu_ms_last_resolved\": " << s.gpu_ms_last
            << ",\n  \"timing_window_samples\": " << cpu.window_samples
            << ", \"timing_total_samples\": " << cpu.total_samples << ",\n  \"frame_interval_ms_p50\": " << interval.p50
            << ", \"frame_interval_ms_p95\": " << interval.p95 << ", \"frame_interval_ms_p99\": " << interval.p99
            << ",\n  \"frame_latency_wait_ms_p95\": " << wait.p95 << ",\n  \"scene_meshes\": " << s.meshes
            << ", \"scene_instances\": " << s.instances << ",\n  \"uploaded_vertex_bytes\": " << s.uploaded_vertex_bytes
            << ", \"uploaded_texture_texel_bytes\": " << s.uploaded_texture_bytes
            << ",\n  \"total_presents\": " << (previous_presents + s.presents)
            << ", \"device_recreations\": " << device_recreations
            << ",\n  \"previous_device_debug_errors\": " << previous_debug_errors
            << ",\n  \"device_loss_check\": " << options.device_loss_check
            << ",\n  \"inspection_distance\": " << last_frustum.distance
            << ", \"inspection_far_plane\": " << last_frustum.far_plane
            << ",\n  \"focus_resume_ticks\": " << focus_resume_ticks << ",\n  \"scene_check\": " << options.scene_check
            << ",\n  \"rig_check\": " << options.rig_check
            << ",\n  \"source_camera_fixture\": " << fixed_camera.has_value() << ",\n  \"gpu_diagnostics\": [";
        bool first = true;
        auto diagnostic = [&](const GpuDiagnostic &entry) {
            if (!first)
                out << ',';
            first = false;
            out << "{\"id\":" << entry.id << ",\"severity\":" << entry.severity
                << ",\"message\":" << json_string(entry.message) << '}';
        };
        for (const auto &entry : previous_diagnostics)
            diagnostic(entry);
        for (const auto &entry : renderer->diagnostics())
            diagnostic(entry);
        out << "]";
        if (race_application)
        {
            out << ",\n  \"race\": ";
            race_application->report(out);
        }
        out << "\n}\n";
        if (!out)
            throw std::runtime_error("cannot save runtime report");
    }
    bool requested_run_finished() const
    {
        return !options.drive_smoke || (race_application && race_application->ticks() >= options.drive_ticks);
    }
    int run()
    {
        rethrow_window_failure();
        if (source_application_core || menu_window_binding)
            throw std::logic_error("Staged source entry must not run the preview Renderer constructor");
        options.renderer.capture_frames = !options.capture.empty();
        renderer = std::make_unique<Renderer>(window, options.renderer);
        rethrow_window_failure();
        load(true);
        rethrow_window_failure();
        ShowWindow(window, options.smoke ? SW_SHOWNOACTIVATE : SW_SHOW);
        rethrow_window_failure();
        UpdateWindow(window);
        rethrow_window_failure();
        if (options.smoke && !options.exercise)
        {
            RECT requested{0, 0, static_cast<LONG>(options.width), static_cast<LONG>(options.height)};
            AdjustWindowRectExForDpi(&requested, WS_OVERLAPPEDWINDOW, FALSE, 0, GetDpiForWindow(window));
            SetWindowPos(window, nullptr, 0, 0, requested.right - requested.left, requested.bottom - requested.top,
                         SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        }
        using Time = std::chrono::steady_clock;
        auto previous = Time::now();
        unsigned rendered = 0;
        double pending_wait_ms = 0;
        const auto clock_enabled = [&] {
            return (!race_application || !race_application->menu_requested()) && window_mode_ready() &&
                   (window_active() || (options.smoke && !options.race) ||
                    (options.drive_smoke && !options.focus_check));
        };
        const auto simulation_step = [&](auto tick) {
            if (race_application)
            {
                if (race_application->menu_requested() || race_application->exit_requested() ||
                    (options.drive_smoke && race_application->ticks() >= options.drive_ticks))
                    return false;
                // Synthetic menu keys belong to this accepted60Hz step, not
                // the display frame; no global keyboard injection is involved.
                auto step_keys = held_keys;
                if (options.drive_smoke)
                {
                    step_keys.fill(false);
                    step_keys[VK_ESCAPE] = options.pause_at_tick && race_application->ticks() == *options.pause_at_tick;
                }
                race_application->tick(step_keys, race_reset_requested, options.drive_smoke, *renderer);
                if (race_application->exit_requested())
                {
                    events.push_back("pause_menu_exit_host_preview");
                    running = false;
                }
                race_reset_requested = false;
            }
            tick_checksum += tick;
            return true;
        };
        while (running)
        {
            rethrow_window_failure();
            MSG msg{};
            for (;;)
            {
                const bool received = PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE) != FALSE;
                // PeekMessage can dispatch sent messages even when it returns false.
                rethrow_window_failure();
                if (!received)
                    break;
                if (msg.message == WM_QUIT)
                    running = false;
                if (msg.message == WM_TIMER && msg.hwnd == window && msg.wParam == 1)
                {
                    KillTimer(window, 1);
                    ShowWindow(window, SW_RESTORE);
                    events.push_back("restore");
                }
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
                rethrow_window_failure();
            }
            if (!running)
                break;
            if (!failure.empty())
                break;
            try
            {
                poll_display_mode(false); // NEW HOST (R24): safe point; device loss reaches the recovery below
                if (race_restart_requested)
                {
                    // New frontend boundary after the recovered441E20 request.
                    // Destroy the old session before binding fresh audio/GPU data.
                    renderer->collect_diagnostics();
                    const auto retired_stats = renderer->stats();
                    previous_presents += retired_stats.presents;
                    previous_debug_errors += retired_stats.debug_errors;
                    const auto &retired_diagnostics = renderer->diagnostics();
                    previous_diagnostics.insert(previous_diagnostics.end(), retired_diagnostics.begin(),
                                                retired_diagnostics.end());
                    race_application.reset(); // release recipe/material borrowers first
                    renderer.reset();         // the new Session requires empty source surface roles
                    renderer = std::make_unique<Renderer>(window, options.renderer);
                    held_keys.fill(false);
                    race_reset_requested = false;
                    race_restart_requested = false;
                    clock = TickClock{};
                    tick_checksum = 0;
                    rethrow_window_failure();
                    load(true);
                    rethrow_window_failure();
                    events.push_back("race-restarted");
                    previous = Time::now();
                    pending_wait_ms = 0;
                }
                if (reload_requested)
                {
                    load(reload_reset);
                    reload_requested = reload_reset = false;
                    previous = Time::now();
                    suspend_clock();
                }
                if (minimized)
                {
                    MsgWaitForMultipleObjectsEx(0, nullptr, 250, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
                    previous = Time::now();
                    pending_wait_ms = 0;
                    continue;
                }
                if (!options.smoke && renderer->occluded())
                {
                    suspend_clock();
                    MsgWaitForMultipleObjectsEx(0, nullptr, 100, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
                    previous = Time::now();
                    pending_wait_ms = 0;
                    continue;
                }
                // Source Flip selects the previous completed back image. The
                // first accepted step has no front yet: advance normal ticks
                // until one exists before consuming a swapchain frame event.
                if (race_application && !race_application->ready_to_draw())
                {
                    const auto now = Time::now();
                    if (rebase_time)
                    {
                        previous = now;
                        suspend_clock();
                        rebase_time = false;
                    }
                    if (clock_enabled())
                        clock.advance(std::chrono::duration_cast<std::chrono::nanoseconds>(now - previous),
                                      simulation_step);
                    previous = now;
                    if (!race_application->ready_to_draw())
                    {
                        if (options.drive_smoke && race_application->ticks() >= options.drive_ticks)
                            throw FormatError("source front requires at least two accepted drive steps");
                        MsgWaitForMultipleObjectsEx(0, nullptr, 17, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
                        continue;
                    }
                }
                const auto wait_begin = Time::now();
                HANDLE ready = renderer->frame_event();
                auto wait = MsgWaitForMultipleObjectsEx(1, &ready, 250, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
                pending_wait_ms += std::chrono::duration<double, std::milli>(Time::now() - wait_begin).count();
                if (wait == WAIT_OBJECT_0 + 1)
                    continue;
                if (wait == WAIT_FAILED)
                    throw std::runtime_error("frame wait failed");
                if (wait == WAIT_TIMEOUT)
                {
                    renderer->check_health();
                    continue;
                }
                wait_ms.record(pending_wait_ms);
                pending_wait_ms = 0;
                if (pending_width && pending_height)
                {
                    renderer->resize(pending_width, pending_height);
                    if (race_application)
                        race_application->resize(pending_width, pending_height);
                    events.push_back("surface:" + std::to_string(pending_width) + "x" + std::to_string(pending_height));
                    pending_width = pending_height = 0;
                }
                auto now = Time::now();
                interval_ms.record(std::chrono::duration<double, std::milli>(now - previous).count());
                if (rebase_time)
                {
                    previous = now;
                    suspend_clock();
                    rebase_time = false;
                }
                if (clock_enabled())
                    clock.advance(std::chrono::duration_cast<std::chrono::nanoseconds>(now - previous),
                                  simulation_step);
                if (focus_resume_pending)
                {
                    focus_resume_ticks = clock.ticks() - focus_before_ticks;
                    focus_resume_pending = false;
                    events.push_back("focus-resumed");
                }
                previous = now;
                const auto &display = renderer->stats();
                last_frustum = inspection_frustum(radius, distance, display.width, display.height);
                Vec3 eye =
                    center + Vec3{std::sin(yaw) * std::cos(pitch), std::sin(pitch), std::cos(yaw) * std::cos(pitch)} *
                                 last_frustum.distance;
                View view{eye, center, last_frustum.near_plane, last_frustum.far_plane};
                if (fixed_camera)
                    view = *fixed_camera;
                const auto &s = renderer->stats();
                if (display_detail.empty() || detail_width != s.width || detail_height != s.height ||
                    detail_samples != s.samples || detail_vsync != options.renderer.vsync)
                {
                    detail_width = s.width;
                    detail_height = s.height;
                    detail_samples = s.samples;
                    detail_vsync = options.renderer.vsync;
                    display_detail = std::to_string(s.uploaded_vertex_bytes / (3 * sizeof(Vertex))) +
                                     " triangles  /  " + std::to_string(s.width) + " x " + std::to_string(s.height) +
                                     "  /  DX12  /  " + (options.renderer.vsync ? "VSync" : "Uncapped") + "  /  " +
                                     std::to_string(s.samples) + "x MSAA";
                }
                auto begin = Time::now();
                const auto dpi = static_cast<float>(GetDpiForWindow(window));
                if (race_application)
                    race_application->draw(*renderer, dpi, window_active() || options.drive_smoke,
                                           clock_enabled() ? clock.alpha() : 1.0);
                else if (options.rig_check)
                    renderer->draw_scene(view, std::span(&rig_instance, 1), display_title, display_detail, dpi);
                else if (options.scene_check)
                    renderer->draw_scene(view, scene_instances, display_title, display_detail, dpi);
                else
                    renderer->draw(view, display_title, display_detail, dpi);
                frame_ms.record(std::chrono::duration<double, std::milli>(Time::now() - begin).count());
                ++rendered;
                exercise(rendered);
                rethrow_window_failure();
                if (options.drive_smoke && race_application->ticks() >= options.drive_ticks)
                    break;
                if (options.frames && rendered >= options.frames)
                    break;
                if (options.debug_error_check && rendered == 19)
                    renderer->inject_debug_error_for_test();
                if (options.focus_check && rendered == 10)
                {
                    focus_before_ticks = clock.ticks();
                    SendMessageW(window, WM_ACTIVATEAPP, FALSE, 0);
                    Sleep(100); // Diagnostic only: stale wall time must not become simulation ticks.
                    SendMessageW(window, WM_ACTIVATEAPP, TRUE, 0);
                    focus_resume_pending = true;
                    events.push_back("injected-focus-cycle");
                }
                if (options.device_loss_check && rendered == 20 && device_recreations == 0)
                {
                    renderer->collect_diagnostics();
                    events.push_back("injected-device-removal");
                    renderer->remove_device_for_test(options.reload_loss_check);
                    if (options.reload_loss_check)
                    {
                        SendMessageW(window, WM_KEYDOWN, 'C', 0);
                        events.push_back("queued-model-after-removal");
                    }
                }
            }
            catch (const GraphicsError &error)
            {
                if (!error.device_lost() || device_recreations >= 3)
                    throw;
                renderer->collect_diagnostics();
                const auto snapshot = renderer->stats();
                previous_presents += snapshot.presents;
                previous_debug_errors += snapshot.debug_errors;
                const auto &diagnostics = renderer->diagnostics();
                previous_diagnostics.insert(previous_diagnostics.end(), diagnostics.begin(), diagnostics.end());
                events.push_back("device-recovery:" + std::to_string(static_cast<unsigned long>(error.result)));
#if defined(SGN_CAMERA_RECOVERY_RECEIVING) && SGN_CAMERA_RECOVERY_RECEIVING == 1
                std::optional<sgn::diagnostics::CameraRecoveryInterval> camera_interval;
                if (!options.camera_recovery_witness.empty())
                {
                    if (!race_application)
                        throw std::runtime_error("camera recovery witness lacks race owner");
                    if (device_recreations == 0)
                        sgn::diagnostics::front_bundle_detail::fresh_directory(options.camera_recovery_witness);
                    camera_interval.emplace(options.camera_recovery_witness /
                                                ("recovery-" + std::to_string(device_recreations + 1)),
                                            device_recreations + 1);
                    camera_interval->begin(race_application->capture_camera_recovery_witness(
                        clock.ticks(), camera_interval->directory() / "front-before"));
                }
#endif
                if (race_application)
                    race_application->suspend_renderer_for_recreation();
                renderer.reset();
                ++device_recreations;
                suspend_clock();
                renderer = std::make_unique<Renderer>(window, options.renderer);
                load(reload_reset);
#if defined(SGN_CAMERA_RECOVERY_RECEIVING) && SGN_CAMERA_RECOVERY_RECEIVING == 1
                if (camera_interval)
                    camera_interval->finish(race_application->capture_camera_recovery_witness(
                        clock.ticks(), camera_interval->directory() / "front-after"));
#endif
                reload_requested = reload_reset = false;
                pending_width = pending_height = 0;
                previous = Time::now();
                pending_wait_ms = 0;
                events.push_back("device-recovered");
            }
            catch (const std::exception &error)
            {
                failure = error.what();
                events.push_back("runtime-failure");
                break;
            }
        }
        rethrow_window_failure();
        if (!options.capture.empty() && failure.empty())
            renderer->capture(options.capture);
        report();
        if (!options.displayed_recipe.empty() && failure.empty())
        {
            try
            {
                if (!race_application)
                    throw std::runtime_error("no retained race application for recipe export");
                if (options.displayed_world_join.empty())
                    race_application->export_displayed_recipe(options.displayed_recipe);
                else if (!race_application->export_displayed_world_join(options.displayed_recipe,
                                                                        options.displayed_world_join))
                    throw std::runtime_error("world join is incomplete for the actual displayed front");
            }
            catch (const std::exception &error)
            {
                failure = std::string(options.displayed_world_join.empty() ? "displayed recipe export: "
                                                                           : "displayed world join export: ") +
                          error.what();
                events.push_back("displayed-recipe-export-failure");
                report();
            }
        }
        if (!options.displayed_front_bundle.empty() && failure.empty())
        {
            try
            {
                if (!race_application)
                    throw std::runtime_error("no race owner for front bundle");
                race_application->export_displayed_front_bundle(options.displayed_front_bundle);
            }
            catch (const std::exception &error)
            {
                failure = std::string("front bundle export: ") + error.what();
                events.push_back("front-bundle-export-failure");
                report();
            }
        }
#if defined(SGN_CAMERA_RECOVERY_RECEIVING) && SGN_CAMERA_RECOVERY_RECEIVING == 1
        if (!options.camera_recovery_witness.empty() && device_recreations == 0 && failure.empty())
        {
            failure = "requested camera recovery witness observed no recovery interval";
            events.push_back("camera-witness-missing-interval");
            report();
        }
#endif
        const auto &stats = renderer->stats();
        if (!failure.empty() || stats.debug_errors != 0 || previous_debug_errors != 0)
            return 1;
        return requested_run_finished() ? 0 : 2;
    }
};
// The guard exists before CreateWindowExA; every startup exit detaches callbacks
// before any menu/race/renderer borrowers are destroyed.
// NEW HOST receivers for the source menu entry. Unreconstructed branches stop
// with their actual source identity; they never report a successful operation.
class SourceMenuUnreconstructed final : public retail::SourceApplicationEntryServices,
                                        public retail::SceneTransitionServices,
                                        public retail::SourceMainMenuRequiredServices
{
    [[noreturn]] static void missing(const char *operation, std::uint32_t identity)
    {
        std::ostringstream message;
        message << "Source menu: unreconstructed " << operation << " (0x" << std::hex << identity << ')';
        throw FormatError(message.str());
    }

  public:
    void show_cursor(bool) override
    {
        missing("unexpected cursor receiver", 0x44ea82);
    }
    std::uint32_t initialize_window_44e890(const retail::SourceApplicationEntryArguments &) override
    {
        missing("unexpected window receiver", 0x44e890);
    }
    void invoke(retail::SourceApplicationCall call) override
    {
        missing("application operation", static_cast<std::uint32_t>(call));
    }
    void perform(retail::SceneOperation call) override
    {
        missing("scene operation", static_cast<std::uint32_t>(call));
    }
    void select_audio_environment(std::uint32_t) override
    {
        missing("audio environment", 0x460af0);
    }
    void initialize_menu_viewport(std::uint32_t) override
    {
        missing("unexpected viewport receiver", 0x424c30);
    }
    void reset_tick_clock() override
    {
        missing("unexpected clock receiver", 0x442100);
    }
    std::uint32_t create_remaining_page(retail::FrontendState &, std::uint32_t, std::uint32_t kind) override
    {
        missing("page factory selector", kind);
    }
    void close_remaining_page(retail::FrontendState &, std::uint32_t, std::uint32_t callback) override
    {
        missing("page close", callback);
    }
    void remaining_page_event(retail::FrontendState &, std::uint32_t, std::uint32_t callback, std::uint32_t,
                              std::uint32_t) override
    {
        missing("page event", callback);
    }
    void run_remaining_mode(retail::FrontendState &, std::uint32_t, std::uint32_t entry) override
    {
        missing("game mode", entry);
    }
};

void record_source_menu_failure(const std::filesystem::path &path, const char *phase,
                                const std::exception_ptr &failure) noexcept
{
    if (!failure)
        return;
    try
    {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream output(path, std::ios::app);
        output << phase << ": ";
        try
        {
            std::rethrow_exception(failure);
        }
        catch (const retail::SourceHostProcessExit &exit)
        {
            output << "source process exit status=" << exit.status << " forced=" << exit.terminate_process;
        }
        catch (const std::exception &error)
        {
            output << error.what();
        }
        catch (...)
        {
            output << "non-standard exception";
        }
        output << '\n';
    }
    catch (...)
    { /* Logging cannot replace the retained source outcome. */
    }
}

// NEW HOST (R23) presentation settings for the game (source menu) path, before
// Core stages the renderer options and before the presenter exists: the file
// <root>/settings/stuntgp-new-graphics.cfg, then this session's command-line
// and environment overrides on top (never saved by the GRAPHICS page unless
// the user changes that setting there). SGN_INTERP is applied after the
// command line because host_presentation::presenter_options applies it last.
void configure_source_menu_graphics(Options &o)
{
    const auto path = host_graphics::default_path(o.root);
    if (!host_graphics::load(path))
        host_graphics_menu::report(o.root, "settings file " + path.string() + " is unreadable; defaults are used");
    const auto persisted = host_graphics::current();
    auto session = persisted;
    host_graphics_menu::SessionOverrides overrides;
    if (o.interpolation_flag)
    {
        session.interpolation = *o.interpolation_flag;
        overrides.interpolation = true;
    }
    char interpolation_environment[16]{};
    std::size_t interpolation_size{};
    if (getenv_s(&interpolation_size, interpolation_environment, sizeof interpolation_environment, "SGN_INTERP") == 0 &&
        interpolation_size > 1 && (interpolation_environment[0] == '0' || interpolation_environment[0] == '1'))
    {
        session.interpolation = interpolation_environment[0] == '1';
        overrides.interpolation = true;
    }
    if (o.aspect_4x3_session) // --aspect-4x3 or SGN_ASPECT=4x3 (host_wide::configure)
    {
        session.original_4x3 = true;
        overrides.original_4x3 = true;
    }
    if (o.vsync_off_session) // --no-vsync or --uncapped
    {
        session.vsync = false;
        overrides.vsync = true;
    }
    host_graphics::set(session);
    host_graphics_menu::begin_session(o.root, persisted, overrides);
    const auto effective = host_graphics::current();
    host_wide::set_original_4x3(effective.original_4x3); // same presentation as the settings
    o.renderer.vsync = effective.vsync;
    o.renderer.requested_samples = effective.msaa_samples;
    // NEW HOST (R23): the renderer follows host_graphics live (MSAA, filtering, mips, A2C, FXAA, vsync).
    o.renderer.host_graphics = true;
    // NEW HOST (R28): pipeline states seen in earlier runs are prewarmed at startup.
    o.renderer.pipeline_state_file = host_graphics::default_path(o.root).parent_path() / "stuntgp-new-pipelines.txt";
    o.interpolate = effective.interpolation;
}

// NEW HOST (R24): the display-mode safe point of the game path. It runs before
// every native PeekMessageA of 44EA60 (inside the R18 pacer, which calls it
// between source passes and between interpolated presentations): no source
// frame, command-list recording or window callback is active there, and no
// source-visible value is read or written. The same window (App::window) is
// the source menu window, the input window and the swap-chain window
// (NewHostNativeWindowBorrow / Renderer::native_window), so switching it
// switches all three. Resizes then follow the existing path (WM_SIZE ->
// 44E9D0 idle -> resize_render_targets -> RendererLoadingSurfaces restore).
class DisplayModeMessages final : public retail::SourceApplicationMessages
{
    retail::SourceApplicationMessages &native_;
    App &app_;
    const SourceMenuPresentationBackend &presentation_;

  public:
    DisplayModeMessages(retail::SourceApplicationMessages &native, App &app,
                        const SourceMenuPresentationBackend &presentation) noexcept
        : native_(native), app_(app), presentation_(presentation)
    {
    }
    bool peek_removed() override
    {
        // Device loss is left to the source path's recovery (44E9D0 idle ->
        // NewHostD3D12ApplicationSurfaces::recreate_removed_renderer).
        if (!presentation_.source_frame_active())
            app_.poll_display_mode(true);
        return native_.peek_removed();
    }
    std::uint32_t message_id() const override
    {
        return native_.message_id();
    }
    std::uint32_t source_wparam() const override
    {
        return native_.source_wparam();
    }
    void translate() override
    {
        native_.translate();
    }
    void dispatch() override
    {
        native_.dispatch();
    }
};

int run_source_menu(App &app, const retail::SourceApplicationEntryArguments &arguments)
{
    if (!app.source_application_core || !app.window)
        throw FormatError("Source menu requires the staged Core and actual native window");
    auto &core = *app.source_application_core;
    retail::NativeSourceMenuCrt crt(app.source_application_literals.reference(), core.platform());
    crt.initialize(); // Actual process CRT startup, once, before source44EA60.
    PersistenceFileService files(app.options.root);
    // NEW HOST install-layout guarantee (R22), before any source save/scan code
    // runs: <root>/save/card00 exists, as in the retail installed tree (its
    // save/ and save/card00/ directory entries). The original never creates save:
    // its only reached CreateDirectoryA path is 463AFF from the user-confirmed
    // card format 440180 (4401A1), a single mkdir of 4401E0's "%s\save\card%02d";
    // 44B040's mkdir has no caller. The automatic post-Language save (457EC0
    // mode 3, operation 2 -> save_persistence) never formats, so without the
    // installed card it fails invisibly and retries. Same private root and
    // reparse checks as the source transport; creates only missing directories,
    // never touches existing cards or files, never writes a save format.
    files.format_card(0);
    retail::SourceSaveNativePersistence save(core, crt, files);
    retail::SourceFrontendDataStartupImpl data;
    SourceMenuUnreconstructed remaining;
    retail::Win32SourceApplicationMessages native_messages(app.window_failure);
    // NEW HOST pacing (R18 P2) at the unchanged 44EAE6 PeekMessageA boundary:
    // a pass is released only when the source clock has a due tick, after the
    // swap chain frame-latency wait. Read-only borrows of the same world words
    // 44EA60 reads; App.renderer is borrowed as a slot (created by 44E890).
    auto &paced_world = core.frontend().world();
    const auto paced_words = core.window_words();
    // NEW HOST high-refresh presentation (interp R19): between due ticks the
    // adapter presents frames interpolated from the canonical 4427B0 captures
    // at the display rate (vsync interval 1 at any Hz, or uncapped). The source
    // pass cadence is unchanged. --no-interpolation or SGN_INTERP=0: original
    // 60 Hz presentation. NEW R23: the value comes from the graphics settings
    // (configure_source_menu_graphics) and the GRAPHICS page changes it live.
    host_presentation::InterpolatingPresenter presenter(
        core.frontend(), core.presentation(), app.renderer, core.counter(),
        host_presentation::presenter_options(host_graphics::current().interpolation));
    DisplayModeMessages display_messages(native_messages, app, core.presentation()); // NEW HOST (R24)
    retail::SourceHostPacedMessages messages(
        display_messages,
        retail::SourceHostPacingWorld{
            paced_world.scene_mode, paced_world.tick_clock, paced_world.source_scheduler.fixed_mode_a,
            paced_world.source_scheduler.fixed_mode_b, paced_words.active_61c374, paced_words.mode_61c364},
        core.counter(), app.renderer, &presenter);
    // NEW HOST (R34) stutter detector / frame trace (host_frame_trace.hpp), diagnostics only: reports/
    // stutter-log.txt and fps-log.txt (SGN_STUTTER=0 off), SGN_TRACE=<file> full trace. Outlives Composition.
    host_frame_trace::Session frame_trace(paced_world.scene_mode, paced_world.frame_pause,
                                          app.options.root / "reports");
    std::exception_ptr primary, previous_window_error, cleanup_error, abandonment_error;
    std::optional<retail::SourceApplicationEntryResult> ordinary;
    const auto log = app.options.root / "reports/source-menu-session.log";
    const auto retire_callbacks = [&] {
        app.retire_source_callbacks_for_final_shutdown();
        // At this terminal outer boundary no input/render callback can be
        // reentered. Preserve the actual exception before making room for
        // a distinct failure during DestroyWindow's source WM_DESTROY.
        if (app.window_failure)
        {
            if (!primary)
                primary = app.window_failure;
            else if (app.window_failure != primary && !previous_window_error)
                previous_window_error = app.window_failure;
            app.window_failure = {};
        }
    };
    // Every borrower above outlives Composition; Core outlives all of them.
    retail::SourceApplicationMenuComposition composition(
        core, crt, remaining, remaining, files, data, save, remaining,
        retail::SourceApplicationHostCallbackState{app.window_callback_depth, app.source_callbacks_retired},
        [&](retail::SourceApplicationMenuServices &application, retail::SourceMainMenuChildren &navigation) {
            auto words = core.window_words();
            app.bind_source_menu_window(application, navigation, words.active_61c374, words.mode_61c364);
        },
        retire_callbacks);
    try
    {
        // NEW HOST (R24): a fullscreen window (already sized by attach) is shown
        // normal, never maximized to the work area below the taskbar.
        const int show = app.display_modes.fullscreen_style() && arguments.show_command == SW_SHOWMAXIMIZED
                             ? SW_SHOWNORMAL
                             : arguments.show_command;
        ShowWindow(app.window, show);
        UpdateWindow(app.window);
        app.rethrow_window_failure();
        ordinary = composition.run(arguments, messages);
    }
    catch (...)
    {
        const auto current = std::current_exception();
        if (!primary)
            primary = current;
        else if (primary != current && !previous_window_error)
            previous_window_error = current;
    }

    // One orderly terminal route for every entry outcome (R22 G3): after a
    // failure following 442120 this runs the full 442180 ladder once, like the
    // original fatal 4234D0 before exit(1); before 442120 (or after an already
    // interrupted 442180) it closes only the inert adapters. Neither route is
    // an error, so `primary` stays the only reported failure. An exception
    // here is a genuine, separate cleanup failure: logged below, never
    // swallowed and never replacing `primary`.
    try
    {
        composition.finish_host_lifetime();
    }
    catch (...)
    {
        cleanup_error = std::current_exception();
    }
    // Preserve a distinct nested DestroyWindow failure before abandonment.
    const auto cleanup_window_error = app.window_failure;
    if (cleanup_error)
    {
        try
        {
            // finish can reject before entry, before it reaches its hook.
            // No native cleanup follows here, so keep its latch unchanged.
            app.retire_source_callbacks_for_final_shutdown();
            composition.abandon_host_menu_lifetime();
        }
        catch (...)
        {
            abandonment_error = std::current_exception();
        }
    }
    record_source_menu_failure(log, "source entry", primary);
    record_source_menu_failure(log, "earlier native callback", previous_window_error);
    record_source_menu_failure(log, "cleanup", cleanup_error);
    record_source_menu_failure(log, "cleanup native callback", cleanup_window_error);
    record_source_menu_failure(log, "terminal adapter retirement", abandonment_error);

    if (primary)
    {
        try
        {
            std::rethrow_exception(primary);
        }
        catch (const retail::SourceHostProcessExit &exit)
        {
            if (previous_window_error || cleanup_error || cleanup_window_error || abandonment_error)
                throw FormatError("Source menu exit had an incomplete cleanup; see reports/source-menu-session.log");
            return std::bit_cast<std::int32_t>(exit.status);
        }
    }
    if (previous_window_error)
        std::rethrow_exception(previous_window_error);
    if (cleanup_error)
        std::rethrow_exception(cleanup_error);
    if (cleanup_window_error)
        std::rethrow_exception(cleanup_window_error);
    if (abandonment_error)
        std::rethrow_exception(abandonment_error);
    if (!ordinary)
        throw FormatError("Source menu ended without a retained result");
    if (!ordinary->source_return_word)
    {
        // Original MSG.wParam is uninitialized on this path. Exit0 is a NEW
        // process policy only; it is not published back into source state.
        std::filesystem::create_directories(log.parent_path());
        std::ofstream(log, std::ios::app) << "No initialized source return word; host process policy exit=0\n";
        return 0;
    }
    return std::bit_cast<std::int32_t>(*ordinary->source_return_word);
}

class SourceMenuStartupFailure final : public retail::SourceDrawStartupFailure
{
    std::string description_;

  public:
    const char *describe_error_422690(std::uint32_t code) override
    {
        // NEW D3D12 host diagnosis; not a reconstructed DirectDraw error table.
        std::ostringstream text;
        text << "D3D12 source menu startup error 0x" << std::hex << code;
        description_ = text.str();
        return description_.c_str();
    }
    [[noreturn]] void exit_44e4a0(const char *description) override
    {
        throw FormatError(description ? description : "D3D12 source menu startup failed");
    }
};

std::optional<std::string> source_menu_tz_snapshot()
{
    char *value = nullptr;
    std::size_t length{};
    const auto error = _dupenv_s(&value, &length, "TZ");
    const std::unique_ptr<char, decltype(&std::free)> owned(value, &std::free);
    if (error != 0)
        throw FormatError("Cannot capture the host TZ environment");
    if (!value)
        return std::nullopt;
    return std::string(value, length ? length - 1u : 0u);
}

std::string source_menu_command_line(const wchar_t *command_line)
{
    if (!command_line)
        return {};
    BOOL substituted = FALSE;
    const bool utf8_acp = GetACP() == CP_UTF8;
    const DWORD flags = utf8_acp ? WC_ERR_INVALID_CHARS : WC_NO_BEST_FIT_CHARS;
    const auto replacement = utf8_acp ? nullptr : &substituted;
    const auto length = WideCharToMultiByte(CP_ACP, flags, command_line, -1, nullptr, 0, nullptr, replacement);
    if (!length || substituted)
        throw FormatError("Source menu command line cannot be represented by the host ANSI code page");
    std::string result(static_cast<std::size_t>(length), '\0');
    if (!WideCharToMultiByte(CP_ACP, flags, command_line, -1, result.data(), length, nullptr, replacement) ||
        substituted)
        throw FormatError("Cannot convert the source menu command line");
    result.pop_back();
    return result;
}

struct WindowLifetime
{
    App &app;
    ~WindowLifetime() noexcept
    {
        app.window_tearing_down = true;
        app.detach_source_menu_window();
        const HWND window = app.window;
        app.window = nullptr;
        if (window)
            SetWindowLongPtrW(window, GWLP_USERDATA, 0);
        app.race_application.reset();        // borrowers die before Renderer and HWND
        app.source_application_core.reset(); // source GPU/input borrowers before Renderer
        app.renderer.reset();                // NEW HOST (R24): its destructor leaves exclusive fullscreen first
        app.display_modes.detach();
        if (window)
            DestroyWindow(window);
    }
};

// New host association only. CreateWindowEx sends WM_GETMINMAXINFO before
// WM_NCCREATE, so lpCreateParams alone is too late for that synchronous call.
// This scoped TLS pointer is not a game owner or a second input/word bank.
struct WindowCreationScope;
thread_local WindowCreationScope *pending_window_creation{};
struct WindowCreationScope
{
    App &app;
    ATOM class_atom;
    WindowCreationScope *previous;
    WindowCreationScope(App &target, ATOM atom) noexcept
        : app(target), class_atom(atom), previous(pending_window_creation)
    {
        pending_window_creation = this;
    }
    ~WindowCreationScope() noexcept
    {
        pending_window_creation = previous;
    }
    WindowCreationScope(const WindowCreationScope &) = delete;
    WindowCreationScope &operator=(const WindowCreationScope &) = delete;
    bool matches(HWND window) const noexcept
    {
        return (!app.window || app.window == window) &&
               static_cast<ATOM>(GetClassLongPtrW(window, GCW_ATOM)) == class_atom;
    }
};
LRESULT CALLBACK window_proc(HWND window, UINT msg, WPARAM wp, LPARAM lp) noexcept
{
    App *app = reinterpret_cast<App *>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (msg == WM_NCDESTROY)
    {
        // Use the callback's HWND, never a slot which a nested call may clear.
        SetWindowLongPtrW(window, GWLP_USERDATA, 0);
        if (app && app->window == window)
        {
            app->window = nullptr;
            app->running = false;
            app->detach_source_menu_window(); // retire stored callbacks, keep owners alive
        }
        return DefWindowProcA(window, msg, wp, lp);
    }
    try
    {
        if (msg == WM_NCCREATE)
        {
            if (!lp)
                return FALSE;
            app = static_cast<App *>(reinterpret_cast<CREATESTRUCTA *>(lp)->lpCreateParams);
            if (!app)
                return FALSE;
            if (app->window && app->window != window)
                throw std::logic_error("Native App already belongs to another HWND");
            app->window = window;
            SetLastError(ERROR_SUCCESS);
            const auto previous = SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
            if (previous == 0 && GetLastError() != ERROR_SUCCESS)
                throw std::runtime_error("Cannot associate native App with HWND");
        }
        else if (!app && msg == WM_GETMINMAXINFO && pending_window_creation && pending_window_creation->matches(window))
        {
            app = &pending_window_creation->app;
            app->window = window;
        }
        if (!app || app->window_tearing_down)
            return DefWindowProcA(window, msg, wp, lp);
        if (!app->window_failure)
        {
            const auto result = app->message(window, msg, wp, lp);
            if (!app->window_failure)
                return result;
        }
    }
    catch (...)
    {
        // std::exception is not exhaustive, and string assignment in a catch
        // can throw again. current_exception/exception_ptr assignment cannot.
        if (app)
            app->capture_window_failure();
    }
    if (msg == WM_NCCREATE)
    {
        SetWindowLongPtrW(window, GWLP_USERDATA, 0);
        if (app && app->window == window)
            app->window = nullptr;
        return FALSE;
    }
    if (msg == WM_CREATE)
        return -1;
    return 0;
}
} // namespace
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous_instance, PWSTR command_line, int show_command)
{
    try
    {
        // NEW HOST (R30): game-thread and process scheduling for the whole run (1 ms timer,
        // power throttling off, above-normal priority, MMCSS "Games"); SGN_SCHED=0 disables it.
        sgn::retail::SourceHostScheduling scheduling;
        SourceMenuStartupFailure menu_startup_failure; // outlives App/Core
        App app(arguments());
        WNDCLASSEXA wc{sizeof(WNDCLASSEXA)};
        wc.lpfnWndProc = window_proc;
        wc.hInstance = instance;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = "StuntGP_NEW_D3D12";
        const ATOM window_class = RegisterClassExA(&wc);
        if (!window_class)
            throw std::runtime_error("RegisterClassEx failed");
        RECT rect{0, 0, static_cast<LONG>(app.options.width), static_cast<LONG>(app.options.height)};
        AdjustWindowRectExForDpi(&rect, WS_OVERLAPPEDWINDOW, FALSE, 0, GetDpiForSystem());
        WindowLifetime window_lifetime{app};
        if (app.options.source_menu)
        {
            // NEW host capture capacity, identical to the current renderer budget.
            // It does not set a source tick rate or display mode.
            constexpr PolygonSubmissionCaptureLimits limits{{{20000, 32u * 1024u * 1024u}, 30000}, 16};
            configure_source_menu_graphics(app.options); // NEW HOST (R23), before Core stages the renderer options
            app.prepare_source_core_before_window(instance, limits, menu_startup_failure, source_menu_tz_snapshot());
        }
        HWND window{};
        {
            WindowCreationScope creating{app, window_class};
            window =
                CreateWindowExA(0, wc.lpszClassName, "StuntGP NEW", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                                rect.right - rect.left, rect.bottom - rect.top, nullptr, nullptr, instance, &app);
        }
        if (!window)
            app.window = nullptr;
        app.rethrow_window_failure(); // now outside all synchronous creation callbacks
        if (!window)
            throw std::runtime_error("CreateWindow failed");
        if (app.window != window)
            throw std::runtime_error("Native HWND was destroyed during creation callbacks");
        // NEW HOST (R24): the configured display mode before the first ShowWindow
        // (settings are loaded above for the game path; defaults otherwise).
        app.display_modes.attach(window);
        app.rethrow_window_failure(); // style/placement changes dispatch synchronous callbacks
        const auto source_command = app.options.source_menu ? source_menu_command_line(command_line) : std::string{};
        const auto result = app.options.source_menu
                                ? run_source_menu(app, {reinterpret_cast<std::uintptr_t>(instance),
                                                        reinterpret_cast<std::uintptr_t>(previous_instance),
                                                        show_command, source_command.c_str()})
                                : app.run();
        app.rethrow_window_failure();
        return result;
    }
    catch (const std::exception &e)
    {
        unsigned live_windows = 0;
        if (!EnumThreadWindows(
                GetCurrentThreadId(),
                [](HWND handle, LPARAM context) -> BOOL {
                    wchar_t name[64]{};
                    GetClassNameW(handle, name, 64);
                    if (lstrcmpW(name, L"StuntGP_NEW_D3D12") == 0)
                        ++*reinterpret_cast<unsigned *>(context);
                    return TRUE;
                },
                reinterpret_cast<LPARAM>(&live_windows)))
            live_windows = ~0u;
        std::ofstream("stuntgp-new-error.log", std::ios::app) << e.what() << '\n'
                                                              << "live_application_windows=" << live_windows << '\n';
        // Automated checks must terminate rather than block on a modal dialog.
        if (std::wstring(GetCommandLineW()).find(L"--smoke") == std::wstring::npos &&
            std::wstring(GetCommandLineW()).find(L"--drive-smoke") == std::wstring::npos)
            MessageBoxA(nullptr, e.what(), "StuntGP NEW - startup failed", MB_OK | MB_ICONERROR);
        return 1;
    }
    catch (...)
    {
        std::ofstream("stuntgp-new-error.log", std::ios::app) << "Non-standard C++ exception in native application\n";
        return 1;
    }
}
