#pragma once
#include "sgn/host_wide_overlay.hpp"

// NEW HOST presentation (wide_hud, R20): the view edge each 2D race overlay
// group is pinned to on a non-4:3 canvas. Derived from the original logical
// layout coordinates (pinned PE; see retail_race_hud.cpp 4478C0/449E80 and the
// draw leaves). mx/my = HUD margins 6244FC/624500 (normally 0).
namespace sgn::host_wide_overlay::race
{
using H = Horizontal;
using V = Vertical;

// 4478C0 element1 -> computed element0 (+2C/+30): x=640-(mx+166.4 [43266667]),
// y=480-(my+106.4 [42D4CCCD]), slides in +256 from the right. Speed gauge
// 449720/4497B0, digits 4492D0, energy bar 4493E0, needle 4495E0 and the demo
// logo 40DE80 (element0 -48/-80) are drawn relative to it.
inline constexpr Anchor gauge = Anchor::at(H::right, V::bottom);
// 4478C0 element2 -> element1 (+50/+54): x=255.2 [437F3333], y=my+6, slides
// in -96 from the top. Score counter 448AB0, flash 448360, stars 448480.
inline constexpr Anchor score = Anchor::at(H::center, V::top);
// 44A800 progress bar: literal x=16 [41800000] width 224 [43600000], y from
// element1 (+54)+8; icons at delta*1.12+122 [42F40000] -> left edge.
inline constexpr Anchor progress = Anchor::at(H::left, V::top);
// 4478C0 elements3..5 -> elements2..4 (+74/+78,+98/+9C,+BC/+C0):
// x=570.2 [440ECCCD]-mx, y=my+3.2, slide +160/+256/+352 from the right.
// Lap timer / record timer 4490E0, lap boxes 44A5C0.
inline constexpr Anchor timers = Anchor::at(H::right, V::top);
// 4478C0 element6 -> element5 (+E0/+E4): x=mx+3.2 [404CCCCD],
// y=my+19.2 [4199999A], slides -192 from the left. Position 448A70/4487D0,
// remaining-time digits 44AAD0 (x from +E0 plus mx).
inline constexpr Anchor position = Anchor::at(H::left, V::top);
// 4478C0 element7 -> element6 (+104/+108): x=mx+35.2 [420CCCCD],
// y=448-my, slides -96 from the left. Gear 4485E0.
inline constexpr Anchor gear = Anchor::at(H::left, V::bottom);
// Centered overlays: wrong way 449850 (quad x=270.4 [43873333] w=99.2, text
// x=320), countdown 449B50 (x=217.6 [4359999A] w=204.8), finish 449D70 /
// start 44A3D0 (x=166.4 [43266666] w=307.2), notice panels 4499A0 (sweep
// toward each other: 5.33*t-66 and 574-5.33*t), waiting text 449CE0
// (320,240), status texts 442520/442590 (x=320 y=208/272/240), score popups
// 401B00 (401220 layout: 320-w, 240-h/2, then 2D motion).
inline constexpr Anchor center = Anchor::at(H::center, V::middle);
// 442E30 notices: x=320, y=480 [43F00000]-animation (rises from the bottom).
// 40DF90 demo bottom line: x=320+mx, y=448 [43E00000]-my.
inline constexpr Anchor bottom_center = Anchor::at(H::center, V::bottom);
// 40DF90 demo top line: x=320+mx, y=my.
inline constexpr Anchor top_center = Anchor::at(H::center, V::top);
// Pause menu 4473F0: panel x=(640-w)/2, y=120..360, header/rows/meters
// relative to it, footer (320,80). Results 449FB0: one composition (heading
// element8 12.8/25.6, title element9 624/32 right-aligned, rows 102.4.., prompt
// element10 320/440). Each kept together as one centered 4:3 composition;
// the scope only widens the clip so slide-ins enter at the screen edge.
// The loading picture 445800 (tiles x=64..576, y=0..480) needs no scope:
// model A already centres untouched 2D (sides stay black as the original's
// 64-unit margins).
inline constexpr Anchor panel = Anchor::at(H::center, V::middle);
// Edge-to-edge flat fills are stretched onto the widened view (fill): pause
// age fade 442200 (0,0,640x480, constant texel 128/128 of material 10) and
// controller overlay fade 4428A0 (0,0,640x480, constant texel 384|128/128).
// Otherwise they would only darken the centred 4:3 area.
inline constexpr Anchor fade = Anchor::fill();
// The view frame 431830 is drawn from host_wide_overlay::view_frame (the
// widened view rectangle), not through an anchor.
} // namespace sgn::host_wide_overlay::race
