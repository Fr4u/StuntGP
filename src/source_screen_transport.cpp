#include "sgn/source_screen_transport.hpp"
#include "sgn/source_transport_storage.hpp"
#include "sgn/source_ring_x87.hpp"
#include "sgn/bit_reinterpret.hpp"
#include <bit>
#include <cstring>
#include <limits>
namespace sgn::retail
{
using W = std::uint32_t;
using Op = RingX87Op;
SourceTransportBuffer::SourceTransportBuffer(SourceWorldBufferBinding b)
    : binding_(std::move(b)), owner_(binding_.platform.lock())
{
    if (!owner_)
        throw SourceAllocationError(0x425838, "source buffer owner expired");
}
W SourceTransportBuffer::extent(W at, W n, W site) const
{

    const auto count = binding_.byte_count == std::numeric_limits<W>::max() ? binding_.bytes : binding_.byte_count;
    if (!binding_.bank || at > count || n > count - at || binding_.byte_offset > binding_.bytes ||
        count > binding_.bytes - binding_.byte_offset)
        throw SourceAllocationError(site, "reached source buffer access leaves captured allocation extent");
    if (binding_.bank->allocation_size(binding_.allocation) != binding_.bytes)
        throw SourceAllocationError(site, "source buffer generation/extent changed");
    return binding_.byte_offset + at;
}
SourceTransportWord SourceTransportBuffer::load(W at, W site) const
{
    const auto offset = extent(at, 4, site);
    return binding_.bank->load_transport_word(binding_.allocation, offset, site);
}
void SourceTransportBuffer::store(W at, const SourceTransportWord &value, W site)
{
    const auto offset = extent(at, 4, site);
    binding_.bank->store_transport_word(binding_.allocation, offset, value, site);
}
void SourceTransportBuffer::known(W at, std::span<const std::uint8_t> bytes, W site)
{
    const auto offset = extent(at, static_cast<W>(bytes.size()), site);
    binding_.bank->write_known(binding_.allocation, offset, bytes, site);
}
std::span<std::uint8_t> SourceTransportBuffer::writable_known(W at, W n, W site)
{
    const auto offset = extent(at, n, site);
    return binding_.bank->readable_bytes(binding_.allocation, offset, n, site);
}
std::span<const std::uint8_t> SourceTransportBuffer::read(W at, W n, W site) const
{
    const auto offset = extent(at, n, site);
    return binding_.bank->readable_bytes(binding_.allocation, offset, n, site);
}
namespace
{
struct Memory
{
    std::function<SourceTransportWord(W, W)> load;
    std::function<void(W, const SourceTransportWord &, W)> store;
    std::function<std::uint8_t(W, W)> byte;
    std::function<void(W, std::uint8_t, W)> put_byte;
    W number(W at, W site) const
    {
        return load(at, site).numeric(site);
    }
    void set(W at, W value, W site) const
    {
        store(at, SourceTransportWord::known(value), site);
    }
};
struct Pointer
{
    Memory *m;
    W at;
    W number(W offset, W site) const
    {
        return m->number(at + offset, site);
    }
    void set(W offset, W value, W site) const
    {
        m->set(at + offset, value, site);
    }
};
void copy(Pointer dst, Pointer src, W dwords, W site)
{
    for (W i = 0; i < dwords; ++i)
    {
        const auto value = src.m->load(src.at + i * 4u, site);
        dst.m->store(dst.at + i * 4u, value, site);
    }
}
using sgn::bit_reinterpret::as_word;
W compare(RingX87 &fp, W value, float bound)
{
    fp.op(Op::fld32, value);
    fp.op(Op::fcomp32, as_word(bound));
    return fp.op(Op::sw);
}
// 4253E0. Original source W is never read or written by this receiver.
// Each numeric input is checked only at its reached FLD/FSUB/FADD; output
// fields committed before a later invalid input remain committed.
void edge(Pointer a, Pointer b, Pointer out, W plane, const ViewportState &v, RingX87 &fp)
{
    unsigned axis = 0;
    W first_site = 0, second_site = 0;
    switch (plane)
    {
    case 1:
        axis = 0;
        first_site = 0x4253fa;
        second_site = 0x425402;
        break;
    case 2:
        axis = 1;
        first_site = 0x42540c;
        second_site = 0x425414;
        break;
    case 4:
        axis = 2;
        first_site = 0x42541e;
        second_site = 0x425427;
        break;
    case 8:
        axis = 3;
        first_site = 0x425432;
        second_site = 0x42543b;
        break;
    default:
        fp.op(Op::fld32, plane);
        fp.op(Op::fld32, plane);
        break;
    }
    if (first_site)
    {
        const W off = axis < 2 ? 0u : 4u;
        fp.op(Op::fld32, a.number(off, first_site));
        fp.op(Op::fsub32, as_word(v.clip_screen[axis]));
        fp.op(Op::fld32, b.number(off, second_site));
        fp.op(Op::fsub32, as_word(v.clip_screen[axis]));
    }
    fp.op(Op::sub1);
    fp.op(Op::fld0);
    fp.op(Op::abs);
    fp.op(Op::fcomp32, 0x3f800000);
    if (fp.op(Op::sw) & 0x4100u)
    {
        fp.op(Op::pop0);
        fp.op(Op::pop0);
        fp.op(Op::fld32, 0x3f800000);
    }
    else
    {
        fp.op(Op::exchange);
        fp.op(Op::div1);
        fp.op(Op::chs);
        fp.op(Op::exchange);
        fp.op(Op::pop0);
    }
    W rounded_x = 0, rounded_y = 0;
    constexpr std::array<W, 5> off = {0, 4, 8, 16, 20};
    constexpr std::array<std::array<W, 5>, 5> sites = {{{0x425475, 0x425477, 0x42547e, 0x425480, 0x425483},
                                                        {0x425485, 0x425488, 0x42548d, 0x425490, 0x425493},
                                                        {0x425496, 0x425499, 0x42549e, 0, 0x4254a1},
                                                        {0x4254a4, 0x4254a7, 0x4254ac, 0, 0x4254af},
                                                        {0x4254b2, 0x4254b5, 0x4254ba, 0, 0x4254bd}}};
    for (unsigned i = 0; i < 5; ++i)
    {
        const auto &s = sites[i];
        fp.op(Op::fld32, b.number(off[i], s[0]));
        fp.op(Op::fsub32, a.number(off[i], s[1]));
        fp.op(Op::mul1);
        fp.op(Op::fadd32, a.number(off[i], s[2]));
        if (i == 0)
            rounded_x = fp.op(Op::fst32);
        if (i == 1)
            rounded_y = fp.op(Op::fst32);
        const auto value = fp.op(Op::fstp32);
        out.set(off[i], value, s[4]);
    }
    fp.op(Op::fmul32, 0x43800000);
    const W factor = fp.ftol();
    auto byte = [&](Pointer p, W at, W site) { return W(p.m->byte(p.at + at, site)); };
    auto publish = [&](W at, W value, W site) { out.m->put_byte(out.at + at, static_cast<std::uint8_t>(value), site); };
    auto product = [&](W a0, W b0) { return (b0 - a0) * factor; }; // IMUL32 and SAR32, modulo writes
    auto result = [](W prod, W first) { return W(std::bit_cast<std::int32_t>(prod) >> 8) + first; };
    W first = byte(a, 27, 0x4254cb), second = byte(b, 27, 0x4254d0);
    auto prod = product(first, second);
    first = byte(a, 27, 0x4254db);
    fp.op(Op::fld32, rounded_x);
    fp.op(Op::fcomp32, as_word(v.clip_screen[0]));
    W next = byte(a, 26, 0x4254ef);
    publish(27, result(prod, first), 0x4254f2);
    second = byte(b, 26, 0x4254f7);
    prod = product(next, second);
    first = byte(a, 26, 0x425502);
    next = byte(a, 25, 0x42550d);
    publish(26, result(prod, first), 0x425510);
    second = byte(b, 25, 0x425515);
    prod = product(next, second);
    first = byte(a, 25, 0x425520);
    next = byte(a, 24, 0x42552b);
    publish(25, result(prod, first), 0x42552e);
    second = byte(b, 24, 0x425533);
    out.set(28, 15, 0x42553e);
    prod = product(next, second);
    const auto left_status = fp.op(Op::sw);
    publish(24, result(prod, next), 0x425554);
    if (left_status & 0x100u)
        out.set(28, 14, 0x425559);
    else if (!(compare(fp, rounded_x, v.clip_screen[1]) & 0x4100u))
        out.set(28, 13, 0x425572);
    if (compare(fp, rounded_y, v.clip_screen[2]) & 0x100u)
        out.set(28, out.number(28, 0x425589) & ~4u, 0x42558e);
    else if (!(compare(fp, rounded_y, v.clip_screen[3]) & 0x4100u))
        out.set(28, out.number(28, 0x4255a8) & ~8u, 0x4255ad);
}
bool clip(Pointer polygon, const ViewportState &view)
{
    RingX87 fp;
    const auto count = polygon.number(12, 0x4255ea);
    if (count == 0)
        return true;
    W both = 15, either = 0;
    for (W i = 0; i < count; ++i)
    {
        Pointer p{polygon.m, polygon.at + 16u + i * 32u};
        if (compare(fp, p.number(0, 0x425610), view.clip_expanded[0]) & 0x100u)
            return false;
        if (!(compare(fp, p.number(0, 0x425623), view.clip_expanded[1]) & 0x4100u))
            return false;
        if (compare(fp, p.number(4, 0x425636), view.clip_expanded[2]) & 0x100u)
            return false;
        if (!(compare(fp, p.number(4, 0x42564a), view.clip_expanded[3]) & 0x4100u))
            return false;
        W code = 15;
        if (compare(fp, p.number(0, 0x42565e), view.clip_screen[0]) & 0x100u)
            code = 14;
        else if (!(compare(fp, p.number(0, 0x425679), view.clip_screen[1]) & 0x100u))
            code = 13;
        if (compare(fp, p.number(4, 0x42568d), view.clip_screen[2]) & 0x100u)
            code &= ~4u;
        else if (!(compare(fp, p.number(4, 0x4256a2), view.clip_screen[3]) & 0x100u))
            code &= ~8u;
        p.set(28, code, 0x4256b8);
        both &= code;
        either |= code;
    }
    if (both == 15)
        return true;
    if (either != 15)
        return false;
    SourceTransportStorage<std::array<std::uint8_t, 0x424>> stack;
    stack.unknown(0x4255e0, -0x424);
    auto scratch_extent = [](W at, W size, W site) {
        if (at > 0x400u || size > 0x400u - at)
            throw SourceAllocationError(site,
                                        "clip scratch aliases source control locals; additional binding is required");
    };
    Memory memory{[&](W at, W site) {
                      scratch_extent(at, 4, site);
                      return stack.load(at, site);
                  },
                  [&](W at, const SourceTransportWord &value, W site) {
                      scratch_extent(at, 4, site);
                      stack.store(at, value, site);
                  },
                  [&](W at, W site) {
                      scratch_extent(at, 1, site);
                      return stack.byte(at, site);
                  },
                  [&](W at, std::uint8_t value, W site) {
                      scratch_extent(at, 1, site);
                      stack.store_byte(at, value, site);
                  }};
    W input = 0x200, output = 0, n = count;
    copy({&memory, input}, {polygon.m, polygon.at + 16}, (count & 0x7ffffffu) << 3, 0x42570f);
    W plane = 1;
    for (unsigned pass = 0; pass < 4; ++pass)
    {
        if (!(both & plane))
        {
            W cursor = output;
            Pointer prev{&memory, input + (n << 5) - 32u};
            for (W i = 0; i < n; ++i)
            {
                Pointer next{&memory, input + i * 32u};
                const bool a = (prev.number(28, 0x425757) & plane) != 0, b = (next.number(28, 0x42575a) & plane) != 0;
                if (a)
                {
                    copy({&memory, cursor}, prev, 8, 0x42576e);
                    cursor += 32;
                    if (!b)
                    {
                        edge(prev, next, {&memory, cursor}, plane, view, fp);
                        cursor += 32;
                    }
                }
                else if (b)
                {
                    edge(prev, next, {&memory, cursor}, plane, view, fp);
                    cursor += 32;
                }
                prev = next;
            }
            const auto delta = cursor - output;
            n = static_cast<W>(std::bit_cast<std::int32_t>(delta) >> 5);
            if (n < 3 || n > 6)
                return false;
            std::swap(input, output);
        }
        both |= plane;
        plane <<= 1;
        if (both == 0x3f)
            break;
    }
    copy({polygon.m, polygon.at + 16}, {&memory, input}, (n & 0x7ffffffu) << 3, 0x425805);
    polygon.set(12, n, 0x42580d);
    return true;
}
} // namespace
bool append_source_screen_captured_425838(SourceWorldBufferBinding binding, W captured, W &count,
                                          const SourceScreenWordRead &input, const ViewportState &view)
{
    SourceTransportBuffer buffer(std::move(binding));
    const W offset = captured * 272u;
    for (W i = 0; i < 68; ++i)
    {
        const auto value = input(i * 4, 0x425853);
        buffer.store(offset + i * 4, value, 0x425853);
    }
    Memory memory{
        [&](W at, W site) { return buffer.load(at, site); },
        [&](W at, const SourceTransportWord &value, W site) { buffer.store(at, value, site); },
        [&](W at, W site) { return buffer.read(at, 1, site)[0]; },
        [&](W at, std::uint8_t value, W site) { buffer.known(at, std::span<const std::uint8_t>(&value, 1), site); }};
    const auto accepted = clip({&memory, offset}, view);
    if (accepted)
        ++count;
    return accepted;
}
bool append_source_screen_words_425830(SourceScreenPolygonQueue queue, const SourceScreenWordRead &input,
                                       const ViewportState &view)
{
    const W captured = queue.count;
    const auto binding = queue.base.source();
    if (!binding)
        throw SourceAllocationError(0x425838, "source trail copy requires actual published allocation");
    return append_source_screen_captured_425838(*binding, captured, queue.count, input, view);
}
} // namespace sgn::retail
