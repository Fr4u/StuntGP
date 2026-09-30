#include "sgn/polygon_submission.hpp"
#include "sgn/format_require.hpp"
#include <limits>
#include <type_traits>
#include <algorithm>
#include <vector>

namespace sgn
{
namespace
{
using sgn::format_check::require;
void add(std::size_t &value, std::size_t amount)
{
    require(amount <= (std::numeric_limits<std::size_t>::max)() - value, "polygon submission count overflow");
    value += amount;
}
bool same_creation(const std::optional<PolygonTextureCreationEvidence> &a,
                   const std::optional<PolygonTextureCreationEvidence> &b) noexcept
{
    return a.has_value() == b.has_value() &&
           (!a || (a->descriptor_flags == b->descriptor_flags && a->texture_stage == b->texture_stage &&
                   a->transparency_byte == b->transparency_byte));
}
bool same_entry(const PolygonGpuTexture &a, const PolygonGpuTexture &b, bool resource) noexcept
{
    return a.token == b.token && a.format == b.format && same_creation(a.source_creation, b.source_creation) &&
           (!resource || a.resource.Get() == b.resource.Get());
}
void receive_events(const CapturedPolygonDeviceFrame &frame)
{
    auto state = frame.initial;
    std::size_t command_index = 0;
    for (const auto &event : frame.events)
    {
        require(state.operation_sequence != (std::numeric_limits<std::uint64_t>::max)(), "device sequence overflow");
        require(event.sequence == ++state.operation_sequence && event.cpu_accepted && event.hresult == 0 &&
                    event.rejection == PolygonDeviceRejection::none,
                "submission contains refused or discontinuous device events");
        const auto a = event.arguments;
        const bool command = event.operation != PolygonDeviceOperation::begin_scene &&
                             event.operation != PolygonDeviceOperation::end_scene &&
                             event.operation != PolygonDeviceOperation::light_state &&
                             event.operation != PolygonDeviceOperation::transform;
        require(event.operation == PolygonDeviceOperation::transform || !event.transform_value,
                "non-transform event retains transform payload");
        if (command)
        {
            require(event.polygon_command && *event.polygon_command == command_index &&
                        command_index < frame.polygon_commands->commands.size(),
                    "device command/event identity differs");
            const auto &item = frame.polygon_commands->commands[command_index++];
            switch (event.operation)
            {
            case PolygonDeviceOperation::render_state: {
                const auto *c = std::get_if<CapturedRenderState>(&item);
                require(c && c->state == a[0] && c->value == a[1] && a[2] == 0 && a[0] < state.polygon.render.size(),
                        "render event differs");
                state.polygon.render[a[0]] = a[1];
                break;
            }
            case PolygonDeviceOperation::texture: {
                const auto *c = std::get_if<CapturedTexture>(&item);
                require(c && c->stage == a[0] && c->token == a[1] && a[2] == 0 && a[0] < state.polygon.texture.size(),
                        "texture event differs");
                state.polygon.texture[a[0]] = a[1];
                break;
            }
            case PolygonDeviceOperation::texture_stage_state: {
                const auto *c = std::get_if<CapturedTextureStageState>(&item);
                require(c && c->stage == a[0] && c->state == a[1] && c->value == a[2] &&
                            a[0] < state.polygon.stage.size() && a[1] < state.polygon.stage[a[0]].size(),
                        "stage event differs");
                state.polygon.stage[a[0]][a[1]] = a[2];
                break;
            }
            case PolygonDeviceOperation::draw:
            case PolygonDeviceOperation::draw_indexed: {
                const auto *c = std::get_if<CapturedPolygonDraw>(&item);
                require(c && state.scene_open && c->type == a[0] && c->fvf == a[1] && c->flags == a[2] &&
                            c->indexed == (event.operation == PolygonDeviceOperation::draw_indexed),
                        "draw event differs");
                break;
            }
            default:
                throw FormatError("unknown command operation");
            }
        }
        else
        {
            require(!event.polygon_command, "non-polygon event names a command");
            if (event.operation == PolygonDeviceOperation::begin_scene)
            {
                require(!state.scene_open && a == std::array<std::uint32_t, 3>{}, "begin scene differs");
                state.scene_open = true;
            }
            else if (event.operation == PolygonDeviceOperation::end_scene)
            {
                require(state.scene_open && a == std::array<std::uint32_t, 3>{}, "end scene differs");
                state.scene_open = false;
            }
            else if (event.operation == PolygonDeviceOperation::transform)
            {
                require(event.transform_value && a[0] >= 1 && a[0] <= 3, "transform event slot/payload differs");
                const auto &value = *event.transform_value;
                require((a[1] == 1 && a[2] == 0 && std::holds_alternative<std::array<std::uint32_t, 16>>(value)) ||
                            (a[0] == 3 && a[1] == 2 && a[2] == 0x431420 &&
                             std::holds_alternative<PolygonSourceUninitializedTransform>(value)),
                        "transform event provenance differs");
                state.transforms[a[0] - 1] = value;
            }
            else
            {
                require(event.operation == PolygonDeviceOperation::light_state, "unknown non-polygon operation");
                require(a[0] < state.light.size() && a[2] == 0, "light event differs");
                state.light[a[0]] = a[1];
            }
        }
    }
    require(command_index == frame.polygon_commands->commands.size() && same_polygon_snapshot(state, frame.final),
            "device events do not reconstruct the complete final state");
}
} // namespace
bool same_polygon_state(const PolygonDeviceState &a, const PolygonDeviceState &b) noexcept
{
    return a.render == b.render && a.stage == b.stage && a.texture == b.texture;
}
bool same_polygon_snapshot(const PolygonDeviceSnapshot &a, const PolygonDeviceSnapshot &b) noexcept
{
    return same_polygon_state(a.polygon, b.polygon) && a.light == b.light && a.scene_open == b.scene_open &&
           a.operation_sequence == b.operation_sequence && a.transforms == b.transforms;
}
bool same_polygon_bank(const PolygonGpuTextures &a, const PolygonGpuTextures &b) noexcept
{
    if (a.epoch != b.epoch || a.entries.size() != b.entries.size())
        return false;
    for (std::size_t i = 0; i < a.entries.size(); ++i)
        if (!same_entry(a.entries[i], b.entries[i], true))
            return false;
    return true;
}
void validate_polygon_bank(const PolygonGpuTextures &bank)
{
    require(bank.epoch != 0, "polygon bank has zero epoch");
    if (bank.entries.size() <= 32u)
    {
        for (std::size_t i = 0; i < bank.entries.size(); ++i)
        {
            require(bank.entries[i].token != 0 && bank.entries[i].resource.Get() != nullptr,
                    "polygon bank has null resource identity");
            for (std::size_t j = 0; j < i; ++j)
                require(bank.entries[i].token != bank.entries[j].token, "duplicate polygon resource token");
        }
        return;
    }
    // NEW R19 (perf_gpu): same index-order checks and first failure as the
    // quadratic scan above; "equal to an earlier token" is a set membership.
    // NEW HOST (R30 perf_renderer): the set is a reused thread-local open-addressing
    // table (the std::unordered_set made one heap node per entry, per bank, per
    // presented frame). A token enters it only after its nonzero check, so 0
    // marks an empty slot. Same entry order, same checks, same first failure.
    thread_local std::vector<std::uint32_t> earlier;
    std::size_t capacity = 64;
    while (capacity < bank.entries.size() * 2)
        capacity *= 2;
    if (earlier.size() < capacity)
        earlier.resize(capacity);
    std::fill_n(earlier.begin(), capacity, 0u);
    const auto mask = capacity - 1;
    for (const auto &entry : bank.entries)
    {
        require(entry.token != 0 && entry.resource.Get() != nullptr, "polygon bank has null resource identity");
        auto at = static_cast<std::size_t>((entry.token * 0x9E3779B1u) >> 7) & mask;
        for (; earlier[at] != 0; at = (at + 1) & mask)
            require(earlier[at] != entry.token, "duplicate polygon resource token");
        earlier[at] = entry.token;
    }
}
void validate_polygon_replacement(const PolygonGpuTextures &before, const PolygonGpuTextures &after,
                                  std::optional<std::uint32_t> expected)
{
    require(before.epoch != (std::numeric_limits<std::uint64_t>::max)() && after.epoch == before.epoch + 1 &&
                before.entries.size() == after.entries.size(),
            "replacement epoch or resource count differs");
    std::size_t changed = 0;
    for (std::size_t i = 0; i < before.entries.size(); ++i)
    {
        const auto &a = before.entries[i];
        const auto &b = after.entries[i];
        require(same_entry(a, b, false) && b.resource.Get() != nullptr,
                "replacement changed token/format/creation identity");
        if (a.resource.Get() != b.resource.Get())
        {
            require(!expected || a.token == *expected, "replacement changed another primary token");
            ++changed;
        }
    }
    require(changed == 1, "replacement must change exactly one actual resource");
}
PolygonSubmissionWorkload polygon_submission_workload(const CapturedPolygonFrame &frame)
{
    static_assert(sizeof(retail::PolygonSubmissionVertex) == 40);
    PolygonSubmissionWorkload result;
    std::size_t payload = 0;
    for (const auto &command : frame.commands)
        if (const auto *draw = std::get_if<CapturedPolygonDraw>(&command))
        {
            require(draw->indexed || draw->indices.empty(), "nonindexed capture retains unexpected indices");
            require(draw->vertices.size() <=
                            (std::numeric_limits<std::size_t>::max)() / sizeof(retail::PolygonSubmissionVertex) &&
                        draw->indices.size() <= (std::numeric_limits<std::size_t>::max)() / sizeof(std::uint16_t),
                    "draw payload overflow");
            add(payload, draw->vertices.size() * sizeof(retail::PolygonSubmissionVertex));
            add(payload, draw->indices.size() * sizeof(std::uint16_t));
            const auto used = draw->indexed ? draw->indices.size() : draw->vertices.size();
            std::size_t indices = draw->indices.size();
            if (draw->type == 6)
            {
                require(used <= (std::numeric_limits<std::size_t>::max)() / 3, "fan index count overflow");
                indices = used > 2 ? (used - 2) * 3 : 0;
            }
            require(indices <= ((std::numeric_limits<std::size_t>::max)() - 3) / 2, "GPU index extent overflow");
            add(result.upload_bytes, draw->vertices.size() * sizeof(retail::PolygonSubmissionVertex));
            add(result.upload_bytes, (indices * 2 + 3) & ~std::size_t(3));
            add(result.draws, 1);
            require(result.draws <= polygon_submission_draw_limit &&
                        result.upload_bytes <= polygon_submission_upload_limit,
                    "polygon submission exceeds aggregate GPU budget");
        }
    require(payload == frame.payload_bytes, "polygon frame payload count differs");
    return result;
}
PolygonSubmissionWorkload validate_polygon_submission(const CapturedPolygonSubmission &submission)
{
    require(submission.device && !submission.segments.empty(), "submission requires device and segments");
    auto state = submission.initial;
    const PolygonGpuTextures *previous = nullptr;
    std::size_t commands = 0, payload = 0, events = 0;
    PolygonSubmissionWorkload total;
    for (const auto &segment : submission.segments)
    {
        require(segment.device_frame && segment.textures && segment.device_frame->polygon_commands,
                "null submission segment");
        const auto &frame = *segment.device_frame;
        const auto &polygon = *frame.polygon_commands;
        require(frame.device == submission.device && polygon.source_tick == submission.source_tick &&
                    polygon.resource_epoch == segment.textures->epoch && same_polygon_snapshot(state, frame.initial),
                "segment identity/state discontinuity");
        validate_polygon_bank(*segment.textures);
        if (previous)
            validate_polygon_replacement(*previous, *segment.textures);
        receive_events(frame);
        const auto work = polygon_submission_workload(polygon);
        add(total.draws, work.draws);
        add(total.upload_bytes, work.upload_bytes);
        require(total.draws <= polygon_submission_draw_limit && total.upload_bytes <= polygon_submission_upload_limit,
                "segments exceed aggregate GPU budget");
        add(commands, polygon.commands.size());
        add(payload, polygon.payload_bytes);
        add(events, frame.events.size());
        state = frame.final;
        previous = segment.textures.get();
    }
    require(same_polygon_snapshot(state, submission.final) && commands == submission.commands &&
                payload == submission.payload_bytes && events == submission.events,
            "submission final state/count differs");
    return total;
}
} // namespace sgn
