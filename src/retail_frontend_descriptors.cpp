#include "sgn/retail_frontend_descriptors.hpp"
#include "sgn/retail_frontend_state.hpp"
#include "sgn/retail_world_material_loading.hpp"
#include "retail_frontend_descriptor_tables.hpp"
#include <algorithm>
#include <bit>
#include <stdexcept>

namespace sgn::retail
{
namespace
{
struct Rectangle
{
    std::uint32_t left, top, right, bottom, material;
};
std::uint32_t branch(std::uint32_t id)
{
    using namespace descriptor_detail;
    // Equivalent to the source signed guards followed by unsigned index guards:
    // the caller has already restricted the accepted domain to1..386.
    if (id < 77)
        return targets_0[indices_0[id - 1]];
    if (id < 87)
        return 0x441774;
    if (id < 242)
        return targets_1[indices_1[id - 87]];
    if (id < 266)
        return 0x44195f;
    return targets_2[indices_2[id - 266]];
}
Rectangle rectangle(std::uint32_t id)
{
    // Each label names the actual entry in441380. Tables, guards and complete
    // assembly ranges are pinned in this packet; no inferred atlas convention.
    switch (branch(id))
    {
        // clang-format off: 441380 jump-table mapping, one source case label per line
    case 0x4413bb: return {1, 64, 255, 192, id + 0x10020};
    case 0x4413e5: { const auto n=id-13, x=(n&1)*128, y=(n/2)*64; return {x,y,x+128,y+64,0x10037}; }
    case 0x441419: { const auto n=id-21, x=(n&1)*128, y=(n/2)*64; return {x,y,x+128,y+64,0x10038}; }
    case 0x44144d: return {1, 1, 255, 255, id + 0x10038};
    case 0x441470: return {id*17-0x40c,193,id*17-0x3fd,208,0x1002b};
    case 0x44149e: return {id*17-0x390,193,id*17-0x381,208,0x10025};
    case 0x4414cc: return {id*17-0x3c3,210,id*17-0x3b4,225,0x10025};
    case 0x4414fa: return {id*17-0x3f6,227,id*17-999,242,0x10025};
    case 0x441528: return {65,id*16-0x3df,91,id*16-0x3d1,0x10025};
    case 0x441556: return {id*64-0xc3f,1,id*64-0xc01,63,0x1002b};
    case 0x441583: return {id*64-0xd3f,65,id*64-0xd01,127,0x1002b};
    case 0x4415b0: return {54,83,255,255,0x10051};
    case 0x4415d5: return {1,83,214,255,0x10052};
    case 0x4415fe: return {54,1,255,179,0x10053};
    case 0x441627: return {1,1,214,179,0x10054};
    case 0x44164d: return {54,83,255,255,0x10055};
    case 0x441672: return {1,83,214,255,0x10056};
    case 0x44169b: return {54,1,255,179,0x10057};
    case 0x4416c4: return {1,1,214,179,0x10058};
    case 0x4416ea: return {0,0,255,256,0x10059};
    case 0x44170d: return {1,0,256,256,0x1005a};
    case 0x44172f: return {0,0,255,256,0x1005b};
    case 0x441752: return {1,0,256,256,0x1005c};
    case 0x441774: { const auto n=id-77, x=(n&3)*64+1, y=(n/4)*64+1; return {x,y,x+62,y+62,0x1002c}; }
    case 0x4417e0: return {0,1,256,31,id+0xffcf};
    case 0x441807: return {0,1,256,63,0x1002a};
    case 0x44182d: return {0,0,256,256,id-91};
    case 0x44184a: return {id*64-0x1aff,129,id*64-0x1ac1,191,0x1002b};
    case 0x441877: return {129,193,135,199,0x10025};
    case 0x4418a0: return {id*64-0x1b7f,1,id*64-0x1b41,63,0x10025};
    case 0x4418cd: return {1,id*15-0x5ed,14,id*15-0x5e0,0x10025};
    case 0x4418fa: return {30,30,226,226,id+0xffe6};
    case 0x44193b: return {0,0,255,256,id+0xffe6};
    case 0x44195f: return {1,0,256,256,id+0xffe6};
    case 0x441982: return {0,0,256,256,id+0xff01};
    case 0x4419a2: { const auto n=id-310, x=(n&3)*64, y=(n/4)*64; return {x,y,x+64,y+64,0x10035}; }
    case 0x4419d2: { const auto n=id-326, x=(n&3)*64, y=(n/4)*64; return {x,y,x+64,y+64,0x10036}; }
    case 0x441a02: { const auto n=id-342, r=n%4, x=(r&1)*128+1, y=(r/2)*128+1; return {x,y,x+126,y+126,n/4+0x1002d}; }
    case 0x441a4e: { const auto n=id-355, r=n%4, x=(r&1)*128+1, y=(r/2)*128+1; return {x,y,x+126,y+126,n/4+0x10031}; }
    case 0x441a97: return {1,1,127,127,0x1011a};
    case 0x441ab6: return {1,1,127,127,id+0xffaa};
    case 0x441aca: return {1,1,255,255,id+0xffaa};
    default: throw std::logic_error("unhandled pinned frontend descriptor branch");
        // clang-format on
    }
}
void put(FrontendDescriptor row, std::size_t at, std::uint32_t bits)
{
    persistence_word(row, at, bits); // unaligned-safe exact little-endian writes
}
void put_float(FrontendDescriptor row, std::size_t at, float value)
{
    put(row, at, std::bit_cast<std::uint32_t>(value));
}
} // namespace
FrontendDescriptorBank frontend_descriptor_bank(FrontendSourceStorage &frontend)
{
    const auto bytes = frontend.source_bytes(0x608d88, frontend_descriptor_bytes);
    return FrontendDescriptorBank{bytes.data(), frontend_descriptor_bytes};
}
std::uint32_t frontend_descriptor_absolute_43d9b0(std::uint32_t value) noexcept
{
    return std::bit_cast<std::int32_t>(value) < 0 ? 0u - value : value;
}
std::uint32_t initialize_frontend_descriptor_441380(FrontendDescriptor row, std::uint32_t id)
{
    if (id < 1 || id > 386)
        return 0; // 44138A..4413A6 /4417AB..4417CB /441923..441926
    const auto r = rectangle(id);
    // 441AE8..441B1A. Valid input guards bound each integer coordinate to0..256.
    // FILD then FMUL4 then FSTP32 is exact throughout this domain.
    put_float(row, 0x14, static_cast<float>(r.left) * 4.0f);
    put_float(row, 0x18, static_cast<float>(r.top) * 4.0f);
    put_float(row, 0x1c, static_cast<float>(r.right) * 4.0f);
    put_float(row, 0x20, static_cast<float>(r.bottom) * 4.0f);
    const auto width = frontend_descriptor_absolute_43d9b0(r.right - r.left);     // 441B1D
    put_float(row, 0x0c, static_cast<float>(std::bit_cast<std::int32_t>(width))); // 441B2D..441B30
    const auto height = frontend_descriptor_absolute_43d9b0(r.bottom - r.top);    // 441B33
    put(row, 0x24, r.material); // 441B3B, raw32 source identity, not a native pointer/physical slot
    put_float(row, 0x10, static_cast<float>(std::bit_cast<std::int32_t>(height))); // 441B3E..441B47
    return 1;
}
void initialize_frontend_descriptors_441340(FrontendDescriptorBank bank)
{
    std::fill(bank.begin(), bank.end(), std::uint8_t{0}); // 441342..441353,0x1224 words
    for (std::uint32_t id = 1; id < frontend_descriptor_count; ++id)
    { // 441355..44136D
        (void)initialize_frontend_descriptor_441380(
            FrontendDescriptor{bank.data() + id * frontend_descriptor_stride, frontend_descriptor_stride}, id);
    }
}
FrontendMaterialInitializationBoundary initialize_frontend_material_prefix_441190(
    FrontendSourceStorage &frontend, std::span<const WorldMaterialPreludeEntry> materials)
{
    std::size_t count = 0;
    // Existing native view uses span-end as the pinned table's terminal null.
    // Do not dereference names or resolve material IDs:441190 only tests pointers.
    for (const auto &entry : materials)
    {
        if (!entry.name)
            break;
        ++count;
    }
    initialize_frontend_descriptors_441340(frontend_descriptor_bank(frontend)); // 4411A8
    return {FrontendInitializationStop::before_4411c0, count};                  // 4411AD tail deliberately not executed
}
} // namespace sgn::retail
