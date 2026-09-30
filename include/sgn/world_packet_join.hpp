#pragma once
#include "sgn/polygon_submission.hpp"
#include <string>
#include <thread>
#include <unordered_map>

namespace sgn::diagnostics
{
// NEW CPU diagnostic. No source owner, renderer, draw callback or bank retention.
enum class WorldJoinStatus
{
    disabled,
    collecting,
    derived_checked,
    incomplete,
    mismatch,
    no_recipe
};
enum class WorldJoinError
{
    none,
    order,
    foreign_thread,
    capacity,
    owner,
    packet,
    sink,
    sequence,
    event,
    draw,
    identity,
    allocation
};
enum class WorldHeaderKind
{
    model_table,
    model_part,
    sprite,
    strip
};
struct WorldHeaderOwner
{
    WorldHeaderKind kind{};
    std::optional<std::uint32_t> registry_token;
    std::optional<std::size_t> model_handle, lod, part;
    std::array<std::optional<unsigned>, 4> source_lods{};
    std::string source_name;
};
struct WorldHeaderRange
{
    WorldHeaderOwner owner;
    retail::WorldPolygonHeaderView headers;
};
struct WorldHeaderMembership
{
    std::size_t owner{}, offset{};
};
struct WorldJoinRange
{
    unsigned pool{};
    std::optional<std::size_t> first;
    std::size_t count{};
};
struct WorldJoinFrameView
{
    std::array<std::span<const retail::WorldPolygonPacket>, 4> pools;
    std::array<WorldJoinRange, 9> lists;
    std::array<WorldJoinRange, 7> clipping;
    std::span<const retail::WorldPolygonHeader> clip_headers; // Written slots only at submission.
};
using WorldJoinViewport = std::array<std::uint32_t, 38>;
WorldJoinViewport world_join_viewport(const retail::ViewportState &) noexcept;
struct WorldPacketOrigin
{
    std::size_t invocation{}, slot{}, membership_begin{}, membership_count{};
    unsigned pool{}, clipping_lists{};
    bool present{};
    retail::WorldPolygonHeader header{};
    std::uint32_t inside_mask{};
};
enum class WorldPreclipRelation
{
    unchanged,
    replaced,
    rejected,
    absent
};
struct WorldPacketOccurrence
{
    std::size_t origin{}, entry{}, membership_begin{}, membership_count{};
    unsigned list{};
    bool present{};
    retail::WorldPolygonHeader header{};
    std::optional<std::size_t> clip_header;
    WorldPreclipRelation relation{};
    std::uint32_t primary_token{};
};
struct WorldJoinInvocation
{
    unsigned controller{};
    WorldJoinViewport viewport{};
    std::uint64_t before{}, after{};
    std::uint32_t depth{}, quality{}, restriction{}, multitexture{}, alternate_token{};
    std::array<std::size_t, 9> list_begin{}, list_count{};
    std::array<WorldJoinRange, 9> ranges{};
    std::array<WorldJoinRange, 7> clipping{};
};
struct WorldPacketSpan
{
    std::size_t occurrence{}, vertex_begin{}, vertex_count{}, index_begin{}, index_count{};
};
struct WorldJoinedBatch
{
    std::size_t invocation{}, segment{}, event_ordinal{}, local_command{}, global_command{};
    std::uint64_t sequence{};
    unsigned list{};
    retail::PolygonSubmissionRoute requested{}, effective{};
    std::vector<WorldPacketSpan> packets;
};
struct WorldJoinResult
{
    WorldJoinStatus status{WorldJoinStatus::disabled};
    WorldJoinError error{WorldJoinError::none};
    std::uint64_t accepted_serial{}, source_tick{};
    std::vector<WorldHeaderOwner> owners;
    std::vector<WorldHeaderMembership> memberships;
    std::vector<WorldPacketOrigin> origins;
    std::vector<WorldPacketOccurrence> occurrences;
    std::vector<WorldJoinInvocation> invocations;
    std::vector<WorldJoinedBatch> batches;
};
struct WorldJoinLimits
{
    std::size_t origins{49152}, occurrences{110592}, memberships{196608}, views{4}, events{30000}, segments{16},
        owners{16384}, header_entries{262144};
};
class WorldPacketJoinCapture final
{
    WorldJoinLimits limits_;
    bool enabled_{}, started_{}, finished_{};
    enum class Phase
    {
        preclip,
        submission,
        submitted
    } phase_{Phase::preclip};
    std::thread::id thread_;
    const PolygonDeviceContext *device_{}; // Only between begin and finish/abort.
    std::weak_ptr<const PolygonDeviceIdentity> device_identity_;
    std::unordered_multimap<const void *, WorldHeaderMembership>
        header_index_; // Prepared before source work; aliases retained.
    std::vector<const void *> origin_headers_;
    std::array<const retail::WorldPolygonPacket *, 4> pending_pools_{};
    std::array<std::array<std::size_t, 3072>, 4> lookup_{};
    struct SegmentIdentity
    {
        std::weak_ptr<const CapturedPolygonDeviceFrame> owner;
        std::uint64_t before{}, after{};
    };
    std::vector<SegmentIdentity> segments_;
    std::uint64_t initial_sequence_{}, final_sequence_{};
    std::size_t command_count_{}, event_count_{};
    WorldJoinResult result_;
    bool fail(WorldJoinError, WorldJoinStatus = WorldJoinStatus::incomplete) noexcept;
    bool active() noexcept;
    bool memberships(retail::WorldPolygonHeaderRef, std::size_t &, std::size_t &) noexcept;
    void release_borrows() noexcept;
    bool resolve(const CapturedPolygonSubmission &);

  public:
    explicit WorldPacketJoinCapture(bool enabled = false, WorldJoinLimits = {}) noexcept;
    WorldPacketJoinCapture(const WorldPacketJoinCapture &) = delete;
    WorldPacketJoinCapture &operator=(const WorldPacketJoinCapture &) = delete;
    // Preparation may allocate; call before the selected source invocation.
    bool begin(std::uint64_t accepted_serial, std::uint64_t source_tick, const PolygonDeviceContext &,
               std::span<const WorldHeaderRange>) noexcept;
    // Bounded copies: no allocation, float evaluation or device/source calls.
    bool preclip(const WorldJoinFrameView &, unsigned controller) noexcept;
    bool before_submission(const WorldJoinFrameView &, const retail::PolygonSubmissionInputs &,
                           const retail::PolygonSubmissionSink &) noexcept;
    bool after_submission(const retail::PolygonSubmissionSink &) noexcept;
    // Offline CPU join after sealing, may allocate. No recipe/bank is retained.
    bool finish(const ImmutablePolygonSubmission &) noexcept;
    void abort(WorldJoinError = WorldJoinError::order) noexcept;
    bool matches_displayed(const ImmutablePolygonSubmission &) const noexcept;
    const WorldJoinResult &result() const noexcept
    {
        return result_;
    }
};
} // namespace sgn::diagnostics
