#include "sgn/source_world_submission.hpp"
#include "sgn/host_presentation.hpp"
#include "sgn/retail_frontend_state.hpp"
#include "sgn/race_session.hpp"
#include "sgn/scene_resources.hpp"
#include "sgn/source_submission_strip_table.hpp"
#include "sgn/gpu_world.hpp" //NEW host rendering (gpu_world R19)
#include "sgn/source_bank_read_memo.hpp"
#include "sgn/host_profiler.hpp"
#include <cstring>
#include <limits>
#include <optional>
#include <array>
#include <utility>

extern "C" void sgn_source_submit_sky_4333e9(void *, const void *, const void *, const void *) noexcept;
extern "C" void sgn_source_submit_opaque_4335e8(void *, const void *, const void *, const void *) noexcept;
extern "C" void sgn_source_submit_specular_4337fc(void *, const void *, const void *, const void *) noexcept;
extern "C" void sgn_source_submit_multi_433dd3(void *, const void *, const void *) noexcept;
extern "C" void sgn_source_submit_alpha_433b07(void *, const void *, const void *, const void *, std::uint32_t,
                                               std::uint32_t) noexcept;

namespace sgn::retail
{
namespace
{
using W = std::uint32_t;
using Route = PolygonSubmissionRoute;
void require(bool condition, W site, const char *reason)
{
    if (!condition)
        throw SourceAllocationError(site, reason);
}
std::size_t sum(std::size_t a, std::size_t b)
{
    require(b <= std::numeric_limits<std::size_t>::max() - a, 0x433f40, "submission host size overflow");
    return a + b;
}
std::size_t product(std::size_t a, std::size_t b)
{
    require(a == 0 || b <= std::numeric_limits<std::size_t>::max() / a, 0x433f40, "submission host product overflow");
    return a * b;
}
// Only submission scratch grows. Retained words survive; appended undefined
// source scratch is initialized under the existing NEW HOST zero policy.
template <class T>
void grow(std::vector<T> &target, std::size_t needed, PolygonSubmissionState &state, PolygonSubmissionBudget budget)
{
    if (needed <= target.size())
        return;
    require(needed <= target.max_size(), 0x433f40, "source submission scratch exceeds host vector extent");
    const auto old = sum(product(sum(state.indexed_vertices.size(), state.alpha_vertices.size()), 40),
                         product(state.indices.size(), 2));
    const auto added = product(needed - target.size(), sizeof(T));
    require(sum(old, added) <= budget.retained_payload_bytes, 0x433f40,
            "source submission retained scratch budget exceeded");
    require(sum(old, product(needed, sizeof(T))) <= budget.transaction_payload_bytes, 0x433f40,
            "source submission scratch transaction budget exceeded");
    target.resize(needed);
}
class Invocation
{
    SourceWorldSubmissionContext &c_;
    std::shared_ptr<RaceSessionPlatform> platform_;
    WorldPolygonFrameTicket ticket_;
    std::size_t visits_{};
    // NEW host performance (perf_world R19). One Invocation is one synchronous
    // submission: it reads source packets/PMD/vertex payloads and issues sink
    // (renderer capture) calls only. Nothing allocates or releases bank storage
    // or stores opaque transport words, and address tokens are never recycled,
    // so a token decodes to the same location for the whole invocation and the
    // memo returns exactly what the uncached bank calls returned. Each memo
    // performs the full uncached admission the first time it is reached.
    std::optional<SourceBankReadMemo> memo_;
    struct Decoded
    {
        std::uint32_t token{};
        bool valid{};
        SourceAddressLocation location;
    };
    std::array<Decoded, 64> decoded_{};

  public:
    explicit Invocation(SourceWorldSubmissionContext &c) : c_(c), platform_(c.frontend.shared_platform())
    {
        require(platform_ && platform_->source_construction_only_entry(), 0x433f40,
                "submission requires actual source platform");
        platform_->require_owner_thread();
        c_.frames.require_source_frontend(c_.frontend);
        ticket_ = capture_world_polygon_level(state().polygon_frame);
        validate_world_polygon_level(state().polygon_frame, ticket_);
        memo_.emplace(platform_->source_allocations());
    }
    SourceBankReadMemo &memo()
    {
        return *memo_;
    }
    // == platform_->source_allocations().decode_address(token); the reference
    // stays valid until the next decode() call.
    const SourceAddressLocation &decode(W token)
    {
        auto &entry = decoded_[token & 63u];
        if (entry.valid && entry.token == token)
            return entry.location;
        auto location = platform_->source_allocations().decode_address(token);
        entry.location = std::move(location);
        entry.token = token;
        entry.valid = true;
        return entry.location;
    }
    RaceSessionState &state()
    {
        return platform_->state();
    }
    const std::shared_ptr<RaceSessionPlatform> &platform() const
    {
        return platform_;
    }
    PolygonSubmissionState &scratch()
    {
        return c_.scratch;
    }
    PolygonSubmissionSink &sink()
    {
        return c_.sink;
    }
    void visit(WorldPolygonPacketPointer::SourceRun &p)
    {
        require(visits_ < c_.budget.packet_visits, 0x433f40, "source submission packet visit budget exceeded");
        ++visits_;
        validate_world_polygon_level(state().polygon_frame, ticket_);
        p.require_source_platform(platform_);
    }
    void check_scratch()
    {
        const auto frame = c_.frames.borrow_loading_frame();
        require(&frame.submission == &c_.scratch, 0x433f40, "source submission has foreign scratch owner");
    }
    const PolygonSubmissionMaterial &material(W id, W site)
    {
        const auto frame = c_.frames.borrow_loading_frame();
        const auto actual = c_.materials.names.materials.active_materials();
        require(&frame.submission == &c_.scratch && frame.quads.materials.data() == actual.data() &&
                    frame.quads.materials.size() == actual.size(),
                site, "source submission has foreign material/scratch owners");
        // Source indexes all255 physical records; it does not gate this read
        // on the loader cursor or6243BC. Record255 reaches adjacent globals.
        require(id < PhysicalMaterialStorage::capacity, site,
                "source material read reaches unrepresented adjacent globals");
        return c_.materials.names.materials.record(id);
    }
    W sky_base()
    {
        return c_.materials.names.sky_material_base;
    }
    W depth_capability()
    {
        return c_.frames.capabilities().depth;
    }
    W multitexture_capability()
    {
        return c_.frames.capabilities().multitexture;
    }
    const void *bytes(W token, std::uint64_t offset, std::size_t length, W site)
    {
        const auto &location = decode(token);
        auto &bank = memo();
        const auto size = bank.size(location.allocation); //== allocation_size
        const auto at = std::uint64_t(location.offset) + offset;
        require(at <= size && length <= size - at, site, "source submission pointer leaves actual allocation");
        return bank.readable(location.allocation, static_cast<W>(at), static_cast<W>(length), site).data();
    }
    template <class T> T read(W token, std::uint64_t offset, W site)
    {
        T value;
        std::memcpy(&value, bytes(token, offset, sizeof(T), site), sizeof(T));
        return value;
    }
    std::uint16_t index(W token, W ordinal, W site)
    {
        if (token == 0x6244a8)
        {
            const auto &values = state().polygon_frame.clip.sequential_indices;
            require(ordinal < values.size(), site, "source sequential index exceeds16-word owner");
            return values[ordinal];
        }
        return read<std::uint16_t>(token, std::uint64_t(ordinal) * 2, site);
    }
    PolygonSubmissionVertex &output(bool alpha, std::size_t at)
    {
        auto &target = alpha ? c_.scratch.alpha_vertices : c_.scratch.indexed_vertices;
        grow(target, sum(at, 1), c_.scratch, c_.budget);
        return target[at];
    }
    void index_store(std::size_t at, W value)
    {
        grow(c_.scratch.indices, sum(at, 1), c_.scratch, c_.budget);
        c_.scratch.indices[at] = static_cast<std::uint16_t>(value); // source WORD truncation
    }
    void draw_indexed(std::size_t vertices, std::size_t indices, W site)
    {
        require(vertices <= c_.scratch.indexed_vertices.size() && indices <= c_.scratch.indices.size(), site,
                "submission scratch changed during source draw");
        c_.sink.draw_indexed(4, 0x2c4, std::span(c_.scratch.indexed_vertices).first(vertices),
                             std::span(c_.scratch.indices).first(indices), 0x1c, state().viewport);
    }
    void draw_alpha(W count)
    {
        require(count <= c_.scratch.alpha_vertices.size(), 0x433b7c, "source alpha scratch count outside owner");
        c_.sink.draw(5, 0x2c4, std::span(c_.scratch.alpha_vertices).first(count), 0x1c, state().viewport);
    }
    void depth()
    {
        if (depth_capability() == 0)
            return;
        sink().set_render_state(23, 7);
        sink().set_render_state(7, 1);
        auto &s = scratch();
        s.depth_scale_bits = 0x43000000;
        s.depth_enabled = 1;
        s.depth_write = 1;
        s.depth_other = 0;
    }
    void specular(bool enabled)
    {
        if (state().view_quality.legacy_driver_restriction == 1)
            return;
        scratch().specular_enabled = enabled ? 1u : 0u;
        sink().set_render_state(29, enabled ? 1u : 0u);
        sink().set_render_state(24, enabled ? 1u : 160u);
    }
    void sky_begin()
    {
        const auto code = sink().set_render_state(23, 8);
        if (code != 0)
        {
            const char *description = c_.failure.describe_error_422690(code);
            c_.failure.exit_44e4a0(description);
        }
    }
};
// Captured source pointer metadata, never a native TextureMotion record.
class Motion
{
    using Pointer = std::variant<SourcePmdLocation, SourceSceneStaticPointer>;
    std::optional<Pointer> track_;
    W offset_{}, sky_{};

  public:
    static Motion sky(W index)
    {
        Motion p;
        p.sky_ = index;
        return p;
    }
    static Motion track(Invocation &in, W index)
    {
        Motion p;
        p.offset_ = index * 20u;
        p.track_ = in.platform()->source_scene_resources().models().source_loading_state().track.texture_motion;
        return p;
    }
    const void *uv(Invocation &in, W site) const
    {
        if (!track_)
        {
            const auto &sky = in.state().texture_animation.sky;
            if (sky_ < sky.size())
                return &sky[sky_].u;
            // 4333A1..4333A9 addresses 577660+20*(id-[583590]) with no bound; the
            // UV pair is read at +4/+8 (433402/43340D). Records past the two sky
            // records overlap the next retained owner, 577688 LevelCurveTables::
            // far_clip (256 floats). Read that same owner, as the original does.
            const auto &clip_table = in.state().curves.far_clip;
            const std::uint64_t byte = std::uint64_t(sky_) * 20u + 4u; // from 577660
            require(byte >= 0x28u && (byte - 0x28u) / 4u + 2u <= clip_table.size(), site,
                    "source sky UV read leaves the 577660/577688 owners");
            return &clip_table[static_cast<std::size_t>((byte - 0x28u) / 4u)];
        }
        const auto *p = std::get_if<SourcePmdLocation>(&*track_);
        require(p != nullptr, site, "source static624488 motion bytes are not represented");
        auto &bank = in.memo();
        const auto size = bank.size(p->allocation); //== allocation_size
        const auto at = std::uint64_t(p->offset) + offset_ + 4u;
        require(at <= size && 8u <= size - at, site, "source motion UV read leaves actual PMD allocation");
        return bank.readable(p->allocation, static_cast<W>(at), 8, site).data();
    }
};
struct CornerSites
{
    W index, vertex_index, attribute, vertex, motion;
};
CornerSites corner_sites(Route route)
{
    switch (route)
    {
    case Route::sky:
        return {0x4333cd, 0x4333e0, 0x433405, 0x4333e9, 0x433402};
    case Route::opaque:
        return {0x4335cc, 0x4335df, 0x4335e8, 0x4335ea, 0x433603};
    case Route::specular:
        return {0x4337e0, 0x4337f3, 0x4337fc, 0x4337fe, 0x433817};
    case Route::multitexture:
        return {0x433db7, 0x433dca, 0x433de7, 0x433dd3, 0};
    case Route::alpha:
        return {0x433aed, 0x433afe, 0x433b07, 0x433b09, 0x433b21};
    default:
        throw SourceAllocationError(0x433f40, "unknown source corner route");
    }
}
struct IndexedSites
{
    W material_id, count, primary, motion_index, flush, final_flush;
};
IndexedSites indexed_sites(Route route)
{
    switch (route)
    {
    case Route::sky:
        return {0x433305, 0x4333b7, 0x433382, 0, 0x433359, 0x4334a4};
    case Route::opaque:
        return {0x433509, 0x4335b8, 0x43358a, 0x43359a, 0x43355e, 0x4336ab};
    case Route::specular:
        return {0x43371b, 0x4337ca, 0x43379c, 0x4337ac, 0x433770, 0x4338c5};
    case Route::multitexture:
        return {0x433d07, 0x433da1, 0x433d84, 0, 0x433d5b, 0x433e9d};
    default:
        throw SourceAllocationError(0x433f40, "unknown source indexed route");
    }
}
void corner(Invocation &in, Route route, W attributes, W vertices, W indices, W ordinal, std::size_t output,
            const Motion *motion, W mode = 0, W alpha = 0)
{
    const auto sites = corner_sites(route);
    const auto attribute_index = in.index(indices, ordinal, sites.index);
    const auto offset = std::uint64_t(attribute_index) * 16;
    const auto vertex_index = in.read<std::uint16_t>(attributes, offset + 8, sites.vertex_index);
    const void *a = in.bytes(attributes, offset, route == Route::sky ? 8u : 16u, sites.attribute);
    const void *v = in.bytes(vertices, std::uint64_t(vertex_index) * 32, 32, sites.vertex);
    auto &destination = in.output(route == Route::alpha, output);
    if (route == Route::multitexture)
    {
        sgn_source_submit_multi_433dd3(&destination, a, v);
        return;
    }
    require(motion != nullptr, sites.motion, "source submission lacks its captured motion pointer");
    const auto uv = motion->uv(in, sites.motion);
    switch (route)
    {
    case Route::sky:
        sgn_source_submit_sky_4333e9(&destination, a, v, uv);
        break;
    case Route::opaque:
        sgn_source_submit_opaque_4335e8(&destination, a, v, uv);
        break;
    case Route::specular:
        sgn_source_submit_specular_4337fc(&destination, a, v, uv);
        break;
    case Route::alpha:
        sgn_source_submit_alpha_433b07(&destination, a, v, uv, mode, alpha);
        break;
    default:
        throw SourceAllocationError(0x433f40, "unknown source submission route");
    }
}
void indexed(Invocation &in, Route route, WorldPolygonPacketPointer packet, W remaining, bool track_inside = false)
{
    if (remaining == 0)
        return;
    const auto sites = indexed_sites(route);
    if (route == Route::multitexture)
    {
        if (in.multitexture_capability() == 0)
        {
            indexed(in, Route::opaque, packet, remaining);
            return;
        }
        const auto quality = in.state().view_quality.second;
        if (quality == 0)
        {
            indexed(in, Route::opaque, packet, remaining);
            return;
        }
        if (quality == 1)
            in.specular(true);
        const auto texture = in.material(0, 0x433c8d).alternate_texture;
        in.sink().set_texture(1, texture);
        in.sink().set_texture_stage_state(1, 11, 1);
        in.sink().set_texture_stage_state(1, 1, 7);
    }
    else if (route == Route::sky)
        in.sky_begin();
    else if (route == Route::specular && in.state().view_quality.second == 1)
        in.specular(true);
    W material = 0xffffffffu, base = 0;
    std::size_t vertices_written = 0, indices_written = 0;
    std::optional<Motion> motion;
    // NEW host rendering (gpu_world R19): mode 1 draws the 42A090 track list
    // from static GPU buffers. Only corner() and the draw payload change; the
    // state calls, material runs and flush points below are the reference.
    std::optional<gpu_world::TrackRun> gpu;
    if (track_inside && route == Route::opaque)
        gpu = gpu_world::TrackRun::open_inside_opaque(in.platform(), in.sink());
    const auto flush = [&](W site) {
        if (!gpu)
        {
            in.draw_indexed(vertices_written, indices_written, site);
            return;
        }
        require(indices_written <= in.scratch().indices.size(), site, "submission scratch changed during source draw");
        const void *uv = vertices_written != 0 && motion ? motion->uv(in, corner_sites(route).motion) : nullptr;
        gpu->flush_indexed(std::span<const std::uint16_t>(in.scratch().indices).first(indices_written),
                           vertices_written, uv, in.state().viewport);
    };
    WorldPolygonPacketPointer::SourceRun run(packet); // NEW host performance: checked record run
    do
    {
        in.visit(run);
        const auto header = run.source_word(0);
        if (header != 0)
        {
            const auto attributes = run.source_word(2), vertices = run.source_word(3);
            if (gpu)
                gpu->admit_packet(attributes, vertices);
            W indices = 0;
            if (route == Route::sky || route == Route::multitexture)
                indices = run.source_word(1);
            const auto id = in.read<std::uint8_t>(header, 1, sites.material_id);
            if (id != material)
            {
                if (base != 0)
                    flush(sites.flush);
                material = id;
                base = 0;
                vertices_written = 0;
                indices_written = 0;
                const auto texture = in.material(id, sites.primary).primary_texture;
                in.sink().set_texture(0, texture);
                if (route == Route::sky)
                    motion = Motion::sky(W(id) - in.sky_base());
                else if (route != Route::multitexture)
                {
                    const auto index = in.material(id, sites.motion_index).motion_index; // AFTER SetTexture
                    motion = Motion::track(in, index); // fresh624488, captured until material changes
                }
            }
            // clang-format off: keep the trailing comment on one line (ReflowComments would split it)
            const auto count = in.read<std::uint8_t>(run.source_word(0), 0, sites.count); // fresh header AFTER API calls
            // clang-format on
            if (count != 0)
            {
                if (route == Route::opaque || route == Route::specular)
                    indices = run.source_word(1);
                for (W i = 0; i < count; ++i)
                {
                    if (gpu)
                        gpu->corner(vertices_written, in.index(indices, i, corner_sites(route).index));
                    else
                        corner(in, route, attributes, vertices, indices, i, vertices_written,
                               motion ? &*motion : nullptr);
                    ++vertices_written;
                }
                host_presentation::submitted_packet(run.pointer().offset().value_or(0), route == Route::sky, count,
                                                    header); // NEW R19 interp host tag (R28: header)
            }
            const W first = base;
            ++base;
            for (W i = 2; i < count; ++i)
            {
                in.index_store(indices_written++, first);
                in.index_store(indices_written++, base);
                ++base;
                in.index_store(indices_written++, base);
            }
            ++base; // even source counts0/1/2 advance the logical WORD base by2
        }
        run.advance();
        --remaining;
    } while (remaining != 0);
    if (base != 0)
        flush(sites.final_flush); // source EAX/EBX gate, not vertex count
    if (route == Route::multitexture)
        in.sink().set_texture_stage_state(1, 1, 1);
    if ((route == Route::specular || route == Route::multitexture) && in.state().view_quality.second == 1)
        in.specular(false);
    if (route == Route::sky)
        in.depth();
}
void alpha(Invocation &in, WorldPolygonPacketPointer packet, W remaining, bool inside_list = false)
{
    if (remaining == 0)
        return;
    W material = 0xffffffffu, previous_mode = 0xffffffffu;
    std::optional<Motion> motion;
    in.sink().set_render_state(14, 0);
    in.sink().set_render_state(27, 1);
    WorldPolygonPacketPointer::SourceRun run(packet); // NEW host performance: checked record run
    do
    {
        in.visit(run);
        const auto header = run.source_word(0);
        if (header != 0)
        {
            const auto attributes = run.source_word(2), vertices = run.source_word(3), indices = run.source_word(1);
            const W mode = in.read<std::uint8_t>(header, 3, 0x433940) & 0x78u;
            if (mode != previous_mode)
            {
                previous_mode = mode;
                const auto blend = source_submission_detail::blend_for_mode(mode);
                in.sink().set_render_state(19, blend[0]);
                in.sink().set_render_state(20, blend[1]);
                in.sink().set_texture_stage_state(0, 2, 2);
            }
            // EBX retains the original header through blend and texture calls.
            const auto id = in.read<std::uint8_t>(header, 1, 0x433a79);
            if (id != material)
            {
                material = id;
                const auto texture = in.material(id, 0x433a96).primary_texture;
                in.sink().set_texture(0, texture);
                const auto index = in.material(id, 0x433aa6).motion_index;
                motion = Motion::track(in, index);
            }
            const W count = in.read<std::uint8_t>(header, 0, 0x433abe);
            const W alpha_word = W(in.read<std::uint8_t>(header, 2, 0x433ac0)) << 24;
            // NEW host rendering (gpu_world R19): 42A090 track packets of 6243DC.
            std::optional<gpu_world::TrackRun> gpu;
            if (inside_list)
                gpu = gpu_world::TrackRun::open_inside_alpha(in.platform(), in.sink(), vertices);
            if (gpu)
                gpu->admit_packet(attributes, vertices);
            for (W i = 0; i < count; ++i)
            {
                const W ordinal = source_submission_detail::strip_index(count, i); // actual477CD0 DWORD
                if (gpu)
                    gpu->corner(i, in.index(indices, ordinal, corner_sites(Route::alpha).index));
                else
                    corner(in, Route::alpha, attributes, vertices, indices, ordinal, i, motion ? &*motion : nullptr,
                           mode, alpha_word);
            }
            host_presentation::submitted_packet(run.pointer().offset().value_or(0), false, count,
                                                header); // NEW R19 interp host tag (R28: header)
            if (gpu)
                gpu->flush_alpha(count,
                                 count != 0 && motion ? motion->uv(in, corner_sites(Route::alpha).motion) : nullptr,
                                 mode, alpha_word, in.state().viewport);
            else
                in.draw_alpha(count); // also for count0
        }
        run.advance();
        --remaining;
    } while (remaining != 0);
    in.sink().set_render_state(19, 2);
    in.sink().set_render_state(20, 1);
    in.sink().set_texture_stage_state(0, 2, 2);
    in.sink().set_texture_stage_state(0, 3, 0);
    in.sink().set_render_state(27, 0);
    in.sink().set_render_state(14, 1);
    in.depth();
}
void route(Invocation &in, Route selected, WorldPolygonPacketPointer p, W count)
{
    switch (selected)
    {
    case Route::alpha:
        alpha(in, p, count);
        break;
    case Route::sky:
    case Route::opaque:
    case Route::specular:
    case Route::multitexture:
        indexed(in, selected, p, count);
        break;
    default:
        throw SourceAllocationError(0x433f40, "unknown source submission route");
    }
}
} // namespace
void submit_source_world_polygon_route(SourceWorldSubmissionContext &c, Route selected, WorldPolygonPacketPointer p,
                                       W count)
{
    Invocation in(c);
    in.check_scratch();
    try
    {
        route(in, selected, p, count);
    }
    catch (...)
    {
        detach_world_polygon_level(in.state().polygon_frame);
        throw;
    }
}
void submit_source_world_polygons_433f40(SourceWorldSubmissionContext &c)
{
    Invocation in(c);
    in.check_scratch();
    auto &frame = in.state().polygon_frame;
    const auto group = [&](Route selected, unsigned g, unsigned p) {
        const auto count = frame.groups[g][p].count;
        const auto first = frame.groups[g][p].first;
        host_presentation::SubmissionListScope presentation_list(p); // NEW R19 interp host tag: packets[p]
        // NEW host rendering (gpu_world R19): 6243E0 is the 42A090 track list.
        if (g == 1 && p == 2 && selected == Route::opaque)
        {
            indexed(in, selected, first, count, true);
            return;
        }
        route(in, selected, first, count);
    };
    try
    {
        in.depth();
        in.sink().set_render_state(24, 160);
        {
            SGN_PROFILE_SCOPE("submission.opaque_groups");
            group(Route::sky, 0, 0);
            host_presentation::pass_yield_point(); // NEW HOST (R33): may present one interpolated frame, no source work
            group(Route::opaque, 1, 0);
            host_presentation::pass_yield_point(); // NEW HOST (R33): may present one interpolated frame, no source work
            group(Route::opaque, 1, 2);
            host_presentation::pass_yield_point(); // NEW HOST (R33): may present one interpolated frame, no source work
            const auto middle = in.state().view_quality.second == 1 ? Route::specular : Route::opaque;
            group(middle, 2, 0);
            host_presentation::pass_yield_point(); // NEW HOST (R33): may present one interpolated frame, no source work
            group(middle, 2, 2);
            host_presentation::pass_yield_point(); // NEW HOST (R33): may present one interpolated frame, no source work
            group(Route::multitexture, 3, 0);
            host_presentation::pass_yield_point(); // NEW HOST (R33): may present one interpolated frame, no source work
            group(Route::multitexture, 3, 2);
        }
        host_presentation::pass_yield_point(); // NEW HOST (R33): may present one interpolated frame, no source work
        in.sink().set_render_state(24, 1);
        {
            SGN_PROFILE_SCOPE("submission.alpha_lists");
            {
                const auto count = frame.totals[1];
                const auto first = frame.packets[1].base();
                host_presentation::SubmissionListScope presentation_list(1);
                alpha(in, first, count);
            } // NEW R19 interp host tag
            {
                const auto count = frame.totals[3];
                const auto first = frame.packets[3].base();
                host_presentation::SubmissionListScope presentation_list(3);
                alpha(in, first, count, true);
            } // NEW R19 interp host tag
        }
    }
    catch (...)
    {
        detach_world_polygon_level(frame);
        throw;
    }
}
} // namespace sgn::retail
