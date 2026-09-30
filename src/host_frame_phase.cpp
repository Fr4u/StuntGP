#include "sgn/host_frame_phase.hpp"
#include "sgn/host_graphics_settings.hpp"
#include "sgn/polygon_d3d12.hpp"
#include <array>

// NEW HOST (R28) frame phase markers; see the header. Host bookkeeping only:
// no source owner is read or written here.
namespace sgn::host_frame_phase
{
namespace
{
class PhaseCommand final : public PolygonGpuCommand
{
    Marker marker_;

  public:
    explicit PhaseCommand(Marker marker) noexcept : marker_(marker) {}
    Marker marker() const noexcept
    {
        return marker_;
    }
    void record(const PolygonGpuRecordContext &context) const override
    {
        if (auto *recorder = active_recorder())
            recorder->record_marker(marker_, context);
    }
};
struct Capture
{
    void *owner{};
    Inserter insert{};
    bool wanted{};
    bool settings_known{};
    std::uint64_t settings_revision{};
    bool settings_wanted{};
    MarkerRecorder *recorder{};
    // One immutable command per marker kind, shared by every frame.
    std::array<std::shared_ptr<const PhaseCommand>, 4> commands;
};
Capture &capture() noexcept
{
    static Capture state; // owner thread only
    return state;
}
bool post_effects_wanted(Capture &c) noexcept
{
    const auto revision = host_graphics::revision();
    if (!c.settings_known || revision != c.settings_revision)
    {
        const auto settings = host_graphics::current();
        c.settings_revision = revision;
        c.settings_known = true;
        c.settings_wanted = settings.bloom || settings.post_anti_aliasing == host_graphics::PostAntiAliasing::fxaa;
    }
    return c.settings_wanted;
}
void insert(Capture &c, Marker marker) noexcept
{
    try
    {
        auto &command = c.commands[static_cast<std::size_t>(marker)];
        if (!command)
            command = std::make_shared<const PhaseCommand>(marker);
        c.insert(c.owner, command);
    }
    catch (...)
    {
        // No marker: the renderer finds no complete split and keeps the single pass.
    }
}
} // namespace

void begin_capture(void *owner, Inserter inserter) noexcept
{
    auto &c = capture();
    c.owner = owner;
    c.insert = inserter;
    c.wanted = owner && inserter && post_effects_wanted(c);
}
void end_capture() noexcept
{
    auto &c = capture();
    c.owner = nullptr;
    c.insert = nullptr;
    c.wanted = false;
}
void region_changed(host_presentation::Region from, host_presentation::Region to) noexcept
{
    auto &c = capture();
    if (!c.wanted || from == to)
        return;
    using host_presentation::Region;
    if (from == Region::world3d)
        insert(c, Marker::world_end);
    else if (from == Region::screen3d)
        insert(c, Marker::screen_end);
    if (to == Region::world3d)
        insert(c, Marker::world_begin);
    else if (to == Region::screen3d)
        insert(c, Marker::screen_begin);
}

std::optional<Marker> marker_of(const PolygonGpuInsertion &insertion) noexcept
{
    if (insertion.draw)
        return std::nullopt;
    if (const auto *phase = dynamic_cast<const PhaseCommand *>(insertion.command.get()))
        return phase->marker();
    return std::nullopt;
}
bool has_foreign_insertions(const CapturedPolygonFrame &frame) noexcept
{
    for (const auto &insertion : frame.gpu_insertions)
        if (!marker_of(insertion))
            return true;
    return false;
}

void set_active_recorder(MarkerRecorder *recorder) noexcept
{
    capture().recorder = recorder;
}
MarkerRecorder *active_recorder() noexcept
{
    return capture().recorder;
}
} // namespace sgn::host_frame_phase
