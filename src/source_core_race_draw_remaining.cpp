#include "sgn/source_core_race_draw_remaining.hpp"
#include "sgn/source_application_core.hpp"
#include "sgn/source_frontend_material_binding.hpp"
#include "sgn/source_menu_sample_native.hpp"
#include "sgn/source_native_crt.hpp"
#include "sgn/source_normal_race_initialize.hpp"
#include "sgn/retail_demo_tail.hpp"
#include "sgn/retail_race_hud.hpp"
#include "sgn/retail_frontend_state.hpp"
#include "sgn/retail_scene_transition.hpp"
#include "sgn/race_session_scene.hpp"
#include "sgn/race_session_lifecycle.hpp"
#include "sgn/vehicle_stunt_awards.hpp"
#include "sgn/race_session.hpp"
#include <bit>

// x64 MASM keeping the original x87 sequence40DDEA..40DE2F (velocity/offset).
extern "C" void sgn_source_demo_motion_40ddea(float *velocity_473a28, float *offset_4d5b18);

namespace sgn::retail
{
namespace
{
using W = std::uint32_t;
template <class T> T read(const T &value)
{
    return static_cast<const volatile T &>(value);
}
template <class T> void put(T &target, T value)
{
    static_cast<volatile T &>(target) = value;
}
std::shared_ptr<RaceSessionPlatform> admit_frontend(FrontendState &frontend, W site)
{
    const auto platform = frontend.shared_platform();
    if (!platform || !platform->source_construction_only_entry() || &frontend.world() != &platform->state())
        throw SourceAllocationError(site, "source demo/pause/results require the retained source platform world");
    platform->require_owner_thread();
    return platform;
}
void admit(SourceCoreRaceDrawContext &c, W site)
{
    (void)admit_frontend(c.frontend, site);
    c.frames.require_source_frontend(c.frontend);
}
LoadingFrameWorld loading(SourceCoreRaceDrawContext &c)
{
    auto frame = c.frames.borrow_loading_frame();
    auto &s = c.frontend.world();
    if (&frame.submission != &c.submission || frame.quads.descriptor.data() != s.ui.descriptor.data() ||
        frame.quads.vertices.data() != c.submission.alpha_vertices.data() ||
        &frame.quads.selected_material != &s.ui.selected_material || &frame.quads.viewport != &s.viewport)
        throw FormatError("source demo borrows foreign frame/scratch owners");
    return frame;
}
// Invocation-only reference view of the same Core (from the reviewed
// demo-tail package). Each borrow is fresh; source writes stay in the world.
class DemoSources final : public DemoTailDrawingSources, public RaceTextureReplacement
{
    SourceCoreRaceDrawContext &c_;
    SourceFrontendMaterialBinding &materials_;
    void same_material_owner() const
    {
        // The binding validates its own epoch/backend/fonts; pointer equality
        // then excludes a binding that belongs to another world.
        if (&materials_.fonts() != &c_.frontend.world().ui.fonts)
            throw FormatError("source demo material binding belongs to another world");
    }

  public:
    DemoSources(SourceCoreRaceDrawContext &c, SourceFrontendMaterialBinding &materials) : c_(c), materials_(materials)
    {
    }
    TextSubmission borrow_text() override
    {
        same_material_owner();
        const auto f = loading(c_);
        auto &s = c_.frontend.world();
        return {s.ui.fonts,        s.ui.selected_material, c_.submission.specular_enabled,
                f.quads.materials, s.view_quality.second,  s.view_quality.legacy_driver_restriction,
                s.viewport};
    }
    QuadSubmission borrow_quad() override
    {
        same_material_owner();
        const auto f = loading(c_);
        return {f.quads.descriptor, f.quads.vertices, f.quads.selected_material, f.quads.materials, f.quads.viewport};
    }
    std::uint32_t selected_overlay_texture() const override
    {
        same_material_owner();
        return materials_.textures().selected_overlay_texture();
    }
    void replace_source_texture(W destination, W source) override
    {
        same_material_owner();
        materials_.textures().replace_source_texture(destination, source); // full430D40
    }
};
// 40DD30..40DD79. 42F1F0(1024) is the same persistent463289 stream (s.random).
void select_demo_vehicle_40dd30(RaceSessionState &s)
{
    W esi = 0; // 40DD31
    do
    {
        const auto draw = vehicle_random_integer(s.random, 0x400); // 40DD33..40DD38
        const auto remainder = draw % 6; // 40DD3D CDQ;IDIV: truncating, remainder keeps the dividend sign
        // NEW repair (PORT-FIX proposal): 40DD45..40DD58 reads
        // [49BDA0+remainder*7724] also for remainder -5..-1, i.e. 49467C,
        // 48CF58, 485834, 47E110, 4769EC: foreign .data (floats/strings, PE
        // bytes never 00000001), not vehicle records 49A1F8+i*7724. Treat
        // those draws as "not active" (the value the original read yields for
        // the pinned image) without forming the foreign address. The RNG stream
        // and loop count are unchanged; only a stray publication at40DD6C of a
        // non-vehicle pointer is excluded.
        if (remainder >= 0)
        {
            const auto slot = static_cast<W>(remainder);
            if (read(s.vehicles.race[slot].active) == 1u)
            {                                                                        // 40DD58 CMP [ +1BA8],1
                put(s.load_selection.template_vehicle, static_cast<unsigned>(slot)); // 40DD61/40DD6C ->474630
                esi = 0x10;                                                          // 40DD67
            }
        }
        ++esi; // 40DD72
    } while (esi < 0x10u); // 40DD73 unsigned JC
}
// 40DD80..40DDC1.
void reset_demo_motion_40dd80(RaceSessionState &s)
{
    if (read(s.outer.source_word_612994) == 1u)
        return;                                                    // 40DD80..40DD8D
    put(s.outer.source_word_612994, 1u);                           // 40DD8F
    put(s.outer.demo.age, 0x2d0u);                                 // 40DD94 ->473A24
    put(s.outer.demo.offset, 0.0f);                                // 40DD9E ->4D5B18 (00000000)
    put(s.outer.demo.velocity, std::bit_cast<float>(0xc0a00000u)); // 40DDA8 ->473A28
    put(s.outer.demo.initialized, 0u);                             // 40DDB2 ->4D5B1C
    select_demo_vehicle_40dd30(s);                                 // 40DDBC tail JMP
}
// 447B60: [61B740]=0, i.e. the selected HUD's countdown-active word (447B50 stores1).
void disable_selected_race_hud_447b60(RaceSessionState &s)
{
    const auto hud = read(s.selected_hud);
    if (hud >= s.hud_countdown.size())
        throw SourceAllocationError(0x447b65, "demo selected HUD outside source owner");
    put(s.hud_countdown[hud].active, 0u);
}
// 40DDD0..40DE2F.
void advance_demo_40ddd0(RaceSessionState &s)
{
    reset_demo_motion_40dd80(s);             // 40DDD0
    disable_selected_race_hud_447b60(s);     // 40DDD5
    const auto age = read(s.outer.demo.age); // 40DDDA
    if (std::bit_cast<std::int32_t>(age) > 0)
    {
        put(s.outer.demo.age, age - 1u);
        return;
    } // 40DDDF..40DDE9 signed JLE
    sgn_source_demo_motion_40ddea(&s.outer.demo.velocity, &s.outer.demo.offset); // 40DDEA..40DE2F x87
}
} // namespace
void submit_source_core_demo_40e030(SourceCoreRaceDrawContext &c, SourceFrontendMaterialBinding &materials)
{
    admit(c, 0x40e030);
    auto &s = c.frontend.world();
    if (read(s.hud_block) != 1u)
        return; // 40E030/40E037 ->40E043, no resource borrow
    auto words = c.frames.borrow_demo_words();
    if (&words.age != &s.outer.demo.age || &words.offset != &s.outer.demo.offset ||
        &words.initialized != &s.outer.demo.initialized)
        throw FormatError("source demo words belong to another retained world");
    DemoSources drawing(c, materials);
    DemoTailWorld world{s.hud_block,     s.demo_ticks,
                        words,           {s.controls, s.hud_countdown, s.hud_layout, s.selected_hud, s.hud_margins},
                        s.game_text,     drawing,
                        s.ui.descriptor, drawing,
                        c.sink};
    submit_demo_graphics_40de80(world); // 40E039, real uploads and448110 tail
    submit_demo_text_40df90(world);     // 40E03E tail; no second gate after callbacks
}
void update_source_demo_40de30(FrontendState &frontend)
{
    const auto platform = admit_frontend(frontend, 0x40de30);
    auto &s = platform->state();
    if (read(s.hud_block) != 1u)
        return;                      // 40DE30/40DE37 ->40DE74
    put(s.startup.field_60f2c0, 1u); // 40DE39 ->60F2C0
    advance_demo_40ddd0(s);          // 40DE43
    const auto ticks = read(s.demo_ticks) - 1u;
    put(s.demo_ticks, ticks); // 40DE48..40DE4E ->612990
    if (ticks != 0u && read(s.normal_finish.confirm_pressed) != 1u)
        return;                          // 40DE53 JZ /40DE55..40DE5C ->612968
    put(s.outer.source_word_612994, 0u); // 40DE60
    put(s.hud_block, 0u);                // 40DE65 ->61299C
    put(s.demo_ticks, 0u);               // 40DE6A
    auto transition = scene_transition_world(s);
    auto results = scene_race_result_world(s);
    request_scene_transition(transition, results); // 40DE6F tail JMP441E20 (includes445530)
}
SourceCoreRaceDrawRemaining::SourceCoreRaceDrawRemaining(SourceApplicationCore &core, NativeSourceMenuCrt &crt)
    : core_(core), crt_(crt)
{
}
void SourceCoreRaceDrawRemaining::same(FrontendState &frontend, W site, W entry) const
{
    if (&frontend != &core_.frontend())
        throw SourceCoreRaceDrawBoundary(site, entry, "remaining race draw child received another Core frontend");
    core_.platform()->require_owner_thread();
}
void SourceCoreRaceDrawRemaining::restart_40d500(FrontendState &frontend, W site)
{
    same(frontend, site, 0x40d500);
    crt_.require_platform(*core_.platform());
    // 45F9D0 already returned in446AF0. Same file/CRT/native owners as the
    // Composition's initialize_race; the source restarts synchronously in draw.
    restart_source_normal_race_446af5(frontend, core_.files(), crt_, core_.native_window_handles());
}
void SourceCoreRaceDrawRemaining::camera_preset(FrontendState &frontend, W site, W entry, W captured_vehicle)
{
    same(frontend, site, entry);
    apply_source_pause_camera_preset(frontend, entry, captured_vehicle);
}
void SourceCoreRaceDrawRemaining::perform(SourceCoreRaceDrawChild child, W site, SourceCoreRaceDrawContext &c)
{
    same(c.frontend, site, static_cast<W>(child));
    admit(c, site);
    switch (child)
    {
    case SourceCoreRaceDrawChild::pause_menu:
        // 44277E. 4473F0 owns its gates/zeroing and continues after callbacks.
        // Adapter acquisition is inert; texture use revalidates its epoch.
        submit_source_pause_menu_4473f0(c, core_.source_race_materials().textures(), core_.source_samples(), *this);
        return;
    case SourceCoreRaceDrawChild::results:
        // 442783. 449FB0 executes its76A4/60D728/76B4 prefix exactly once.
        submit_source_results_449fb0(c, core_.source_race_materials().textures());
        return;
    case SourceCoreRaceDrawChild::demo:
        // 44278D. Inactive exact gate precedes the material adapter borrow.
        if (read(c.frontend.world().hud_block) != 1u)
            return; // 40E030..40E037 ->40E043
        submit_source_core_demo_40e030(c, core_.source_race_materials());
        return;
    default:
        break;
    }
    throw SourceCoreRaceDrawBoundary(site, static_cast<W>(child),
                                     "Core handles this race draw child; it is not a remaining receiver child");
}
} // namespace sgn::retail
