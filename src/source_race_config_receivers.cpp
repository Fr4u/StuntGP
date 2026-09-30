#include "sgn/source_race_config_receivers.hpp"
#include "sgn/source_script_path_formatter.hpp"
#include "sgn/source_script_search.hpp"
#include "sgn/source_script_number.hpp"
#include "sgn/source_decimal_conversion.hpp"
#include "sgn/source_native_crt.hpp"
#include "sgn/retail_frontend_state.hpp"
#include "sgn/race_session.hpp"
#include <array>
#include <bit>
extern "C" void sgn_source_vehicle_store_4349a2(const std::uint64_t *, float *);
extern "C" void sgn_source_config_engine_40a1d0(const float *, const float *, const float *, const float *, float *,
                                                float *);
namespace sgn::retail
{
namespace
{
using W = std::uint32_t;
using Pointer = SourceBootSearchBoundary;
[[noreturn]] void gap(W site, const char *reason)
{
    throw SourceBootConfigError(site, reason);
}
class Configuration final
{
    RaceSessionPlatform &platform_;
    RaceSessionState &w_;
    SourceBootFilePlatform &files_;
    NativeSourceMenuCrt &crt_;
    SourceApplicationLiteralRef app_;
    W current(W site) const
    {
        if (w_.current_slot >= 6)
            gap(site, "source configuration current outside actual six vehicles");
        return w_.current_slot;
    }
    VehicleParameters &p(W site)
    {
        return w_.vehicles.parameters[current(site)];
    }
    SourceScriptNeedle needle(W address) const
    {
        const auto bytes = app_.vehicle_configuration_literal(address);
        return {{reinterpret_cast<const char *>(bytes.data()), bytes.size()}, 0, false};
    }
    Pointer search(Pointer input, W literal)
    {
        return find_source_script_462890(platform_, input, needle(literal));
    }
    Pointer after_equal(Pointer input, SourceScriptNeedle key, W equals)
    {
        auto found = find_source_script_462890(platform_, input, key);
        found = search(found, equals);
        ++found.offset;
        return found; // Null+1 remains null until reached read.
    }
    void scalar(Pointer input, W key, float &actual)
    { // 434980, destination was captured before the call.
        const auto at = after_equal(input, needle(key), 0x478b5c);
        const auto value = convert_source_camera_atof_bounded_4631a7(platform_, crt_, at);
        sgn_source_vehicle_store_4349a2(&value, &actual); // Only after real CRT publication.
    }
    void integer(Pointer input, W key, std::int32_t &actual)
    { // 434A40, real4631F4 wrap/classification.
        const auto value = parse_source_bank_integer_4631f4(platform_, after_equal(input, needle(key), 0x478b64));
        actual = std::bit_cast<std::int32_t>(value);
    }
    std::uint8_t byte(Pointer input, W site)
    {
        return read_source_script_byte(platform_, input, site);
    }
    void to_space(Pointer &cursor, std::uint8_t character, W next_site)
    {
        while (character != 0x20)
        {
            auto next = cursor;
            ++next.offset;
            character = byte(next, next_site);
            cursor = next;
        }
    }
    void skip_spaces(Pointer &cursor, W site)
    {
        for (;;)
        {
            const auto value = byte(cursor, site);
            if (value != 0x20 && value != 0x0d && value != 0x0a)
                return;
            ++cursor.offset;
        }
    }
    void array(Pointer input, SourceScriptNeedle key, std::span<float> actual)
    { // 434A70
        auto at = find_source_script_462890(platform_, input, key);
        at = search(at, 0x478b68);
        to_space(at, byte(at, 0x434a8b), 0x434a90);
        skip_spaces(at, 0x434a98);
        for (auto &destination : actual)
        {
            const auto value = convert_source_camera_atof_bounded_4631a7(platform_, crt_, at);
            const auto character = byte(at, 0x434abd); // This read precedes FSTP, including on failure.
            sgn_source_vehicle_store_4349a2(&value, &destination);
            to_space(at, character, 0x434ac8);
            skip_spaces(at, 0x434ad0); // Also after the last float.
        }
    }
    void array(Pointer input, W key, std::span<float> actual)
    {
        array(input, needle(key), actual);
    }
    template <std::size_t N>
    SourceScriptNeedle format_key(W format, W index, std::array<std::uint8_t, N> &local, W returned)
    {
        SourceScriptPathCall frame;
        format_source_vehicle_config_key_46280c(w_.boot_config, app_, format, index, local, returned, frame);
        // Formatter published its actual local NUL. Borrow the same local, never a made-up key.
        return {{reinterpret_cast<const char *>(local.data()), frame.initialized_prefix}, 0, false};
    }
    void dimensions(Pointer input)
    {
        const auto s = search(input, 0x4734b0);
        scalar(s, 0x4734c0, p(0x40b49e).mass);
        scalar(s, 0x4734c8, p(0x40b4b8).com_a);
        scalar(s, 0x4734d0, p(0x40b4d1).com_b);
        scalar(s, 0x4734d8, p(0x40b4ea).com_height);
        scalar(s, 0x4734e0, p(0x40b501).wheelbase);
        scalar(s, 0x4734ec, p(0x40b51a).track_front);
        scalar(s, 0x4734f8, p(0x40b533).track_rear);
        scalar(s, 0x473504, p(0x40b54a).length);
        scalar(s, 0x47350c, p(0x40b563).width);
        scalar(s, 0x473514, p(0x40b57c).height);
        scalar(s, 0x47351c, p(0x40b593).ride_height);
    }
    void engine(Pointer input)
    {
        const auto s = search(input, 0x473528);
        scalar(s, 0x473534, p(0x40b5c6).rpm_idle);
        scalar(s, 0x47353c, p(0x40b5e0).rpm_limit);
        scalar(s, 0x473548, p(0x40b5f9).rpm_max);
        scalar(s, 0x473550, p(0x40b612).engine_decay);
        scalar(s, 0x473554, p(0x40b629).max_torque); // Source key is substring Torque, not invented MaxTorque.
        std::array<std::uint8_t, 16> key;
        for (W i = 0; i < 4; ++i)
        {
            const auto n = format_key(0x47355c, i, key, 0x40b655);
            array(s, n, p(0x40b655).torque_curves[i]);
        }
    }
    void gearbox(Pointer input)
    {
        const auto s = search(input, 0x47356c);
        integer(s, 0x47357c, p(0x40b69e).drive_mode);
        integer(s, 0x473588, p(0x40b6b8).gear_count);
        scalar(s, 0x473594, p(0x40b6d1).final_drive);
        scalar(s, 0x4735a0, p(0x40b6ea).shift_time);
        scalar(s, 0x4735ac, p(0x40b701).efficiency);
        scalar(s, 0x4735b8, p(0x40b71a).power_split);
        array(s, 0x4735c8, p(0x40b733).ratios); // C78 manual_gears is untouched.
    }
    void brakes(Pointer input)
    {
        const auto s = search(input, 0x4735d4);
        scalar(s, 0x4735e0, p(0x40b75e).brake_bias);
        scalar(s, 0x4735e8, p(0x40b778).brake_acceleration);
    }
    void tyres(Pointer input)
    {
        const auto s = search(input, 0x4735f8);
        array(s, 0x473604, p(0x40b7b9).friction_curve);
        std::array<std::uint8_t, 32> key;
        for (W i = 0; i < 4; ++i)
        {
            const auto n = format_key(0x473614, i, key, 0x40b7f5);
            const auto t = find_source_script_462890(platform_, input, n); // From original file, not TYRES subsection.
            scalar(t, 0x473620, p(0x40b7ff).tyres[i].radius);
            scalar(t, 0x473628, p(0x40b81e).tyres[i].pressure);
            scalar(t, 0x473634, p(0x40b838).tyres[i].static_friction);
            scalar(t, 0x473638, p(0x40b851).tyres[i].kinetic_friction);
            scalar(t, 0x47363c, p(0x40b86b).tyres[i].travel_in);
            scalar(t, 0x473648, p(0x40b885).tyres[i].damping_in);
            scalar(t, 0x473658, p(0x40b89e).tyres[i].stiffness_in);
            scalar(t, 0x473668, p(0x40b8b8).tyres[i].travel_out);
            scalar(t, 0x473678, p(0x40b8d2).tyres[i].damping_out);
            scalar(t, 0x473688, p(0x40b8eb).tyres[i].stiffness_out);
        }
    }
    void aerodynamics(Pointer input)
    {
        const auto s = search(input, 0x473698);
        scalar(s, 0x4736ac, p(0x40b93e).drag_coefficient);
        scalar(s, 0x4736b0, p(0x40b958).lift_coefficient);
        scalar(s, 0x4736b4, p(0x40b971).frontal_area);
    }
    void steering(Pointer input)
    {
        const auto s = search(input, 0x4736b8);
        scalar(s, 0x4736c8, p(0x40b99e).steering_max);
        scalar(s, 0x4736d4, p(0x40b9b8).steering_sensitivity);
        scalar(s, 0x4736e0, p(0x40b9d1).steering_acceleration);
        array(s, 0x4736f0, p(0x40b9ea).steering_curve);
    }
    void controls(Pointer input)
    {
        const auto s = search(input, 0x473700);
        array(s, 0x473710, p(0x40ba1e).digital_steering);
        array(s, 0x473728, p(0x40ba3d).digital_brake);
        array(s, 0x473740, p(0x40ba5b).analog_steering);
        array(s, 0x473758, p(0x40ba79).analog_brake);
        array(s, 0x47376c, p(0x40ba95).velocity_steering);
    }
    void fuel(Pointer input)
    {
        const auto s = search(input, 0x47378c);
        scalar(s, 0x473798, p(0x40bace).fuel_capacity);
        scalar(s, 0x4737a0, p(0x40bae8).fuel_unit_mass);
        scalar(s, 0x4737ac, p(0x40bb01).fuel_consumption);
        scalar(s, 0x4737bc, p(0x40bb1a).refuel_rate);
        array(s, 0x4737c8, p(0x40bb31).consumption_curve);
    }
    void turbo(Pointer input)
    {
        const auto s = search(input, 0x4737d8);
        scalar(s, 0x4737e4, p(0x40bb6e).turbo_acceleration);
        scalar(s, 0x4737ec, p(0x40bb88).turbo_max);
        scalar(s, 0x4737f4, p(0x40bba1).turbo_decay);
        scalar(s, 0x4737f8, p(0x40bbba).turbo_scale);
        scalar(s, 0x473808, p(0x40bbd1).turbo_consumption);
        scalar(s, 0x473818, p(0x40bbea).turbo_energy_threshold);
        array(s, 0x473828, p(0x40bc03).turbo_curve);
    }
    void sounds(Pointer input)
    {
        const auto s = search(input, 0x473834);
        integer(s, 0x473840, w_.vehicles.load[current(0x40bc3e)].sounds.engine_rev);
        integer(s, 0x47384c, w_.vehicles.load[current(0x40bc58)].sounds.engine_idle);
        integer(s, 0x473858, w_.vehicles.load[current(0x40bc71)].sounds.engine_boost);
        integer(s, 0x473868, w_.vehicles.load[current(0x40bc8a)].sounds.horn);
    }
    void stunt(Pointer input)
    {
        const auto s = search(input, 0x473870);
        scalar(s, 0x47387c, p(0x40bcbe).launch_time);
        scalar(s, 0x473888, p(0x40bcd8).launch_tolerance);
        scalar(s, 0x473898, p(0x40bcf1).launch_speed);
        scalar(s, 0x4738a4, p(0x40bd0a).max_pitch_speed);
        scalar(s, 0x4738b0, p(0x40bd21).max_yaw_speed);
    }
    void curve()
    { // 40A1D0 captures current once; publishes scale0 before first curve read.
        const auto id = current(0x40a1d6);
        auto &s = w_.vehicles.physics[id];
        const auto &parameters = w_.vehicles.parameters[id];
        s.engine_output_scale = 0;
        const auto index = std::bit_cast<W>(s.torque_curve);
        if (index >= parameters.torque_curves.size())
            gap(0x40a227, "engine curve pointer outside the represented four authored curves");
        sgn_source_config_engine_40a1d0(&parameters.rpm_max, &parameters.rpm_limit, &parameters.max_torque,
                                        parameters.torque_curves[index].data(), s.engine_output_curve.data(),
                                        &s.engine_output_scale);
    }
    void histories()
    { // 40A120, all6 slots even when inactive; global current does not change.
        for (W id = 0; id < 6; ++id)
        {
            auto &h = w_.vehicles.history[id];
            h = {}; // 40A0D0 exact41 words represented by8 samples+ticks.
            for (auto &sample : h.samples)
                sample.position = w_.vehicles.physics[id].position;
            w_.vehicles.physics[id].fuel = w_.vehicles.parameters[id].fuel_capacity;
            w_.vehicles.load[id].current_refuel_rate = w_.vehicles.parameters[id].refuel_rate;
        }
    }

  public:
    Configuration(RaceSessionPlatform &platform, SourceBootFilePlatform &files, NativeSourceMenuCrt &crt)
        : platform_(platform), w_(platform.state()), files_(files), crt_(crt),
          app_(platform.source_application_literals())
    {
    }
    void load()
    {
        const auto configuration =
            std::bit_cast<W>(w_.load_selection.configuration_id); // 40BDE9 captured before formatter.
        SourceScriptPathCall path;
        format_source_vehicle_config_path_40bdff(w_.boot_config, app_, configuration, path);
        w_.current_slot = w_.load_selection.template_vehicle; // 40BE04/10, after completed format.
        const auto size = source_boot_file_size_44b3b0(w_.boot_config, files_, path.completed_path());
        auto &bank = platform_.source_allocations();
        const auto allocation = bank.allocate_42e8d0(size + 32u, 0);
        std::size_t copied = 0;
        (void)source_boot_file_read_bank_44b260(w_.boot_config, files_, bank, path.completed_path(), size, allocation,
                                                copied);
        const Pointer original{allocation, 0, !static_cast<bool>(allocation)};
        dimensions(original);
        engine(original);
        gearbox(original);
        brakes(original);
        tyres(original);
        aerodynamics(original);
        steering(original);
        controls(original);
        fuel(original);
        turbo(original);
        sounds(original);
        stunt(original);
        (void)bank.release_42e980(
            allocation); // Only reached on complete parsing; failure never auto-frees source storage.
        curve();
        histories();
        const auto id = current(0x40be99);
        w_.vehicles.race[id].control_kind = std::bit_cast<std::int32_t>(w_.control_kind);
        w_.vehicles.physics[id].brake_signal = 1;
        enable_vehicle_control(w_.vehicles.race[id]); // 4436E0 receives the captured40BE99 ECX.
    }
    void selected()
    {
        const auto saved = w_.load_selection.template_vehicle;
        for (W id = 0; id < 6; ++id)
        {
            if (w_.vehicles.race[id].active != 1)
                continue;
            const auto configuration = w_.cars[id].settings.config; // 40BEE1 before either global write.
            w_.load_selection.template_vehicle = id;
            w_.load_selection.configuration_id = std::bit_cast<std::int32_t>(configuration);
            load();
        }
        w_.load_selection.template_vehicle = saved; // No restore of current/config, no finally on failure.
    }
    void prefix()
    {
        w_.current_slot = 0;
        w_.load_selection.template_vehicle = 0;
        load();
    }
};
std::shared_ptr<RaceSessionPlatform> platform(FrontendState &f, NativeSourceMenuCrt &crt)
{
    const auto p = f.shared_platform();
    p->require_owner_thread();
    if (!p->source_construction_only_entry() || &f.world() != &p->state())
        throw FormatError("source vehicle configuration requires same source Platform");
    crt.require_platform(*p);
    return p;
}
} // namespace
void load_source_vehicle_configuration_40bde0(FrontendState &f, SourceBootFilePlatform &files, NativeSourceMenuCrt &crt)
{
    const auto p = platform(f, crt);
    Configuration(*p, files, crt).load();
}
void reload_selected_source_vehicle_configurations_40bec0(FrontendState &f, SourceBootFilePlatform &files,
                                                          NativeSourceMenuCrt &crt)
{
    const auto p = platform(f, crt);
    Configuration(*p, files, crt).selected();
}
void initialize_source_vehicle_config_prefix_413676(FrontendState &f, SourceBootFilePlatform &files,
                                                    NativeSourceMenuCrt &crt)
{
    const auto p = platform(f, crt);
    Configuration(*p, files, crt).prefix();
}
} // namespace sgn::retail
