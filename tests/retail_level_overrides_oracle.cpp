#include "retail_level_overrides_state.hpp"
#include <fstream>
#include <iostream>

namespace
{
using namespace sgn::retail;
using Record = std::array<std::uint8_t, level_override_oracle::record_size>;
unsigned get(const Record &b, unsigned at)
{
    unsigned value;
    std::memcpy(&value, b.data() + at, 4);
    return value;
}
void put(Record &b, unsigned at, unsigned value)
{
    std::memcpy(b.data() + at, &value, 4);
}
struct Dependencies : LevelOverrideDependencies
{
    Record &b;
    explicit Dependencies(Record &bytes) : b(bytes) {}
    void emit(unsigned entry, const LevelOverrideState &s)
    {
        auto count = get(b, 384);
        if (count >= 8)
            throw std::runtime_error("callback trace overflow");
        put(b, 388 + count * 12, entry);
        put(b, 392 + count * 12, s.level.game_type);
        put(b, 396 + count * 12, s.level.lap_count);
        put(b, 384, count + 1);
    }
    void prepare_time_trial(LevelOverrideState &s) override
    {
        emit(0x445360, s);
        s.split = 0;
        if (get(b, 380) & 1)
            s.level.game_type = 3;
    }
    void prepare_cars(LevelOverrideState &s) override
    {
        emit(0x444db0, s);
        if (get(b, 380) & 2)
            s.level.game_type = 3;
        s.level.lap_record_ticks = 123456;
    }
};
void read(std::istream &in, void *data, std::size_t bytes)
{
    if (!in.read(static_cast<char *>(data), static_cast<std::streamsize>(bytes)))
        throw std::runtime_error("truncated level override corpus");
}
} // namespace
int main(int argc, char **argv)
{
    try
    {
        if (argc != 2)
            throw std::runtime_error("expected level override corpus");
        std::ifstream in(argv[1], std::ios::binary);
        char magic[8];
        read(in, magic, 8);
        if (std::string_view(magic, 8) != "SGNLO001")
            throw std::runtime_error("bad override magic");
        unsigned count;
        read(in, &count, 4);
        if (!count || count > 4096)
            throw std::runtime_error("invalid override corpus count");
        Record actual{}, expected{};
        for (unsigned i = 0; i < count; ++i)
        {
            unsigned entry;
            read(in, &entry, 4);
            read(in, actual.data(), actual.size());
            read(in, expected.data(), expected.size());
            LevelOverrideOptions options;
            LevelOverrideState state;
            level_override_oracle::transfer(actual, options, state, true);
            Dependencies deps(actual);
            switch (entry)
            {
            case 0x460bc0:
                update_level_audio(state.audio, options);
                break;
            case 0x445310:
                apply_demo_override(state);
                break;
            case 0x445120:
                apply_challenge_override(state, options.challenge_ticks);
                break;
            case 0x4453a0:
                apply_level_overrides(options, state, deps);
                break;
            default:
                throw std::runtime_error("unknown override entry");
            }
            level_override_oracle::transfer(actual, options, state, false);
            for (unsigned at = 0; at < actual.size(); at += 4)
                if (get(actual, at) != get(expected, at))
                    throw std::runtime_error("override case=" + std::to_string(i) + " offset=" + std::to_string(at) +
                                             " expected=" + std::to_string(get(expected, at)) +
                                             " actual=" + std::to_string(get(actual, at)));
        }
        if (in.peek() != std::char_traits<char>::eof())
            throw std::runtime_error("trailing override corpus");
        std::cout << "{\"passed\":true,\"cases\":" << count << ",\"compared_words\":" << count * 128
                  << ",\"tolerance_ulp\":0}\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
