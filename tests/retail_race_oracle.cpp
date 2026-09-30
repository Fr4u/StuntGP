#include "retail_race_state.hpp"
#include <fstream>
#include <iostream>
#include <vector>

namespace
{
constexpr std::size_t stride = 0x7724, record_size = stride * 6 + 16;
std::uint32_t get(const auto &b, std::size_t at)
{
    std::uint32_t v{};
    std::memcpy(&v, b.data() + at, 4);
    return v;
}
void put(auto &b, std::size_t at, std::uint32_t v)
{
    std::memcpy(b.data() + at, &v, 4);
}
void read(std::istream &in, void *data, std::size_t size)
{
    if (!in.read(static_cast<char *>(data), static_cast<std::streamsize>(size)))
        throw std::runtime_error("truncated race corpus");
}
} // namespace
int main(int argc, char **argv)
{
    try
    {
        using namespace sgn::retail;
        if (argc != 2)
            throw std::runtime_error("expected race corpus");
        std::ifstream in(argv[1], std::ios::binary);
        char magic[8]{};
        read(in, magic, 8);
        if (std::string(magic, 8) != "SGNRC001")
            throw std::runtime_error("wrong race corpus");
        std::uint32_t count{};
        read(in, &count, 4);
        if (!count || count > 20000)
            throw std::runtime_error("race case count outside limit");
        std::vector<std::uint8_t> actual(record_size), expected(record_size);
        std::uint64_t words = 0;
        for (std::uint32_t n = 0; n < count; ++n)
        {
            std::uint32_t entry{}, slot{};
            read(in, &entry, 4);
            read(in, &slot, 4);
            if (slot >= 6)
                throw std::runtime_error("race slot outside limit");
            read(in, actual.data(), actual.size());
            read(in, expected.data(), expected.size());
            std::array<VehicleRaceState, 6> cars;
            for (unsigned i = 0; i < 6; ++i)
            {
                auto span = std::span(actual).subspan(i * stride, stride);
                race_oracle::transfer(span, cars[i], true);
            }
            VehicleState physics;
            physics.throttle = std::bit_cast<float>(get(actual, slot * stride + 0x1cf8));
            physics.energy_exempt = std::bit_cast<std::int32_t>(get(actual, slot * stride + 0x24e0));
            RaceClockState clock;
            clock.rolling_state = get(actual, stride * 6);
            clock.course_length = std::bit_cast<float>(get(actual, stride * 6 + 4));
            switch (entry)
            {
            case 0x41d300:
                reset_race_record(cars[slot], physics);
                break;
            case 0x4436e0:
                enable_vehicle_control(cars[slot]);
                break;
            case 0x443700:
                disable_vehicle_control(cars[slot], physics);
                break;
            case 0x443750:
                enable_all_controls(cars);
                break;
            case 0x41de40:
                begin_lap_timers(cars);
                break;
            case 0x444c00:
                arm_start_countdown(cars);
                break;
            case 0x443a30:
                check_rolling_start(clock, cars[slot], cars);
                break;
            case 0x443e10:
                clear_vehicle_notice(cars[slot]);
                break;
            case 0x405350:
                reset_route_tracking(cars[slot]);
                break;
            default:
                throw std::runtime_error("unrecognized race entry");
            }
            for (unsigned i = 0; i < 6; ++i)
            {
                auto span = std::span(actual).subspan(i * stride, stride);
                race_oracle::transfer(span, cars[i], false);
            }
            put(actual, slot * stride + 0x1cf8, std::bit_cast<std::uint32_t>(physics.throttle));
            put(actual, slot * stride + 0x24e0, std::bit_cast<std::uint32_t>(physics.energy_exempt));
            put(actual, stride * 6, clock.rolling_state);
            for (std::size_t i = 0; i < record_size; i += 4)
            {
                ++words;
                if (get(actual, i) != get(expected, i))
                    throw std::runtime_error("race case " + std::to_string(n) + " offset " + std::to_string(i) +
                                             " expected " + std::to_string(get(expected, i)) + " actual " +
                                             std::to_string(get(actual, i)));
            }
        }
        if (in.peek() != std::char_traits<char>::eof())
            throw std::runtime_error("trailing race corpus");
        std::cout << "{\"passed\":true,\"cases\":" << count << ",\"compared_words\":" << words
                  << ",\"tolerance_ulp\":0}\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
