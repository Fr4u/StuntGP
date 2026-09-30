#include "sgn/source_scene_crt_power.hpp"
#include "sgn/source_native_crt.hpp"
#include "sgn/retail_frontend_state.hpp"
#include "sgn/race_session.hpp"
#include <cstddef>
namespace sgn::retail
{
namespace
{
struct PowerRun
{
    const SourceScenePowerArguments *input;
    const std::uint32_t *actual_bypass_636528;
    SourceSceneExtended80 output;
    std::uint16_t source_status;
    std::uint32_t source_frontier, required_child;
    std::uint16_t source_saved_control, source_temporary_control;
    std::uint32_t result_classification;
    std::uint64_t argument_base, argument_exponent, result_spill;
    // Actual4661D0 locals: _exception{type,name,arg1,arg2,retval}32 bytes.
    // Published only on a reached46620B boundary, never zero-filled as a result.
    std::array<std::uint32_t, 8> error_record;
};
static_assert(offsetof(PowerRun, output) == 16 && offsetof(PowerRun, source_status) == 26);
static_assert(offsetof(PowerRun, source_frontier) == 28 && offsetof(PowerRun, required_child) == 32);
static_assert(offsetof(PowerRun, source_saved_control) == 36 && offsetof(PowerRun, source_temporary_control) == 38);
static_assert(offsetof(PowerRun, result_classification) == 40 && offsetof(PowerRun, argument_base) == 48);
static_assert(offsetof(PowerRun, argument_exponent) == 56 && offsetof(PowerRun, result_spill) == 64);
static_assert(offsetof(PowerRun, error_record) == 72);
extern "C" std::uint32_t sgn_scene_crt_power_462bb0(PowerRun *);
} // namespace
SourceSceneExtended80 SourceSceneCrtPower::power_462bb0(FrontendState &f, NativeSourceMenuCrt &crt,
                                                        std::uint32_t callsite, const SourceScenePowerArguments &input)
{
    switch (callsite)
    {
    case 0x4223a1:
    case 0x4223bd:
    case 0x422446:
    case 0x422462:
    case 0x42249e:
    case 0x4224ba:
        break;
    default:
        throw SourceSceneSmallBoundary(callsite, 0x462bb0, "unbound color-table source POW callsite");
    }
    const auto p = f.shared_platform();
    if (!p || !p->source_construction_only_entry() || &p->state() != &f.world())
        throw FormatError("source POW requires actual Core frontend/platform");
    p->require_owner_thread();
    crt.require_platform(*p);
    // Same pre-existing process storage as source4650D5. Borrow, never snapshot
    // or reset636528. This method also enforces completed CRT startup.
    auto &storage = crt.fraction_storage_46291a();
    PowerRun run{&input, &storage.bypass_636528};
    const auto result = sgn_scene_crt_power_462bb0(&run);
    if (result != 0)
    {
        const char *reason =
            result == 1   ? "POW Win64 transport requires empty physical x87 stack and captured two-operand TOP6"
            : result == 2 ? "source POW reached an unimplemented special exponent/base or zero/negative-base path"
            : result == 3
                ? "source POW result reached actual underflow/overflow classification outside normal result path"
                : "source POW precision exception requires actual46B6D5 CRT/matherr/Windows exception continuation";
        throw SourceSceneSmallBoundary(run.source_frontier, run.required_child, reason);
    }
    return run.output;
}
bool perform_source_race_scene_small_child_with_crt_power(SceneOperation op, FrontendState &f,
                                                          SourceBootFilePlatform &files, NativeSourceMenuCrt &crt)
{
    SourceSceneCrtPower required;
    return perform_source_race_scene_small_child(op, f, files, crt, required);
}
} // namespace sgn::retail
