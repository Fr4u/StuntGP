#include "sgn/retail_vehicle_load.hpp"
#include <charconv>
#include <string>

namespace sgn::retail
{
VehicleSoundIds decode_vehicle_sound_ids(std::string_view text)
{
    const auto section = text.find("-SFX[] =");
    if (section == text.npos)
        throw FormatError("missing vehicle SFX section");
    text.remove_prefix(section);
    auto integer = [&](std::string_view key) {
        auto at = text.find(key);
        if (at == text.npos)
            throw FormatError("missing vehicle sound identifier");
        auto tail = text.substr(at + key.size());
        at = tail.find_first_not_of(" \t\r\n");
        if (at == tail.npos || tail[at] != '=')
            throw FormatError("malformed vehicle sound field");
        tail.remove_prefix(at + 1);
        at = tail.find_first_not_of(" \t\r\n");
        if (at == tail.npos)
            throw FormatError("missing vehicle sound value");
        tail.remove_prefix(at);
        if (tail.front() == '+')
        {
            tail.remove_prefix(1);
            if (tail.empty() || tail.front() < '0' || tail.front() > '9')
                throw FormatError("invalid signed vehicle sound identifier");
        }
        std::int32_t value;
        auto [end, error] = std::from_chars(tail.data(), tail.data() + tail.size(), value);
        if (error != std::errc{} ||
            (end != tail.data() + tail.size() && *end != ' ' && *end != '\r' && *end != '\n' && *end != '\t'))
            throw FormatError("invalid vehicle sound identifier");
        return value;
    };
    // Source calls in rev/idle/boost/horn order. Physical storage uses horn first.
    VehicleSoundIds result;
    result.engine_rev = integer("Engine_Rev");
    result.engine_idle = integer("Engine_Idle");
    result.engine_boost = integer("Engine_Boost");
    result.horn = integer("Horn");
    return result;
}
void reset_loaded_position_history(const VehicleState &physics, VehiclePositionHistory &history)
{
    history = {};
    for (auto &sample : history.samples)
        sample.position = physics.position;
}
void reset_loaded_vehicle_histories(std::span<VehicleLoadVehicle, 6> vehicles)
{
    for (auto v : vehicles)
    {
        reset_loaded_position_history(v.physics, v.history);
        v.physics.fuel = v.parameters.fuel_capacity;
        v.load.current_refuel_rate = v.parameters.refuel_rate;
    }
}
void load_vehicle_configuration(VehicleLoadWorld &world, const AssetStore &assets, unsigned target,
                                std::int32_t config_id, std::int32_t control_kind)
{
    if (target >= world.vehicles.size())
        throw FormatError("configuration target outside six vehicles");
    world.current = target;
    auto car = world.vehicles[target];
    const auto raw = assets.resource("config\\vehicles\\car" + std::to_string(config_id) + ".cfg");
    const std::string_view text(reinterpret_cast<const char *>(raw.data()), raw.size());
    auto parsed = decode_vehicle_parameters(text);
    const auto sounds = decode_vehicle_sound_ids(text);
    // The twelve source readers never assign C78. A default-constructed parser
    // result must not silently erase the world-owned transmission mode.
    parsed.manual_gears = car.parameters.manual_gears;
    car.parameters = parsed;
    car.load.sounds = sounds;
    build_engine_curve(car.parameters, car.physics);
    reset_loaded_vehicle_histories(world.vehicles);
    car.race.control_kind = control_kind;
    car.physics.brake_signal = 1;
    enable_vehicle_control(car.race);
}
} // namespace sgn::retail
