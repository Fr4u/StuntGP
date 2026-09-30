#include "sgn/displayed_recipe_export.hpp"
#include <bit>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace sgn::diagnostics
{
namespace
{
// Fixed-width little-endian fields, never native struct padding or addresses.
class Writer
{
    std::ostream &out_;
    std::array<char, 65536> buffer_{};
    std::size_t used_{};
    bool transforms_{};

  public:
    explicit Writer(std::ostream &out, bool transforms) : out_(out), transforms_(transforms) {}
    void flush()
    {
        out_.write(buffer_.data(), static_cast<std::streamsize>(used_));
        used_ = 0;
        if (!out_)
            throw std::runtime_error("displayed recipe stream write failed");
    }
    void u8(std::uint8_t v)
    {
        if (used_ == buffer_.size())
            flush();
        buffer_[used_++] = std::bit_cast<char>(v);
    }
    void u16(std::uint16_t v)
    {
        for (unsigned i = 0; i < 2; ++i)
            u8(static_cast<std::uint8_t>(v >> (i * 8)));
    }
    void u32(std::uint32_t v)
    {
        for (unsigned i = 0; i < 4; ++i)
            u8(static_cast<std::uint8_t>(v >> (i * 8)));
    }
    void u64(std::uint64_t v)
    {
        for (unsigned i = 0; i < 8; ++i)
            u8(static_cast<std::uint8_t>(v >> (i * 8)));
    }
    void flag(bool v)
    {
        u8(v ? 1u : 0u);
    }
    void optional32(const std::optional<std::uint32_t> &v)
    {
        flag(v.has_value());
        if (v)
            u32(*v);
    }
    void f32(const float &v)
    {
        u32(std::bit_cast<std::uint32_t>(v));
    }
    void i32(std::int32_t v)
    {
        u32(std::bit_cast<std::uint32_t>(v));
    }
    void transform(const PolygonDeviceTransform &v)
    {
        if (std::holds_alternative<std::monostate>(v))
            u8(0);
        else if (const auto *words = std::get_if<std::array<std::uint32_t, 16>>(&v))
        {
            u8(1);
            for (const auto word : *words)
                u32(word);
        }
        else
        {
            u8(2);
        } // explicit431420 uninitialized source-stack provenance
    }
    void snapshot(const PolygonDeviceSnapshot &v)
    {
        for (const auto &x : v.polygon.render)
            optional32(x);
        for (const auto &stage : v.polygon.stage)
            for (const auto &x : stage)
                optional32(x);
        for (const auto &x : v.polygon.texture)
            optional32(x);
        for (const auto &x : v.light)
            optional32(x);
        flag(v.scene_open);
        u64(v.operation_sequence);
        if (transforms_)
            for (const auto &value : v.transforms)
                transform(value);
    }
    void viewport(const retail::ViewportState &v)
    {
        i32(v.width);
        i32(v.height);
        i32(v.center_pixel_x);
        i32(v.center_pixel_y);
        f32(v.center_x);
        f32(v.center_y);
        f32(v.scale_x);
        f32(v.scale_y);
        f32(v.right_edge);
        f32(v.bottom_edge);
        i32(v.min_x);
        i32(v.min_y);
        i32(v.max_x);
        i32(v.max_y);
        i32(v.depth_bits);
        i32(v.bytes_per_pixel);
        i32(v.full_width);
        i32(v.full_height);
        u32(v.cropped);
        u32(v.selector);
        f32(v.origin_x);
        f32(v.origin_y);
        for (const auto &x : v.clip_virtual)
            f32(x);
        for (const auto &x : v.clip_screen)
            f32(x);
        for (const auto &x : v.clip_expanded)
            f32(x);
        u32(v.applied_selector);
        u32(v.requested_selector);
        f32(v.ui_scale_x);
        f32(v.ui_scale_y);
    }
    void command(const CapturedPolygonCommand &command)
    {
        std::visit(
            [&](const auto &v) {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, CapturedRenderState>)
                {
                    u8(1);
                    u32(v.state);
                    u32(v.value);
                }
                else if constexpr (std::is_same_v<T, CapturedTexture>)
                {
                    u8(2);
                    u32(v.stage);
                    u32(v.token);
                }
                else if constexpr (std::is_same_v<T, CapturedTextureStageState>)
                {
                    u8(3);
                    u32(v.stage);
                    u32(v.state);
                    u32(v.value);
                }
                else
                {
                    static_assert(std::is_same_v<T, CapturedPolygonDraw>);
                    u8(4);
                    u32(v.type);
                    u32(v.fvf);
                    u32(v.flags);
                    flag(v.indexed);
                    viewport(v.viewport);
                    u64(v.vertices.size());
                    for (const auto &vertex : v.vertices)
                        for (auto word : vertex.words)
                            u32(word);
                    u64(v.indices.size());
                    for (auto index : v.indices)
                        u16(index);
                }
            },
            command);
    }
    void event(const PolygonDeviceCall &v)
    {
        u64(v.sequence);
        switch (v.operation)
        {
        case PolygonDeviceOperation::begin_scene:
            u8(0);
            break;
        case PolygonDeviceOperation::end_scene:
            u8(1);
            break;
        case PolygonDeviceOperation::render_state:
            u8(2);
            break;
        case PolygonDeviceOperation::light_state:
            u8(3);
            break;
        case PolygonDeviceOperation::texture:
            u8(4);
            break;
        case PolygonDeviceOperation::texture_stage_state:
            u8(5);
            break;
        case PolygonDeviceOperation::draw_indexed:
            u8(6);
            break;
        case PolygonDeviceOperation::draw:
            u8(7);
            break;
        case PolygonDeviceOperation::transform:
            u8(8);
            break;
        default:
            throw std::invalid_argument("unknown displayed recipe operation");
        }
        for (auto x : v.arguments)
            u32(x);
        u32(v.hresult);
        flag(v.cpu_accepted);
        switch (v.rejection)
        {
        case PolygonDeviceRejection::none:
            u8(0);
            break;
        case PolygonDeviceRejection::scene_order:
            u8(1);
            break;
        case PolygonDeviceRejection::missing_capture:
            u8(2);
            break;
        case PolygonDeviceRejection::selector:
            u8(3);
            break;
        case PolygonDeviceRejection::draw_layout:
            u8(4);
            break;
        case PolygonDeviceRejection::draw_index:
            u8(5);
            break;
        case PolygonDeviceRejection::capacity:
            u8(6);
            break;
        case PolygonDeviceRejection::allocation:
            u8(7);
            break;
        default:
            throw std::invalid_argument("unknown displayed recipe rejection");
        }
        flag(v.polygon_command.has_value());
        if (v.polygon_command)
            u64(*v.polygon_command);
        if (transforms_)
        {
            flag(v.transform_value.has_value());
            if (v.transform_value)
                transform(*v.transform_value);
        }
    }
    void bank(const PolygonGpuTextures &v)
    {
        u64(v.epoch);
        u64(v.entries.size());
        for (const auto &entry : v.entries)
        {
            u32(entry.token);
            u32(static_cast<std::uint32_t>(entry.format));
            flag(entry.resource.Get() != nullptr);
            flag(entry.source_creation.has_value());
            if (entry.source_creation)
            {
                u32(entry.source_creation->descriptor_flags);
                u32(entry.source_creation->texture_stage);
                u8(entry.source_creation->transparency_byte);
            }
            flag(bool(entry.recovery));
            if (entry.recovery)
            {
                u32(entry.recovery->width);
                u32(entry.recovery->height);
                u64(entry.recovery->words.size());
                for (auto word : entry.recovery->words)
                    u16(word);
            }
        }
    }
};
bool has_transforms(const PolygonDeviceSnapshot &s)
{
    for (const auto &value : s.transforms)
        if (!std::holds_alternative<std::monostate>(value))
            return true;
    return false;
}
bool needs_version2(const ImmutablePolygonSubmission &submission)
{
    if (!submission)
        return false;
    if (has_transforms(submission->initial) || has_transforms(submission->final))
        return true;
    for (const auto &segment : submission->segments)
        if (segment.device_frame)
        {
            const auto &frame = *segment.device_frame;
            if (has_transforms(frame.initial) || has_transforms(frame.final))
                return true;
            for (const auto &event : frame.events)
                if (event.operation == PolygonDeviceOperation::transform || event.transform_value)
                    return true;
        }
    return false;
}
} // namespace
void write_displayed_recipe(std::ostream &out, const ImmutablePolygonSubmission &displayed)
{
    static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559);
    // No locale, precision or formatting state changes on the caller's stream.
    const bool transforms = needs_version2(displayed);
    out.write("SGNDRCP\0", 8);
    Writer w(out, transforms);
    w.u32(transforms ? 2u : 1u);
    w.flag(bool(displayed));
    if (displayed)
    {
        const auto &v = *displayed;
        w.u64(v.source_tick);
        w.flag(bool(v.device));
        w.snapshot(v.initial);
        w.snapshot(v.final);
        w.u64(v.commands);
        w.u64(v.payload_bytes);
        w.u64(v.events);
        w.u64(v.segments.size());
        for (const auto &segment : v.segments)
        {
            w.flag(bool(segment.device_frame));
            if (segment.device_frame)
            {
                const auto &frame = *segment.device_frame;
                w.flag(bool(frame.device));
                w.flag(frame.device == v.device);
                w.snapshot(frame.initial);
                w.snapshot(frame.final);
                w.flag(bool(frame.polygon_commands));
                if (frame.polygon_commands)
                {
                    const auto &commands = *frame.polygon_commands;
                    w.u64(commands.source_tick);
                    w.u64(commands.resource_epoch);
                    w.u64(commands.payload_bytes);
                    w.u64(commands.commands.size());
                    for (const auto &command : commands.commands)
                        w.command(command);
                }
                w.u64(frame.events.size());
                for (const auto &event : frame.events)
                    w.event(event);
            }
            w.flag(bool(segment.textures));
            if (segment.textures)
                w.bank(*segment.textures);
        }
    }
    w.flush();
    out.write("ENDDRCP\0", 8);
    if (!out)
        throw std::runtime_error("displayed recipe stream write failed");
}
void export_displayed_recipe(const std::filesystem::path &path, const ImmutablePolygonSubmission &displayed)
{
    if (path.empty())
        throw std::invalid_argument("displayed recipe output path is empty");
    // The caller explicitly selected this file. Do not create unrelated paths.
    std::ofstream out(path, std::ios::binary | std::ios::out | std::ios::noreplace);
    if (!out)
        throw std::runtime_error("cannot open displayed recipe output");
    write_displayed_recipe(out, displayed);
    out.close();
    if (!out)
        throw std::runtime_error("displayed recipe output close failed");
}
} // namespace sgn::diagnostics
