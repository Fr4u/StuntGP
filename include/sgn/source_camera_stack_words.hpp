#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <thread>

namespace sgn::retail
{
// NEW host representation of a bounded, retained x86 stack fragment. This
// owns call-link words, including unknown words; it is NOT an x86 emulator or
// a claim about historical retail ESP. S is the actual relative call anchor
// before404299, reached from40D5C6. Normal scene entry and pause446AF5
// have separate retained relative fragments: their absolute stack anchors
// must not be assumed equal. Within each proven caller lineage, the four
// camera calls reuse that relative depth. No scene reset/free recycles them.
// These domains are NEW provenance, never historical numeric ESP values.
// Payloads cannot escape their owning RaceSessionPlatform.
//
// Source camera/contact vectors stay16 bytes. Relocations travel as opaque
// DWORD tokens in their W lane, just as the source MOVs transport the address.
// A token is never a floating point value, retail address, bank address token,
// host pointer, source allocation or emulated stack base. Its bits are normal
// binary32 so existing non-arithmetic float copies preserve them exactly.
// Only the proven XYZ-only graph admits these tokens; an arithmetic/address
// consumer requires a separate interpretation and must not cast them directly.
class SourceCameraStackWords final
{
  public:
    enum class CallDomain : std::uint8_t
    {
        normal_scene,
        pause_restart
    };

  private:
    using W = std::uint32_t;
    enum class Kind : std::uint8_t
    {
        unknown,
        literal,
        relative_address
    };
    struct Word
    {
        Kind kind{Kind::unknown};
        W literal{};
        std::int32_t relative{};
        static Word bits(W value)
        {
            return {Kind::literal, value, 0};
        }
        static Word address(std::int32_t offset)
        {
            return {Kind::relative_address, 0, offset};
        }
    };
    // Only this retained window is represented. Writes below it belong to
    // proven non-aliasing projection/normalization locals, not synthetic zero.
    static constexpr std::int32_t bottom = -128;
    static constexpr std::size_t count = 32, domain_count = 2;
    static constexpr W transport_base = 0xc1000000u;
    static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559);
    static_assert((transport_base & 0x7f800000u) != 0u &&
                  ((transport_base + static_cast<W>(count * domain_count) - 1u) & 0x7f800000u) != 0x7f800000u);
    const std::thread::id thread_{std::this_thread::get_id()};
    std::array<std::array<Word, count>, domain_count> words_{};
    std::array<std::array<bool, count>, domain_count> issued_{};
    bool active_{};
    void check_thread() const
    {
        if (std::this_thread::get_id() != thread_)
            throw std::logic_error("camera stack words accessed off their platform thread");
    }
    static std::size_t index(std::int32_t offset)
    {
        if (offset < bottom || offset >= 0 || offset % 4 != 0)
            throw std::logic_error("camera stack operation outside the proven relative word window");
        return static_cast<std::size_t>((offset - bottom) / 4);
    }
    W encode(std::size_t domain, std::int32_t offset)
    {
        const auto at = index(offset);
        issued_[domain][at] = true;
        return transport_base + static_cast<W>(domain * count + at);
    }

  public:
    SourceCameraStackWords() = default;
    SourceCameraStackWords(const SourceCameraStackWords &) = delete;
    SourceCameraStackWords &operator=(const SourceCameraStackWords &) = delete;
    SourceCameraStackWords(SourceCameraStackWords &&) = delete;
    SourceCameraStackWords &operator=(SourceCameraStackWords &&) = delete;

    class Frame final
    {
        SourceCameraStackWords &owner_;
        const std::size_t domain_;
        std::int32_t sp_{}; // relative to S, never native ESP
        bool recorded_{};
        bool admitted_{};
        friend class SourceCameraStackWords;
        explicit Frame(SourceCameraStackWords &owner, CallDomain domain)
            : owner_(owner), domain_(static_cast<std::size_t>(domain))
        {
            owner_.check_thread();
            if (domain_ >= domain_count)
                throw std::logic_error("unproved camera stack caller domain");
            if (owner_.active_)
                throw std::logic_error("nested camera stack fragment ownership");
            owner_.active_ = true;
        }
        void push(Word value)
        {
            sp_ -= 4;
            owner_.words_[domain_][index(sp_)] = value;
        }
        void discard(std::int32_t bytes)
        {
            sp_ += bytes;
            if (sp_ > 0)
                throw std::logic_error("camera stack fragment unwound beyond its anchor");
        }

      public:
        Frame(const Frame &) = delete;
        Frame &operator=(const Frame &) = delete;
        Frame(Frame &&) = delete;
        Frame &operator=(Frame &&) = delete;
        ~Frame()
        {
            owner_.active_ = false;
        } // Host unwind only; retained cells/tokens survive.

        void after_preset_404310(W selected)
        {
            owner_.check_thread();
            if (recorded_)
                throw std::logic_error("camera preset stack footprint recorded twice");
            recorded_ = true;
            if (selected < 1 || selected > 4)
                return; // No invented prior S-28 contents.
            admitted_ = true;
            push(Word::bits(0x0040429eu)); // 404299 CALL404310, tail-JMP to preset
            if (selected == 4)
            {
                push({});                      // 40D380 PUSH ESI: opaque, never observed here
                push(Word::bits(0x0040d388u)); // 40D383 CALL40D280
            }
            // 4028D0 before these operations cannot affect the later producer;
            // its call-link writes are overwritten by the following calls.
            push(Word::bits(0x4005c28fu)); // preset lens argument
            const W projection_return = selected == 2 ? 0x0040d266u : selected == 3 ? 0x0040d366u : 0x0040d2e6u;
            push(Word::bits(projection_return)); // CALL403310
            push({});                            // 403310 PUSH caller EBP; value is not consumed here
            const auto projection_ebp = sp_;     // 403311 MOV EBP,ESP: actual owned cell
            push(Word::bits(0x0040333au));       // 403335 CALL44F4C0
            push(Word::bits(0x0044f4cau));       // 44F4C5 CALL44EE10, no pushes in leaf
            discard(4);
            push(Word::bits(0x0044f4d4u));       // 44F4CF CALL44F4E0
            push(Word::address(projection_ebp)); // 44F4E0 PUSH EBP, observed producer
            // 44F4E0's body writes below its saved EBP. Its leave/return and
            // 44F4C0's tail44EE10 do not write this cell. Computation itself
            // has ALREADY run on the real projection owner, not in this model.
            discard(4);                    // 44F637 POP EBP
            discard(4);                    // 44F638 RET
            discard(4);                    // 44F4D9 tail44EE10 RET to40333A
            discard(4);                    // 40333A POP EBP
            discard(8);                    // 40333B RET4
            push(Word::bits(0x4005c28fu)); // later preset lens request
            const W lens_return = selected == 2 ? 0x0040d270u : selected == 3 ? 0x0040d370u : 0x0040d2f0u;
            push(Word::bits(lens_return)); // CALL402AB0
            push({});                      // 402AB0 saved EBP, reaches S-16 or S-24, not S-28
            discard(4);
            discard(8); // POP EBP, RET4
            if (selected == 4)
            {
                discard(4);
                discard(4);
            } // 40D2FA RET,40D39C POP ESI
            discard(4); // preset RET to40429E
            if (sp_ != 0)
                throw std::logic_error("camera preset stack footprint did not unwind");
            push(Word::bits(0x004042acu)); // 4042A7 CALL403F40
            push({});                      // 403F40 PUSH EBP -> EBP=S-8
            // The403F40 local at EBP-14 is not written in its body. All XYZ
            // stores and44ED40 accesses are disjoint, proven in packet evidence.
        }
        std::optional<W> read_404217()
        {
            owner_.check_thread();
            if (!recorded_)
                throw std::logic_error("camera W read before its actual preceding preset");
            if (!admitted_)
                return std::nullopt;
            const Word word = owner_.words_[domain_][index(sp_ - 0x14)]; // EBP-14 = S-28
            if (word.kind == Kind::literal)
                return word.literal;
            if (word.kind == Kind::relative_address)
                return owner_.encode(domain_, word.relative);
            return std::nullopt;
        }
    };
    Frame enter_404280_from_40d5c6(CallDomain domain = CallDomain::normal_scene)
    {
        return Frame(*this, domain);
    }

    // NEW diagnostics may resolve an already known relocation-domain word.
    // Do NOT use prefix matching to classify arbitrary asset scalar bits.
    // The owning Platform is part of identity; tokens from another platform
    // cannot be compared as source pointers or passed to SourceAllocationBank.
    std::int32_t relative_offset(W token) const
    {
        check_thread();
        if (token < transport_base || token - transport_base >= count * domain_count)
            throw std::logic_error("unissued camera stack relocation token");
        const auto at = static_cast<std::size_t>(token - transport_base);
        if (!issued_[at / count][at % count])
            throw std::logic_error("unissued camera stack relocation token");
        // Relative coordinate only: equal offsets in different domains are
        // not an assertion that their historical absolute addresses matched.
        return bottom + static_cast<std::int32_t>(at % count) * 4;
    }
};
} // namespace sgn::retail
