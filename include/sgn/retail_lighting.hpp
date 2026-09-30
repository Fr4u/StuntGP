#pragma once
#include "sgn/retail_camera_initialization.hpp"
#include "sgn/vehicle_shadow.hpp"
#include "sgn/source_scene_static_pointer.hpp"
#include "sgn/source_scene_allocation_pointer.hpp"
#include <variant>
#include "sgn/source_light_selection.hpp"
#include <vector>

namespace sgn::retail
{
struct RetailLightRecord
{
    std::uint32_t active{}, kind{};
    float strength{};
    std::uint32_t opaque_0c{};
    contact::Vector4 position{}, relative{};
    std::array<std::uint32_t, 8> opaque_30_4c{};
    std::uint32_t type{};
    std::array<std::uint32_t, 5> opaque_54_64{};
    std::array<float, 3> color{};
    std::uint32_t opaque_74{};
    std::array<std::uint32_t, 3> scaled_color{};
    std::uint32_t visible{};
    std::array<std::uint32_t, 2> opaque_88_8c{};
};
static_assert(sizeof(RetailLightRecord) == 0x90);
static_assert(offsetof(RetailLightRecord, position) == 0x10 && offsetof(RetailLightRecord, type) == 0x50);
static_assert(offsetof(RetailLightRecord, color) == 0x68 && offsetof(RetailLightRecord, visible) == 0x84);
struct RetailAmbientLight
{
    float strength{};
    std::array<float, 3> color{}, scaled{};
    std::array<std::uint32_t, 3> packed{};
};
static_assert(sizeof(RetailAmbientLight) == 40);
struct PmdLightBank
{
    std::vector<RetailLightRecord> records;
    std::uint32_t render_count{}; // 6244A0; preview records.size() is62449C; source cold count belongs to Models
    RetailAmbientLight authored_ambient{}; // preview PMD ambient; cold sole static5898D0..5898F7
    // One pointer representation: preview index, static menu alias or checked
    // actual PMD allocation pointer. The source selection never copies records.
    std::variant<std::optional<std::uint32_t>, SourceSceneStaticPointer, SourceSceneAllocationPointer>
        selected;                                 // sole589E04
    RetailLightRecord source_menu_light_589628{}; // sole589628..5896B7, pinned PE zero; no inferred light count
};
struct LightViewSelection
{
    std::uint32_t active_count{};  // 62452C
    LightSelectionStorage ordered; // 624534, unused tail retained
};
struct LightFadeState
{
    float target{1}, current{1}, blend{}, last_gamma{}; // 477B80/477B84/589E00/6244F4, pinned PE initial values
    std::array<std::uint16_t, 768> ramp{};              // 592180, three source channels
};
// Controller+1E8 is owned by ControllerOverlayTransition.active. Lighting
// borrows that existing controller; it never stores a second copy of the flag.
struct ControllerOverlayTransition;
struct ViewMatrixArena;
struct LightCameraView
{
    const CameraState &camera;
    const CameraLifecycle &lifecycle;
    const CameraPresentationState &presentation;
    const ControllerOverlayTransition &overlay;
    // Optional invocation borrows required by source434590. They resolve the
    // current camera+50, never substitute its cached rotation value.
    const ViewMatrixArena *source_matrices{};
    const CameraMatrix *source_identity{};
};
// Mandatory device boundary when the original gamma device is present. The
// platform adapter consumes the source ramp; there is no default silent sink.
struct LightGammaDevice
{
    virtual ~LightGammaDevice() = default;
    virtual void apply(std::span<const std::uint16_t, 768>) = 0;
};
PmdLightBank decode_pmd_lights(std::span<const std::uint8_t>);    // 42EEE0, bounded host parser
void select_shadow_light(PmdLightBank &);                         // 42D970, source order, no visibility test
void initialize_pmd_lights(PmdLightBank &, RetailAmbientLight &); // 42D700 scene!=2 branch +42D7E0+42D970
void update_ambient_light(RetailAmbientLight &); // complete42D7E0, also written by446840 level configuration
void set_light_gamma(LightFadeState &, float, LightGammaDevice *); // full4320C0; null represents absent source device
void update_light_fade(LightFadeState &, const ControllerOverlayTransition &, std::uint32_t split_setting,
                       LightGammaDevice *); // full42D870
void prepare_light_view(PmdLightBank &, LightViewSelection &, LightFadeState &, LightCameraView,
                        std::uint32_t split_setting, LightGammaDevice *); // full42D9B0
std::optional<VehicleShadowLighting> shadow_lighting(const PmdLightBank &, const LightViewSelection &);
} // namespace sgn::retail
