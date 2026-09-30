#pragma once
#include "sgn/polygon_submission.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

// NEW HOST widescreen presentation of the frontend (wide_menu, R20). This is
// not recovered game behaviour and it never writes a source owner.
//
// The source menu (441D70 -> 43D250 -> 43F6B0 + 437EC0) keeps drawing its
// unchanged 4:3 logical canvas (640x480 menu units, mapped to the captured
// viewport by 432E00 with ui_scale). On a wider (or taller) output the shared
// wide layout (sgn/host_wide_layout.hpp, owned by wide_layout) shows the
// canvas centered and unscaled and makes the region beyond its edges visible.
// This module fills that region the way the source itself would with a wider
// screen rectangle, on a presentation COPY of the sealed capture made when the
// recipe is rasterized (RendererLoadingSurfaces stage/restore). The retained
// recipes, the capture, diagnostics/front snapshots and every source owner stay
// exactly as sealed; the copy uses the layout current at rasterization:
//   tiled        the scrolling background tiles of 438830 whose rectangle
//                reaches a canvas edge (43F6B0 layout 1 full screen, the four
//                43FEB0 "outside" rectangles, 43FF90 full screen). The outer
//                edge of such a quad is moved outward; its texture coordinate
//                continues with the exact 438830 period (1/64 per menu unit,
//                u = (x*0.015625 - scroll)*1024 / 1024), i.e. the pattern keeps
//                tiling (texture addressing is the captured source state).
//   screen_frame the full-screen frame 439450(layer 1, descriptor 113, 0..640
//                x 0..480) of 43F6B0. Every vertex moves outward by the side
//                it lies on (left/right of the canvas center, above/below), so
//                corners follow the output corners and the 8-texel middle
//                slices stretch exactly as 439520 stretches them for any rect.
//   fill         the full-screen 4428A0 transition overlay of 441D70 (one
//                0..640 x 0..480 quad sampling a constant texel region with
//                its fade alpha). Its canvas-edge vertices move outward, so
//                the fade/wipe covers the whole output; texture unchanged.
// Everything else (pages, panels, text, sprites, car/track picture strips) is
// presented exactly as captured: the 4:3 content stays centered, unstretched.
// Vertex counts, commands, device events and states are unchanged, so the
// copy passes the same submission validation. With no extension (4:3 output,
// --aspect-4x3 / SGN_ASPECT=4x3, or untagged frames such as race frames) the
// original shared submission is returned unchanged (same pointer, same bytes).
//
// Tags: the source-side hooks record which queued 438500 commands belong to a
// tagged call (QueueScope), which captured draw a command produced
// (ConsumeScope around 432E00 in 437F40) and the capture ordinal of every
// accepted draw (note_draw from the presentation backend). All functions are
// owner (source draw) thread only and noexcept; a bookkeeping failure only
// marks the frame inconsistent (then it is presented exactly as captured).
namespace sgn::host_wide_menu
{

enum class Kind : std::uint8_t
{
    none = 0,
    tiled = 1,
    screen_frame = 2,
    fill = 3
};

// ---- source-side hooks (NEW HOST bookkeeping, no source state) ----
// Kind of the 438370 -> 438500 commands queued while alive (43F6B0/43FF90).
class QueueScope
{
    Kind previous_;

  public:
    explicit QueueScope(Kind) noexcept;
    ~QueueScope();
    QueueScope(const QueueScope &) = delete;
    QueueScope &operator=(const QueueScope &) = delete;
};
// 438500 after 438590 linked the command (actual payload identity 5DA374+n).
void queued_command(std::uint32_t command_identity) noexcept;
// 437F40 tag-1 branch around 432E00: the draw it produces carries the kind.
class ConsumeScope
{
    Kind previous_;

  public:
    explicit ConsumeScope(std::uint32_t command_identity) noexcept;
    ~ConsumeScope();
    ConsumeScope(const ConsumeScope &) = delete;
    ConsumeScope &operator=(const ConsumeScope &) = delete;
};
// 437EC0 after 437ED0 consumed every list (437E80 then clears them).
void consumed_all() noexcept;
// Direct source draws (not queued through 438500) made while alive: the
// 441D70 calls of 442A00/442A20 (4428A0 overlay quad).
class DrawScope
{
    Kind previous_;

  public:
    explicit DrawScope(Kind) noexcept;
    ~DrawScope();
    DrawScope(const DrawScope &) = delete;
    DrawScope &operator=(const DrawScope &) = delete;
};

// ---- presentation backend (BeginScene/draws/EndScene of every source frame) ----
struct FrameTags
{
    bool consistent{true};
    std::size_t tagged{};    // draws with a kind other than none
    std::vector<Kind> draws; // one entry per accepted capture draw, capture order
};
using ImmutableFrameTags = std::shared_ptr<const FrameTags>;
void begin_frame() noexcept;
void note_draw(std::size_t vertices, bool accepted) noexcept;
ImmutableFrameTags end_frame() noexcept; // null when nothing was tagged
void abandon_frame() noexcept;

// ---- presentation transform ----
// Visible region beyond the 4:3 canvas, per side, in 640x480 menu units
// (host_wide::Layout extend_x/extend_y; converted per draw with ui_scale).
struct Extension
{
    double left{}, right{}, top{}, bottom{};
    bool empty() const noexcept
    {
        return !(left > 0) && !(right > 0) && !(top > 0) && !(bottom > 0);
    }
};
// Presentation copy of a sealed source frame (see above). Returns `submission`
// itself when there is nothing to extend or the tags do not match the capture.
ImmutablePolygonSubmission widen_submission(const ImmutablePolygonSubmission &submission, const FrameTags *tags,
                                            const Extension &visible);

// ---- recipe association (backend EndScene -> surfaces stage/restore) ----
// Associates the sealed submission with its tags (null tags: nothing to do).
// A small ring of weak references; the retained front/back recipes are always
// among the most recent entries. Owner thread; noexcept (failure: as captured).
void remember(const ImmutablePolygonSubmission &sealed, ImmutableFrameTags tags) noexcept;
// Presentation copy of a retained recipe for the current host_wide layout.
// Null means "rasterize `recipe` itself" (no tags, not wide, --aspect-4x3,
// unknown recipe, or any failure). Never throws; never modifies the recipe.
ImmutablePolygonSubmission presentation_copy(const CapturedPolygonSubmission *recipe) noexcept;
// NEW HOST (R30): presentation copies reuse the storage of earlier ones (host_wide_menu.cpp, Recycler).
// Frees that storage (a renderer suspension or recreation calls it; idle copies never hold resources).
void release_recycled_storage() noexcept;

// ---- NEW HOST (widescreen R37): frontend clip reaching the real screen edges ----
// 438370 rejects/clips every queued menu rectangle against the frontend clip
// 5DA360 (438190 clamps it to 0..640 x 0..480). On a wide canvas that clip is an
// invisible wall at the 4:3 edges: rows, icons, the title logo and the language
// flags sliding in or out were cut there. When the source clip is exactly the
// full 640x480 frame and the output is wide, 438370 uses these HOST bounds
// instead (canvas edges, in menu units); any narrower source clip (panels,
// windows, strips) is used exactly as stored. The stored clip words are never
// written. Identity (returns false) in 4:3 / original_4x3 mode.
struct FrontendClipBounds
{
    float left{}, top{}, right{640.0f}, bottom{480.0f};
};
bool widened_frontend_clip(const std::array<std::uint32_t, 4> &source_clip, FrontendClipBounds &out) noexcept;
// Caps the widened extension (menu units per side) for the rectangles queued
// while alive (the title logo carousel parks its second copy 111.5 units beyond
// the 4:3 edge; on ultra-wide outputs it must stay hidden). Owner thread only.
class ClipLimitScope
{
    float previous_;

  public:
    explicit ClipLimitScope(float max_extension) noexcept;
    ~ClipLimitScope();
    ClipLimitScope(const ClipLimitScope &) = delete;
    ClipLimitScope &operator=(const ClipLimitScope &) = delete;
};
} // namespace sgn::host_wide_menu

