#pragma once
#include <array>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <thread>
#include <vector>
#include "sgn/source_contact_query_word.hpp"

namespace sgn::retail
{
// NEW bounded representation, not recovered numeric stack bytes. Only the
// 418A62 ->409B60 call context and the documented finite query/response closure
// are admitted. An unknown WORD denotes the value of one retained source stack
// cell at this call activation. Its DWORD serialization is never a scalar or
// address. No prefix classifier is provided: literal asset bits may coincide.
// Known source stores replace the cell; copies retain the original value ID.
class SourceContactStackWords final
{
    using W = std::uint32_t;
    using Value = SourceContactQueryWord;
    const std::thread::id thread_{std::this_thread::get_id()};
    W issued_{};
    std::uint64_t next_partial_{1};
    bool active_{};
    bool batch_{};
    // Same anchor as the original sixteen cells; deeper reached source
    // frames and call links share this owner, so overlapping stores survive.
    static constexpr int bottom = -0x400, top = 0x40;
    std::array<Value, (top - bottom) / 4> cells_{};
    std::vector<Value> relocations_;
    static constexpr W base = 0xc2000000u, capacity = 0x1d000000u;
    static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559);
    static_assert(((base + capacity - 1u) & 0x7f800000u) != 0x7f800000u);
    void check() const
    {
        if (std::this_thread::get_id() != thread_)
            throw std::logic_error("contact stack words accessed off the platform thread");
    }
    W unknown_value()
    {
        check();
        if (issued_ == capacity)
            throw std::overflow_error("contact opaque value ID domain exhausted");
        return base + issued_++;
    }
    W transport(Value &value)
    {
        if (value.kind == Value::Kind::number)
            return value.bits;
        if (value.kind == Value::Kind::unknown)
        {
            value = Value::opaque(unknown_value());
        }
        if (value.kind == Value::Kind::opaque)
            return value.transport; // known W=0 is still copied as0
        if (value.kind == Value::Kind::partial)
        {
            if (!value.identity)
                partial(value);
            for (const auto &prior : relocations_)
                if (prior.kind == Value::Kind::partial && prior.identity == value.identity)
                    return prior.transport;
            value.transport = unknown_value();
            relocations_.push_back(value);
            return value.transport;
        }
        for (const auto &prior : relocations_)
            if (prior.kind == value.kind && prior.slot == value.slot && prior.offset == value.offset &&
                prior.allocation == value.allocation && prior.identity == value.identity)
                return prior.transport;
        value.transport = unknown_value();
        relocations_.push_back(value);
        return value.transport;
    }
    void partial(Value &value)
    {
        if (next_partial_ == std::numeric_limits<std::uint64_t>::max())
            throw std::overflow_error("contact partial-value identity domain exhausted");
        value.identity = next_partial_++;
        value.transport = 0;
    }

  public:
    SourceContactStackWords() = default;
    SourceContactStackWords(const SourceContactStackWords &) = delete;
    SourceContactStackWords &operator=(const SourceContactStackWords &) = delete;
    class Batch final
    {
        SourceContactStackWords &owner_;
        friend class SourceContactStackWords;
        explicit Batch(SourceContactStackWords &o) : owner_(o)
        {
            o.check();
            if (o.batch_ || o.active_)
                throw std::logic_error("nested source vehicle contact batch");
            o.cells_ = {};
            o.batch_ = true;
        }

      public:
        Batch(const Batch &) = delete;
        Batch &operator=(const Batch &) = delete;
        Batch(Batch &&) = delete;
        Batch &operator=(Batch &&) = delete;
        ~Batch()
        {
            owner_.batch_ = false;
        }
    };
    // Wraps the actual418A90 call.4189E0 has no intervening CALL between
    // successive418A62 invocations. Returning no-contact tails preserve-24,
    // therefore the previous420FE0 producer is retained for the next vehicle.
    Batch enter_418a90_from_44285a()
    {
        return Batch(*this);
    }
    class Frame final
    {
        SourceContactStackWords &owner_;
        // EBP of the same-depth children = S-8. Cells[-40,-4] are retained
        // between their calls. Earlier unmodeled callers are unknown inputs,
        // not zero and not assumed to lack producers.
        friend class SourceContactStackWords;
        explicit Frame(SourceContactStackWords &o, bool invalidate_prior = false) : owner_(o)
        {
            o.check();
            if (o.active_)
                throw std::logic_error("nested source contact frame");
            if (!o.batch_ || invalidate_prior)
                o.cells_ = {};
            o.active_ = true;
        }
        static unsigned index(int offset)
        {
            if (offset < bottom || offset >= top || offset % 4)
                throw std::logic_error("contact stack operand outside the admitted frame");
            return static_cast<unsigned>((offset - bottom) / 4);
        }

      public:
        Frame(const Frame &) = delete;
        Frame &operator=(const Frame &) = delete;
        Frame(Frame &&) = delete;
        Frame &operator=(Frame &&) = delete;
        ~Frame()
        {
            owner_.active_ = false;
        }
        void store(int offset, W bits)
        {
            store_value(offset, Value::number(bits));
        }
        void store_opaque(int offset, W bits)
        {
            store_value(offset, Value::opaque(bits));
        }
        void store_value(int offset, const Value &value)
        {
            owner_.check();
            owner_.cells_[index(offset)] = value;
        }
        Value read_value(int offset)
        {
            owner_.check();
            auto &cell = owner_.cells_[index(offset)];
            if (cell.kind == Value::Kind::unknown)
                cell = Value::opaque(owner_.unknown_value());
            return cell;
        }
        W transport(Value &value)
        {
            owner_.check();
            return owner_.transport(value);
        }
        void partial(Value &value)
        {
            owner_.check();
            owner_.partial(value);
        }
        Value opaque()
        {
            owner_.check();
            return Value::opaque(owner_.unknown_value());
        }
        W copy(int offset)
        {
            owner_.check();
            auto &cell = owner_.cells_[index(offset)];
            return owner_.transport(cell);
        }
        // QueryWord keeps address/opaque/partial values distinct from numbers.
        // The query adapter checks known masks at actual numeric observers;
        // legacy copy() callers remain within their proved W-only closure.
    };
    // Intervening418830 children are native source receivers with unmodeled
    // x86 local bytes. Do not mistake earlier contact cells for AI producers.
    Frame enter_408000_from_418878()
    {
        return Frame(*this, true);
    }
    // Each stage is entered after native418830 receivers whose local stack
    // image is intentionally unknown. The reached source stores initialize
    // every scalar read by these five ground/stunt bodies.
    Frame enter_ground_stunt_from_418830()
    {
        return Frame(*this, true);
    }
    Frame enter_409b60_from_418a62()
    {
        return Frame(*this);
    }
    // NEW R17: 4188BC->405C80 and 4188C1->406190 are argless same-depth 418830
    // children after further native receivers; prior cells are unknown inputs.
    Frame enter_405c80_from_4188bc()
    {
        return Frame(*this, true);
    }
    Frame enter_406190_from_4188c1()
    {
        return Frame(*this, true);
    }
    // NEW R17: 41894A/41894F/418954/418959 (418830) and 4189C0/4189C5/4189D8
    //(418970, the last a JMP) trail/shadow children follow native receivers;
    // every prior cell is an unknown input, never a producer.
    Frame enter_trail_shadow_from_418830_418970()
    {
        return Frame(*this, true);
    }
};
} // namespace sgn::retail
