#pragma once
#include "sgn/world_packet_join.hpp"
#include "sgn/world_join_options.hpp"
#include <utility>
#include <limits>

namespace sgn::diagnostics
{
// Metadata only. No recipe, texture bank or strong device-frame ownership.
class WorldJoinRecipeIdentity
{
    struct Segment
    {
        std::weak_ptr<const CapturedPolygonDeviceFrame> frame;
        std::uint64_t before{}, after{};
    };
    std::array<Segment, 16> segments_{};
    std::weak_ptr<const PolygonDeviceIdentity> device_;
    std::size_t count_{}, commands_{}, events_{}, payload_{};
    std::uint64_t tick_{}, before_{}, after_{};
    bool bound_{};

  public:
    bool bind(const ImmutablePolygonSubmission &recipe) noexcept
    {
        *this = {};
        if (!recipe || !recipe->device || recipe->segments.empty() || recipe->segments.size() > segments_.size())
            return false;
        for (std::size_t i = 0; i < recipe->segments.size(); ++i)
        {
            const auto &frame = recipe->segments[i].device_frame;
            if (!frame || !frame->polygon_commands)
                return false;
            segments_[i] = {frame, frame->initial.operation_sequence, frame->final.operation_sequence};
        }
        device_ = recipe->device;
        count_ = recipe->segments.size();
        commands_ = recipe->commands;
        events_ = recipe->events;
        payload_ = recipe->payload_bytes;
        tick_ = recipe->source_tick;
        before_ = recipe->initial.operation_sequence;
        after_ = recipe->final.operation_sequence;
        bound_ = true;
        return true;
    }
    bool matches(const ImmutablePolygonSubmission &recipe) const noexcept
    {
        if (!bound_ || !recipe || count_ != recipe->segments.size() || tick_ != recipe->source_tick ||
            commands_ != recipe->commands || events_ != recipe->events || payload_ != recipe->payload_bytes ||
            before_ != recipe->initial.operation_sequence || after_ != recipe->final.operation_sequence ||
            device_.owner_before(recipe->device) || recipe->device.owner_before(device_))
            return false;
        const auto device = device_.lock();
        if (!device || device.get() != recipe->device.get())
            return false;
        for (std::size_t i = 0; i < count_; ++i)
        {
            const auto &actual = recipe->segments[i].device_frame;
            const auto &expected = segments_[i];
            const auto locked = expected.frame.lock();
            if (!actual || !locked || locked.get() != actual.get() || expected.frame.owner_before(actual) ||
                actual.owner_before(expected.frame) || actual->initial.operation_sequence != expected.before ||
                actual->final.operation_sequence != expected.after)
                return false;
        }
        return true;
    }
};
struct WorldJoinRetentionCounters
{
    std::uint64_t reserved{}, committed{}, allocation_failures{}, capacity_failures{}, order_failures{}, nonrendered{},
        aborted{}, identity_failures{}, join_failures{};
};
enum class WorldJoinLookupStatus
{
    matched,
    no_displayed,
    not_retained,
    ambiguous,
    foreign_thread
};

// Three cells cover the current front, previous completed back (which may flip
// during the next source call), and a pending diagnostic. Never pick by content
// or latest tick. The Record is SessionWorldJoin in the real application.
template <class Record> class WorldJoinRetention final
{
    static_assert(noexcept(std::declval<Record &>().abort()));
    static_assert(noexcept(std::declval<Record &>().finish(std::declval<const ImmutablePolygonSubmission &>())));

  public:
    struct Token
    {
        std::size_t cell{};
        std::uint64_t generation{};
    };
    struct Entry
    {
        std::uint64_t serial{}, tick{}, generation{};
        std::unique_ptr<Record> record;
        WorldJoinRecipeIdentity identity;
    };
    struct Lookup
    {
        WorldJoinLookupStatus status;
        const Entry *entry{};
    };

  private:
    std::array<Entry, 3> cells_{};
    std::optional<Token> pending_, last_back_;
    std::uint64_t generation_{};
    WorldJoinSelection selection_;
    WorldJoinRetentionCounters counters_;
    std::thread::id owner_{std::this_thread::get_id()};
    bool owner() const noexcept
    {
        return owner_ == std::this_thread::get_id();
    }
    bool valid(Token token) const noexcept
    {
        return token.cell < cells_.size() && cells_[token.cell].record &&
               cells_[token.cell].generation == token.generation;
    }
    bool is_pending(Token token) const noexcept
    {
        return pending_ && pending_->cell == token.cell && pending_->generation == token.generation && valid(token);
    }

  public:
    explicit WorldJoinRetention(WorldJoinSelection selection) noexcept : selection_(selection) {}
    WorldJoinRetention(const WorldJoinRetention &) = delete;
    WorldJoinRetention &operator=(const WorldJoinRetention &) = delete;
    const WorldJoinSelection &selection() const noexcept
    {
        return selection_;
    }
    const WorldJoinRetentionCounters &counters() const noexcept
    {
        return counters_;
    }
    Lookup lookup(const ImmutablePolygonSubmission &displayed) const noexcept
    {
        if (!owner())
            return {WorldJoinLookupStatus::foreign_thread};
        if (!displayed)
            return {WorldJoinLookupStatus::no_displayed};
        const Entry *found = nullptr;
        for (const auto &cell : cells_)
            if (cell.record && cell.identity.matches(displayed))
            {
                if (found)
                    return {WorldJoinLookupStatus::ambiguous};
                found = &cell;
            }
        return found ? Lookup{WorldJoinLookupStatus::matched, found} : Lookup{WorldJoinLookupStatus::not_retained};
    }
    template <class Factory>
    std::optional<Token> reserve(std::uint64_t serial, std::uint64_t tick, const ImmutablePolygonSubmission &displayed,
                                 Factory &&factory) noexcept
    {
        if (!owner() || !selection_.contains(serial))
            return {};
        if (pending_)
        {
            ++counters_.order_failures;
            return {};
        }
        for (const auto &cell : cells_)
            if (cell.record && cell.serial == serial)
            {
                ++counters_.order_failures;
                return {};
            }
        std::optional<std::size_t> victim;
        for (std::size_t i = 0; i < cells_.size(); ++i)
        {
            const auto &cell = cells_[i];
            const bool back = last_back_ && last_back_->cell == i && valid(*last_back_);
            if (back || (cell.record && cell.identity.matches(displayed)))
                continue;
            if (!victim || !cell.record || cell.generation < cells_[*victim].generation)
                victim = i;
            if (!cell.record)
                break;
        }
        if (!victim || generation_ == (std::numeric_limits<std::uint64_t>::max)())
        {
            ++counters_.capacity_failures;
            return {};
        }
        try
        {
            auto record = std::forward<Factory>(factory)();
            if (!record)
            {
                ++counters_.allocation_failures;
                return {};
            }
            const auto gen = ++generation_;
            cells_[*victim] = {serial, tick, gen, std::move(record), {}};
            pending_ = Token{*victim, gen};
            ++counters_.reserved;
            return pending_;
        }
        catch (...)
        {
            ++counters_.allocation_failures;
            return {};
        }
    }
    Record *pending(Token token) noexcept
    {
        return owner() && is_pending(token) ? cells_[token.cell].record.get() : nullptr;
    }
    void cancel(std::optional<Token> token) noexcept
    {
        if (!owner() || !token || !is_pending(*token))
            return;
        cells_[token->cell].record->abort();
        cells_[token->cell] = {};
        pending_.reset();
        ++counters_.aborted;
    }
    // Called for every actual capture while diagnostics are enabled, including
    // invocations outside the requested window. A null recipe retains old roles.
    void complete(std::optional<Token> token, std::uint64_t actual_serial,
                  const ImmutablePolygonSubmission &recipe) noexcept
    {
        if (!owner())
            return;
        if (!token)
        {
            if (pending_)
            {
                ++counters_.order_failures;
                return;
            }
            if (recipe)
            {
                last_back_.reset();
                for (std::size_t i = 0; i < cells_.size(); ++i)
                    if (cells_[i].record && cells_[i].identity.matches(recipe))
                        last_back_ = Token{i, cells_[i].generation};
            }
            return;
        }
        if (!is_pending(*token))
        {
            ++counters_.order_failures;
            return;
        }
        auto &entry = cells_[token->cell];
        if (actual_serial != entry.serial || (recipe && recipe->source_tick != entry.tick))
        {
            ++counters_.identity_failures;
            cancel(token);
            return;
        }
        const bool joined = entry.record->finish(recipe);
        if (!recipe)
        {
            entry = {};
            pending_.reset();
            ++counters_.nonrendered;
            return;
        }
        if (!entry.identity.bind(recipe))
        {
            entry = {};
            pending_.reset();
            last_back_.reset();
            ++counters_.identity_failures;
            return;
        }
        if (!joined)
            ++counters_.join_failures;
        last_back_ = *token;
        pending_.reset();
        ++counters_.committed;
    }
};
} // namespace sgn::diagnostics
