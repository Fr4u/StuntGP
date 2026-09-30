#include "sgn/source_selection_data.hpp"
#include "sgn/retail_frontend_state.hpp"
#include "sgn/race_session_platform.hpp"
namespace sgn::retail
{
namespace
{
std::uint32_t track_type_437360(std::uint32_t id) noexcept
{
    switch (id)
    {
    case 4:
    case 5:
    case 6:
    case 7:
    case 10:
    case 11:
    case 14:
    case 15:
    case 17:
    case 21:
    case 22:
        return 1;
    case 1:
    case 8:
    case 16:
    case 19:
        return 2;
    case 12:
    case 18:
    case 20:
    case 23:
    case 24:
        return 3;
    case 2:
    case 3:
    case 9:
    case 13:
        return 4;
    case 25:
        return 5;
    default:
        return 0;
    }
}
std::uint32_t car_type_437400(std::uint32_t id) noexcept
{
    switch (id)
    {
    case 4:
    case 9:
    case 10:
    case 11:
    case 15:
        return 1;
    case 1:
    case 7:
        return 2;
    case 8:
    case 16:
    case 19:
        return 3;
    case 17:
    case 18:
        return 4;
    case 21:
        return 5;
    case 2:
    case 3:
    case 5:
    case 6:
    case 12:
    case 13:
    case 14:
    case 20:
        return 6;
    default:
        return 0;
    }
}
std::uint32_t availability_byte(FrontendState &frontend, std::uint32_t base, std::uint32_t index)
{
    // Source x86 address arithmetic wraps at32 bits. No invented index clamp,
    // car/track count check or boolean normalization before the actual byte read.
    return frontend.source.source_bytes(static_cast<std::uint32_t>(base + index), 1)[0];
}
} // namespace
std::uint32_t source_track_available_4371c0(FrontendState &frontend, std::uint32_t track, std::uint32_t mode)
{
    frontend.shared_platform()->require_owner_thread();
    const auto type = track_type_437360(track); // 4371C6, before mode dispatch
    switch (mode)
    {
    case 1:
        break; // 4371F0, even when classifier returned0
    case 2:
    case 6:
    case 7:
        if (type < 1 || type > 4)
            return 0;
        break;
    case 3:
        if (type != 2 && type != 4)
            return 0;
        break;
    default:
        return 0;
    }
    return availability_byte(frontend, 0x5df580, track); // MOVZX-equivalent XOR EAX/MOV AL
}
std::uint32_t source_car_available_437230(FrontendState &frontend, std::uint32_t car, std::uint32_t mode)
{
    frontend.shared_platform()->require_owner_thread();
    const auto type = car_type_437400(car); // 437236, before mode dispatch
    switch (mode)
    {
    case 1:
        if (type != 1 && type != 2)
            return 0;
        break;
    case 2:
    case 6:
    case 7:
        if (!((type >= 1 && type <= 4) || type == 6))
            return 0;
        break;
    case 3:
        if (type != 2 && type != 3)
            return 0;
        break;
    case 4:
        if ((type >= 1 && type <= 4) || type == 6)
            return 1; // 437289..437290, no availability read
        if (type != 5)
            return 0;
        break;
    case 5:
        if (type != 1 && type != 2 && type != 3 && type != 6)
            return 0;
        break;
    default:
        return 0;
    }
    return availability_byte(frontend, 0x5df568, car); // 437259, raw byte preserved
}
std::uint32_t source_track_value_437da0(std::uint32_t track) noexcept
{
    switch (track)
    {
    case 1:
    case 2:
    case 3:
    case 8:
    case 9:
    case 13:
    case 16:
    case 19:
        return 30000; // 437DB7 literal7530
    default:
        return 0;
    }
}
} // namespace sgn::retail

namespace sgn::retail
{
std::uint32_t source_track_type_437360(std::uint32_t track) noexcept
{
    return track_type_437360(track);
}
} // namespace sgn::retail
