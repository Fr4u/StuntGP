#include "sgn/world_join_sidecar.hpp"
#include "sgn/displayed_recipe_export.hpp"
#include <charconv>
#include <fstream>
#include <stdexcept>
#include <streambuf>
#include <string_view>

namespace sgn::diagnostics
{
namespace
{
class HashBuffer final : public std::streambuf
{
    std::streambuf &target_;
    WorldJoinSha256 hash_;

  protected:
    std::streamsize xsputn(const char *bytes, std::streamsize count) override
    {
        const auto written = target_.sputn(bytes, count);
        if (written > 0)
            hash_.append({reinterpret_cast<const std::uint8_t *>(bytes), static_cast<std::size_t>(written)});
        return written;
    }
    int_type overflow(int_type c) override
    {
        if (traits_type::eq_int_type(c, traits_type::eof()))
            return traits_type::not_eof(c);
        const auto byte = traits_type::to_char_type(c);
        return xsputn(&byte, 1) == 1 ? c : traits_type::eof();
    }
    int sync() override
    {
        return target_.pubsync();
    }

  public:
    explicit HashBuffer(std::streambuf &target) : target_(target) {}
    WorldJoinFingerprint fingerprint() const
    {
        return hash_.finish();
    }
};
class Json
{
    std::ostream &out_;

  public:
    explicit Json(std::ostream &out) : out_(out) {}
    void raw(std::string_view s)
    {
        out_.write(s.data(), static_cast<std::streamsize>(s.size()));
    }
    void number(std::uint64_t n)
    {
        std::array<char, 32> text{};
        const auto r = std::to_chars(text.data(), text.data() + text.size(), n);
        if (r.ec != std::errc{})
            throw std::runtime_error("sidecar integer conversion");
        raw({text.data(), static_cast<std::size_t>(r.ptr - text.data())});
    }
    void wide(std::uint64_t n)
    {
        raw("\"");
        number(n);
        raw("\"");
    }
    void flag(bool b)
    {
        raw(b ? "true" : "false");
    }
    template <class T> void optional(const std::optional<T> &n)
    {
        if (n)
            number(*n);
        else
            raw("null");
    }
    void optional_wide(const std::optional<std::uint64_t> &n)
    {
        if (n)
            wide(*n);
        else
            raw("null");
    }
    void hex(std::span<const std::uint8_t> bytes)
    {
        static constexpr char digits[] = "0123456789abcdef";
        raw("\"");
        for (auto b : bytes)
        {
            const std::array pair{digits[b >> 4], digits[b & 15]};
            raw({pair.data(), 2});
        }
        raw("\"");
    }
    void ascii(std::string_view text)
    {
        for (unsigned char c : text)
            if (c >= 128)
            {
                raw("null");
                return;
            }
        static constexpr char digits[] = "0123456789abcdef";
        raw("\"");
        for (unsigned char c : text)
        {
            if (c == '"' || c == '\\')
            {
                raw("\\");
                const char ch = static_cast<char>(c);
                raw({&ch, 1});
            }
            else if (c < 32)
            {
                raw("\\u00");
                const std::array pair{digits[c >> 4], digits[c & 15]};
                raw({pair.data(), 2});
            }
            else
            {
                const char ch = static_cast<char>(c);
                raw({&ch, 1});
            }
        }
        raw("\"");
    }
    void header(const retail::WorldPolygonHeader &bytes)
    {
        hex(bytes);
    }
    template <class Range, class F> void array(const Range &range, F write)
    {
        raw("[");
        bool first = true;
        for (const auto &v : range)
        {
            if (!first)
                raw(",");
            first = false;
            write(v);
        }
        raw("]");
    }
    void range(const WorldJoinRange &r)
    {
        raw("{\"pool\":");
        number(r.pool);
        raw(",\"first\":");
        optional(r.first);
        raw(",\"count\":");
        number(r.count);
        raw("}");
    }
};
const char *lookup_name(WorldJoinLookupStatus status)
{
    switch (status)
    {
    case WorldJoinLookupStatus::matched:
        return "matched";
    case WorldJoinLookupStatus::no_displayed:
        return "no_displayed";
    case WorldJoinLookupStatus::not_retained:
        return "not_retained";
    case WorldJoinLookupStatus::ambiguous:
        return "ambiguous";
    case WorldJoinLookupStatus::foreign_thread:
        return "foreign_thread";
    }
    throw std::invalid_argument("unknown sidecar lookup status");
}
const char *core_name(WorldJoinStatus status)
{
    switch (status)
    {
    case WorldJoinStatus::disabled:
        return "disabled";
    case WorldJoinStatus::collecting:
        return "collecting";
    case WorldJoinStatus::derived_checked:
        return "DERIVED_CHECKED";
    case WorldJoinStatus::incomplete:
        return "incomplete";
    case WorldJoinStatus::mismatch:
        return "mismatch";
    case WorldJoinStatus::no_recipe:
        return "no_recipe";
    }
    throw std::invalid_argument("unknown sidecar core status");
}
const char *kind_name(WorldHeaderKind kind)
{
    switch (kind)
    {
    case WorldHeaderKind::model_table:
        return "model_table";
    case WorldHeaderKind::model_part:
        return "model_part";
    case WorldHeaderKind::sprite:
        return "sprite";
    case WorldHeaderKind::strip:
        return "strip";
    }
    throw std::invalid_argument("unknown header owner kind");
}
const char *relation_name(WorldPreclipRelation r)
{
    switch (r)
    {
    case WorldPreclipRelation::unchanged:
        return "unchanged";
    case WorldPreclipRelation::replaced:
        return "replaced";
    case WorldPreclipRelation::rejected:
        return "rejected";
    case WorldPreclipRelation::absent:
        return "absent";
    }
    throw std::invalid_argument("unknown preclip relation");
}
const char *route_name(retail::PolygonSubmissionRoute r)
{
    switch (r)
    {
    case retail::PolygonSubmissionRoute::sky:
        return "sky";
    case retail::PolygonSubmissionRoute::opaque:
        return "opaque";
    case retail::PolygonSubmissionRoute::specular:
        return "specular";
    case retail::PolygonSubmissionRoute::multitexture:
        return "multitexture";
    case retail::PolygonSubmissionRoute::alpha:
        return "alpha";
    }
    throw std::invalid_argument("unknown polygon route");
}
void provenance(Json &j, std::size_t first, std::size_t count)
{
    j.raw(",\"membership_begin\":");
    j.number(first);
    j.raw(",\"membership_count\":");
    j.number(count);
    j.raw(",\"membership_status\":");
    j.ascii(count == 0 ? "unknown" : count == 1 ? "one_storage_membership" : "multiple_storage_memberships");
}
void result(Json &j, const WorldJoinResult &r)
{
    j.raw("{\"owners\":");
    j.array(r.owners, [&](const auto &o) {
        j.raw("{\"kind\":");
        j.ascii(kind_name(o.kind));
        j.raw(",\"registry_token\":");
        j.optional(o.registry_token);
        j.raw(",\"model_handle\":");
        j.optional(o.model_handle);
        j.raw(",\"lod\":");
        j.optional(o.lod);
        j.raw(",\"part\":");
        j.optional(o.part);
        j.raw(",\"source_lods\":");
        j.array(o.source_lods, [&](const auto &n) { j.optional(n); });
        j.raw(",\"source_name_ascii\":");
        j.ascii(o.source_name);
        j.raw(",\"source_name_bytes_hex\":");
        j.hex({reinterpret_cast<const std::uint8_t *>(o.source_name.data()), o.source_name.size()});
        j.raw("}");
    });
    j.raw(",\"memberships\":");
    j.array(r.memberships, [&](const auto &m) {
        j.raw("{\"owner\":");
        j.number(m.owner);
        j.raw(",\"offset\":");
        j.number(m.offset);
        j.raw("}");
    });
    j.raw(",\"origins\":");
    j.array(r.origins, [&](const auto &o) {
        j.raw("{\"invocation\":");
        j.number(o.invocation);
        j.raw(",\"pool\":");
        j.number(o.pool);
        j.raw(",\"slot\":");
        j.number(o.slot);
        j.raw(",\"present\":");
        j.flag(o.present);
        j.raw(",\"header_hex\":");
        j.header(o.header);
        j.raw(",\"inside_mask\":");
        j.number(o.inside_mask);
        j.raw(",\"clipping_lists_mask\":");
        j.number(o.clipping_lists);
        provenance(j, o.membership_begin, o.membership_count);
        j.raw("}");
    });
    j.raw(",\"occurrences\":");
    j.array(r.occurrences, [&](const auto &o) {
        j.raw("{\"origin\":");
        j.number(o.origin);
        j.raw(",\"list\":");
        j.number(o.list);
        j.raw(",\"entry\":");
        j.number(o.entry);
        j.raw(",\"present\":");
        j.flag(o.present);
        j.raw(",\"header_hex\":");
        j.header(o.header);
        j.raw(",\"clip_header\":");
        j.optional(o.clip_header);
        j.raw(",\"relation\":");
        j.ascii(relation_name(o.relation));
        j.raw(",\"primary_texture_token\":");
        j.number(o.primary_token);
        provenance(j, o.membership_begin, o.membership_count);
        j.raw("}");
    });
    j.raw(",\"invocations\":");
    j.array(r.invocations, [&](const auto &v) {
        j.raw("{\"controller\":");
        j.number(v.controller);
        j.raw(",\"viewport_u32\":");
        j.array(v.viewport, [&](auto n) { j.number(n); });
        j.raw(",\"sequence_before\":");
        j.wide(v.before);
        j.raw(",\"sequence_after\":");
        j.wide(v.after);
        j.raw(",\"depth\":");
        j.number(v.depth);
        j.raw(",\"quality\":");
        j.number(v.quality);
        j.raw(",\"restriction\":");
        j.number(v.restriction);
        j.raw(",\"multitexture\":");
        j.number(v.multitexture);
        j.raw(",\"alternate_texture_token\":");
        j.number(v.alternate_token);
        j.raw(",\"list_begin\":");
        j.array(v.list_begin, [&](auto n) { j.number(n); });
        j.raw(",\"list_count\":");
        j.array(v.list_count, [&](auto n) { j.number(n); });
        j.raw(",\"ranges\":");
        j.array(v.ranges, [&](const auto &n) { j.range(n); });
        j.raw(",\"clipping\":");
        j.array(v.clipping, [&](const auto &n) { j.range(n); });
        j.raw("}");
    });
    j.raw(",\"batches\":");
    j.array(r.batches, [&](const auto &b) {
        j.raw("{\"invocation\":");
        j.number(b.invocation);
        j.raw(",\"list\":");
        j.number(b.list);
        j.raw(",\"segment\":");
        j.number(b.segment);
        j.raw(",\"event_ordinal\":");
        j.number(b.event_ordinal);
        j.raw(",\"event_sequence\":");
        j.wide(b.sequence);
        j.raw(",\"local_command\":");
        j.number(b.local_command);
        j.raw(",\"global_command\":");
        j.number(b.global_command);
        j.raw(",\"requested_route\":");
        j.ascii(route_name(b.requested));
        j.raw(",\"effective_route\":");
        j.ascii(route_name(b.effective));
        j.raw(",\"packets\":");
        j.array(b.packets, [&](const auto &p) {
            j.raw("{\"occurrence\":");
            j.number(p.occurrence);
            j.raw(",\"vertex_begin\":");
            j.number(p.vertex_begin);
            j.raw(",\"vertex_count\":");
            j.number(p.vertex_count);
            j.raw(",\"index_begin\":");
            j.number(p.index_begin);
            j.raw(",\"index_count\":");
            j.number(p.index_count);
            j.raw(",\"triangle_begin\":");
            j.number(p.index_begin / 3);
            j.raw(",\"triangle_count\":");
            j.number(p.index_count / 3);
            j.raw("}");
        });
        j.raw("}");
    });
    j.raw("}");
}
} // namespace
WorldJoinFingerprint write_bound_displayed_recipe(const std::filesystem::path &path,
                                                  const ImmutablePolygonSubmission &displayed)
{
    if (path.empty())
        throw std::invalid_argument("displayed recipe output path is empty");
    std::ofstream file(path, std::ios::binary | std::ios::out | std::ios::noreplace);
    if (!file)
        throw std::runtime_error("cannot create displayed recipe output");
    HashBuffer buffer(*file.rdbuf());
    std::ostream stream(&buffer);
    write_displayed_recipe(stream, displayed);
    stream.flush();
    if (!stream)
        throw std::runtime_error("displayed recipe bound stream write failed");
    const auto fingerprint = buffer.fingerprint();
    file.close();
    if (!file)
        throw std::runtime_error("displayed recipe bound file close failed");
    if (fingerprint_world_join_file(path) != fingerprint)
        throw std::runtime_error("displayed recipe saved bytes differ from streamed export");
    return fingerprint;
}
bool write_world_join_sidecar(std::ostream &out, const WorldJoinFingerprint &fingerprint,
                              const ImmutablePolygonSubmission &displayed, const WorldJoinSidecarView &view)
{
    const bool matched = view.lookup == WorldJoinLookupStatus::matched && view.result && view.accepted_serial &&
                         view.record_tick && displayed && view.matched_identity.matches(displayed);
    const bool complete = matched && view.core_matches_displayed &&
                          view.result->status == WorldJoinStatus::derived_checked &&
                          view.result->accepted_serial == *view.accepted_serial &&
                          view.result->source_tick == *view.record_tick && *view.record_tick == displayed->source_tick;
    Json j(out);
    j.raw("{\"schema\":\"sgn.displayed-world-join.v1\",\"algorithm\":\"DERIVED_CHECKED\",\"complete\":");
    j.flag(complete);
    j.raw(",\"lookup_status\":");
    j.ascii(view.lookup == WorldJoinLookupStatus::matched && !matched ? "identity_mismatch" : lookup_name(view.lookup));
    j.raw(",\"recipe\":{\"sha256\":");
    j.hex(fingerprint.sha256);
    j.raw(",\"bytes\":");
    j.wide(fingerprint.bytes);
    j.raw("}");
    j.raw(",\"selection\":{\"first_accepted_serial\":");
    j.wide(view.selection.first);
    j.raw(",\"count\":");
    j.number(view.selection.count);
    j.raw("}");
    j.raw(",\"displayed\":");
    if (displayed)
    {
        j.raw("{\"source_tick\":");
        j.wide(displayed->source_tick);
        j.raw(",\"sequence_before\":");
        j.wide(displayed->initial.operation_sequence);
        j.raw(",\"sequence_after\":");
        j.wide(displayed->final.operation_sequence);
        j.raw(",\"commands\":");
        j.number(displayed->commands);
        j.raw(",\"events\":");
        j.number(displayed->events);
        j.raw(",\"segments\":");
        j.number(displayed->segments.size());
        j.raw("}");
    }
    else
        j.raw("null");
    j.raw(",\"record\":");
    if (matched)
    {
        j.raw("{\"accepted_serial\":");
        j.wide(*view.accepted_serial);
        j.raw(",\"source_tick\":");
        j.wide(*view.record_tick);
        j.raw(",\"core_status\":");
        j.ascii(core_name(view.result->status));
        j.raw(",\"core_error\":");
        j.number(static_cast<unsigned>(view.result->error));
        j.raw(",\"core_matches_displayed\":");
        j.flag(view.core_matches_displayed);
        j.raw("}");
    }
    else
        j.raw("null");
    const auto &c = view.counters;
    j.raw(",\"retention\":{\"cells\":3,\"reserved\":");
    j.wide(c.reserved);
    j.raw(",\"committed\":");
    j.wide(c.committed);
    j.raw(",\"allocation_failures\":");
    j.wide(c.allocation_failures);
    j.raw(",\"capacity_failures\":");
    j.wide(c.capacity_failures);
    j.raw(",\"order_failures\":");
    j.wide(c.order_failures);
    j.raw(",\"nonrendered\":");
    j.wide(c.nonrendered);
    j.raw(",\"aborted\":");
    j.wide(c.aborted);
    j.raw(",\"identity_failures\":");
    j.wide(c.identity_failures);
    j.raw(",\"join_failures\":");
    j.wide(c.join_failures);
    j.raw("}");
    j.raw(",\"geometry\":");
    if (complete)
        result(j, *view.result);
    else
        j.raw("null");
    j.raw("}\n");
    if (!out)
        throw std::runtime_error("world join sidecar write failed");
    return complete;
}
bool export_displayed_world_join(const std::filesystem::path &recipe_path, const std::filesystem::path &sidecar_path,
                                 const ImmutablePolygonSubmission &displayed, const WorldJoinSidecarView &view)
{
    if (sidecar_path.empty() || recipe_path == sidecar_path)
        throw std::invalid_argument("world join requires distinct nonempty output paths");
    const auto fingerprint = write_bound_displayed_recipe(recipe_path, displayed);
    std::ofstream sidecar(sidecar_path, std::ios::binary | std::ios::out | std::ios::noreplace);
    if (!sidecar)
        throw std::runtime_error("cannot create world join sidecar");
    const bool complete = write_world_join_sidecar(sidecar, fingerprint, displayed, view);
    sidecar.close();
    if (!sidecar)
        throw std::runtime_error("world join sidecar close failed");
    return complete;
}
} // namespace sgn::diagnostics
