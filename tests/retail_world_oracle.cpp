#include "sgn/retail_world.hpp"
#include "retail_race_state.hpp"
#include <bit>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>

namespace
{
using namespace sgn::retail;
constexpr unsigned stride = 0x7724, tail = stride * 6, size = tail + 1024;
using Bytes = std::vector<std::uint8_t>;
std::uint32_t get(const Bytes &b, unsigned at)
{
    std::uint32_t v;
    std::memcpy(&v, b.data() + at, 4);
    return v;
}
void put(Bytes &b, unsigned at, std::uint32_t v)
{
    std::memcpy(b.data() + at, &v, 4);
}
float real(const Bytes &b, unsigned at)
{
    return std::bit_cast<float>(get(b, at));
}
void event(Bytes &b, unsigned entry, unsigned slot, unsigned value = 0)
{
    const auto count = get(b, tail + 164);
    if (count >= 32)
        throw std::runtime_error("trace capacity");
    const auto at = tail + 168 + count * 12;
    put(b, at, entry);
    put(b, at + 4, slot);
    put(b, at + 8, value);
    put(b, tail + 164, count + 1);
}
struct Announcements : RankingAnnouncements
{
    Bytes &bytes;
    explicit Announcements(Bytes &b) : bytes(b) {}
    void leader(unsigned vehicle, std::string_view text) override
    {
        event(bytes, 0x442cf0, vehicle, 180);
        put(bytes, tail + 600, static_cast<unsigned>(text.size()));
        std::memcpy(bytes.data() + tail + 604, text.data(), text.size());
        bytes[tail + 604 + text.size()] = 0;
    }
};
struct Steps : WorldSteps
{
    Bytes &bytes;
    WorldTraversal &world;
    std::array<std::uint16_t, 6> &statuses;
    unsigned seed;
    Steps(Bytes &b, WorldTraversal &w, std::array<std::uint16_t, 6> &s)
        : bytes(b), world(w), statuses(s), seed(get(b, tail + 160))
    {
    }
    std::uint16_t replay_status(ReplayHandle h) const override
    {
        if (h.slot >= statuses.size())
            throw std::out_of_range("replay status slot");
        return statuses[h.slot];
    }
    void contact(unsigned i) override
    {
        event(bytes, 0x409b60, i, std::bit_cast<unsigned>(world.physics[i].air_factor));
        const auto next = (i + 1) % 6;
        world.physics[next].air_factor = static_cast<float>(i + 2);
        if ((seed + i) % 4 == 0)
            world.race[next].active = 1;
    }
    void advance_replay(unsigned i) override
    {
        event(bytes, 0x41af50, i);
        if (world.race[i].recording && (seed + i) % 3 == 0)
            statuses[world.race[i].recording->slot] = 0;
    }
    void playback_pose(unsigned i) override
    {
        event(bytes, 0x418970, i);
        world.race[i].progress = static_cast<float>(double(world.race[i].progress) + 0.25);
    }
    void vehicle_step(unsigned i) override
    {
        event(bytes, 0x418830, i);
        world.race[i].progress = static_cast<float>(double(world.race[i].progress) + 1);
    }
    void ranking() override
    {
        event(bytes, 0x41e2d0, world.current_slot);
        world.race[0].place = seed;
    }
};
void apply(Bytes &b, unsigned entry)
{
    std::array<VehicleRaceState, 6> cars{};
    std::array<VehicleState, 6> physics{};
    std::array<std::uint16_t, 6> statuses{};
    for (unsigned i = 0; i < 6; ++i)
    {
        auto span = std::span(b).subspan(i * stride, stride);
        race_oracle::transfer(span, cars[i], true);
        cars[i].place = get(b, i * stride + 0x24ac);
        const auto replay = get(b, i * stride + 0x7268);
        if (replay != 0xffffffff)
        {
            if (replay >= 6)
                throw std::runtime_error("bad replay handle");
            cars[i].recording = ReplayHandle{replay};
        }
        physics[i].position.y = real(b, i * stride + 0x1bb0);
        physics[i].air_factor = real(b, i * stride + 0x1fd0);
        statuses[i] = static_cast<std::uint16_t>(get(b, tail + 128 + i * 4));
    }
    TimeTrialGhosts ghosts;
    ghosts.reference_completed = get(b, tail + 8);
    for (unsigned i = 0; i < 4; ++i)
    {
        const auto at = tail + 64 + i * 16;
        ghosts.roles[i] = {get(b, at), get(b, at + 4), {get(b, at + 8)}, get(b, at + 12)};
    }
    unsigned slot = get(b, tail);
    WorldTraversal world{physics, cars, slot, get(b, tail + 4)};
    Steps steps(b, world, statuses);
    switch (entry)
    {
    case 0x443300:
        put(b, tail + 12, select_replacement_ghost(ghosts).value_or(0xffffffff));
        break;
    case 0x443590:
        select_active_ghosts(ghosts);
        break;
    case 0x443560:
        hold_ghost_collisions(ghosts, cars);
        break;
    case 0x41e2d0: {
        RaceRanking ranking;
        const std::array<std::string_view, 6> names{"CAR0", "CAR1", "CAR2", "CAR3", "CAR4", "CAR5"};
        Announcements output(b);
        update_ranking(cars, ranking, names, "leads", output);
        put(b, tail + 16, ranking.count);
        for (unsigned i = 0; i < 6; ++i)
            put(b, tail + 20 + i * 4, ranking.slots[i].value_or(0xffffffff));
        break;
    }
    case 0x40e340:
        reset_air_factors(physics);
        break;
    case 0x4189e0:
        world_contacts(world, steps);
        break;
    case 0x418a90:
        world_tick(world, steps);
        break;
    default:
        throw std::runtime_error("unrecognized world entry");
    }
    put(b, tail, slot);
    put(b, tail + 8, ghosts.reference_completed);
    for (unsigned i = 0; i < 4; ++i)
    {
        const auto at = tail + 64 + i * 16;
        const auto &role = ghosts.roles[i];
        put(b, at, role.active);
    }
    for (unsigned i = 0; i < 6; ++i)
    {
        auto span = std::span(b).subspan(i * stride, stride);
        race_oracle::transfer(span, cars[i], false);
        put(b, i * stride + 0x24ac, cars[i].place);
        put(b, i * stride + 0x1fd0, std::bit_cast<unsigned>(physics[i].air_factor));
        put(b, tail + 128 + i * 4, statuses[i]);
    }
}
void read(std::istream &stream, void *out, std::size_t bytes)
{
    if (!stream.read(static_cast<char *>(out), static_cast<std::streamsize>(bytes)))
        throw std::runtime_error("truncated world corpus");
}
} // namespace
int main(int argc, char **argv)
{
    try
    {
        if (argc != 2)
            throw std::runtime_error("expected world corpus");
        std::ifstream stream(argv[1], std::ios::binary);
        char magic[8];
        read(stream, magic, 8);
        if (std::string_view(magic, 8) != "SGNWR001")
            throw std::runtime_error("wrong world corpus");
        unsigned cases{};
        read(stream, &cases, 4);
        if (cases == 0 || cases > 4096)
            throw std::runtime_error("invalid world corpus count");
        Bytes actual(size), expected(size);
        std::uint64_t words = 0;
        for (unsigned n = 0; n < cases; ++n)
        {
            unsigned entry;
            read(stream, &entry, 4);
            read(stream, actual.data(), size);
            read(stream, expected.data(), size);
            apply(actual, entry);
            for (unsigned at = 0; at < size; at += 4)
            {
                ++words;
                if (get(actual, at) != get(expected, at))
                    throw std::runtime_error("world case=" + std::to_string(n) + " entry=" + std::to_string(entry) +
                                             " offset=" + std::to_string(at) +
                                             " expected=" + std::to_string(get(expected, at)) +
                                             " actual=" + std::to_string(get(actual, at)));
            }
        }
        if (stream.peek() != std::char_traits<char>::eof())
            throw std::runtime_error("trailing world corpus");
        std::cout << "{\"passed\":true,\"cases\":" << cases << ",\"compared_words\":" << words
                  << ",\"tolerance_ulp\":0}\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
