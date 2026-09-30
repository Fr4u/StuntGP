#include "sgn/contact_response.hpp"
#include <bit>
#include <cstring>
#include <fstream>
#include <iostream>

namespace {
using namespace sgn::retail::contact;
namespace next = sgn::retail::contact::response;
using Record = std::array<std::uint8_t, 0x7800>;
std::uint32_t word(const Record& b, std::size_t at) {
    std::uint32_t value; std::memcpy(&value, b.data() + at, 4); return value;
}
void word(Record& b, std::size_t at, std::uint32_t value) { std::memcpy(b.data() + at, &value, 4); }
float real(const Record& b, std::size_t at) { return std::bit_cast<float>(word(b, at)); }
void transfer(Record& b, next::ContactRig& rig, Vector4& position, Matrix4& matrix, bool reading) {
    auto f = [&](float& value, std::size_t at) {
        if (reading) value = real(b, at); else word(b, at, std::bit_cast<std::uint32_t>(value));
    };
    auto u = [&](std::uint32_t& value, std::size_t at) { if (reading) value = word(b, at); else word(b, at, value); };
    auto v = [&](Vector4& value, std::size_t at) { f(value.x, at); f(value.y, at + 4); f(value.z, at + 8); u(value.opaque, at + 12); };
    v(position, 0x7270);
    for (unsigned j = 0; j < 16; ++j) f(matrix[j], 0x72f8 + 4 * j);
    u(rig.count, 0x73ac); f(rig.origin_y_offset, 0x73b0); f(rig.bounding_radius, 0x72b4);
    for (unsigned j = 0; j < 6; ++j) {
        auto& p = rig.probes[j]; const auto at = 0x73b4 + j * 0x4c;
        v(p.local, at); v(p.current, at + 16); v(p.previous, at + 32); v(p.velocity, at + 48);
        u(p.contact_iterations, at + 64); f(p.mass_fraction, at + 68); f(p.radius, at + 72);
    }
}
void apply(Record& b, unsigned entry, unsigned selector) {
    next::ContactRig rig; Vector4 position; Matrix4 matrix{};
    transfer(b, rig, position, matrix, true);
    switch (entry) {
    case 0x4082d0: next::initialize_rig(selector, position, rig); break;
    case 0x408640: {
        const Vector4 requested{real(b,0x7000),real(b,0x7004),real(b,0x7008),word(b,0x700c)};
        Pose pose{};ContactBody body{};
        next::place_rig(requested,pose,body,rig);position=body.position;
        const auto write=[&](unsigned at,Vector4 v){word(b,at,std::bit_cast<std::uint32_t>(v.x));word(b,at+4,std::bit_cast<std::uint32_t>(v.y));word(b,at+8,std::bit_cast<std::uint32_t>(v.z));word(b,at+12,v.opaque);};
        write(0x1bac,pose.position);write(0x1bbc,pose.previous_position);write(0x7280,body.previous_position);
        break;
    }
    case 0x422200:
        if (rig.count > 6) throw std::runtime_error("probe fixture outside retail layout");
        next::center_probes(std::span(rig.probes).first(rig.count), rig.origin_y_offset); break;
    case 0x421e70: next::damp_incremental(matrix, std::span<const ContactProbe, 4>(rig.probes.data(), 4)); break;
    case 0x421fa0: next::settle_incremental(matrix, std::span<const ContactProbe, 4>(rig.probes.data(), 4)); break;
    case 0x4092d0: next::limit_incremental(matrix); break;
    case 0x408bc0: {
        const Vector4 velocity{real(b, 0x72a0), real(b, 0x72a4), real(b, 0x72a8), word(b, 0x72ac)};
        const sgn::Vec3 normal{real(b, 0x7080), real(b, 0x7084), real(b, 0x7088)};
        sgn::Vec3 delta{real(b, 0x7100), real(b, 0x7104), real(b, 0x7108)};
        next::linear_friction(velocity, normal, real(b, 0x7090), delta);
        word(b, 0x7100, std::bit_cast<std::uint32_t>(delta.x));
        word(b, 0x7104, std::bit_cast<std::uint32_t>(delta.y));
        word(b, 0x7108, std::bit_cast<std::uint32_t>(delta.z));
        break;
    }
    case 0x4089b0: {
        if (selector >= 6) throw std::runtime_error("angular friction probe outside layout");
        const sgn::Vec3 normal{real(b, 0x7080), real(b, 0x7084), real(b, 0x7088)};
        const sgn::Vec3 up{real(b, 0x738c), real(b, 0x7390), real(b, 0x7394)};
        Matrix4 delta{};
        for (unsigned j = 0; j < 16; ++j) delta[j] = real(b, 0x7100 + j * 4);
        next::angular_friction(position, rig.probes[selector], matrix, up, normal, real(b, 0x7090), delta);
        for (unsigned j = 0; j < 16; ++j) word(b, 0x7100 + j * 4, std::bit_cast<std::uint32_t>(delta[j]));
        break;
    }
    case 0x408d10: {
        next::ImpactCount state{std::bit_cast<std::int32_t>(word(b, 0x2568)), std::bit_cast<std::int32_t>(word(b, 0x256c))};
        next::add_impact_count(state, std::bit_cast<std::int32_t>(selector), real(b, 0x7090));
        word(b, 0x2568, std::bit_cast<std::uint32_t>(state.value));
        break;
    }
    case 0x408dc0: {
        next::EnergyImpactParameters p{real(b, 0x7040), real(b, 0x7044), real(b, 0x7048), real(b, 0x704c), real(b, 0x7050), real(b, 0x7054)};
        next::EnergyImpactState state{std::bit_cast<std::int32_t>(word(b, 0x24e0)), std::bit_cast<std::int32_t>(word(b, 0x2640)),
            real(b, 0x2660), real(b, 0x2570), real(b, 0x257c), std::bit_cast<std::int32_t>(word(b, 0x2580))};
        next::consume_impact_energy(state, p, real(b, 0x7090));
        word(b, 0x2570, std::bit_cast<std::uint32_t>(state.energy));
        word(b, 0x257c, std::bit_cast<std::uint32_t>(state.accumulated_loss));
        word(b, 0x2580, std::bit_cast<std::uint32_t>(state.hold));
        break;
    }
    case 0x408cb0: {
        next::DirectionalImpact state{{real(b, 0x2328), real(b, 0x232c), real(b, 0x2330)}, real(b, 0x76f8)};
        next::directional_impact(state, std::bit_cast<std::int32_t>(selector),
            {real(b, 0x7080), real(b, 0x7084), real(b, 0x7088)}, real(b, 0x7090));
        word(b, 0x2328, std::bit_cast<std::uint32_t>(state.accumulated.x));
        word(b, 0x232c, std::bit_cast<std::uint32_t>(state.accumulated.y));
        word(b, 0x2330, std::bit_cast<std::uint32_t>(state.accumulated.z));
        word(b, 0x76f8, std::bit_cast<std::uint32_t>(state.magnitude));
        break;
    }
    case 0x409664: {
        if (selector >= 6) throw std::runtime_error("physical response probe outside layout");
        ContactBody body{}; body.position = position; body.incremental = matrix;
        body.velocity = {real(b, 0x72a0), real(b, 0x72a4), real(b, 0x72a8), word(b, 0x72ac)};
        ImpactAccumulator impact{}; impact.normal_speed = real(b, 0x7090);
        impact.velocity_delta = {real(b, 0x7140), real(b, 0x7144), real(b, 0x7148)};
        for (unsigned j = 0; j < 16; ++j) impact.rotation_delta[j] = real(b, 0x7100+j*4);
        next::ImpactResponseState state{}; WheelContact wheel{};
        const auto fields = [&](bool reading) {
            auto f = [&](float& v, unsigned at) { if (reading) v = real(b, at); else word(b, at, std::bit_cast<std::uint32_t>(v)); };
            auto i = [&](std::int32_t& v, unsigned at) { if (reading) v = std::bit_cast<std::int32_t>(word(b, at)); else word(b, at, std::bit_cast<std::uint32_t>(v)); };
            auto u = [&](std::uint32_t& v, unsigned at) { if (reading) v = word(b, at); else word(b, at, v); };
            f(state.base_com_height, 0x22c); f(state.effective_com_height, 0x261c); i(state.reset_ticks, 0x2624);
            u(state.reset_2628, 0x2628); u(state.reset_262c, 0x262c); f(state.shock, 0x2360); i(state.wheel_touched, 0x2378);
            f(state.alignment, 0x7150); f(state.contact_load, 0x7154); i(state.body_touched, 0x7158); f(state.effect_strength, 0x715c);
            if (selector < 4) {
                const auto at = 0x1fe0 + selector * 0x98;
                f(wheel.normal.x, at+76); f(wheel.normal.y, at+80); f(wheel.normal.z, at+84); u(wheel.normal.opaque, at+88);
                i(wheel.countdown, at+112); i(wheel.previous_type, at+116); i(wheel.type, at+120); i(wheel.changed, at+124);
                f(wheel.grip, at+132); f(wheel.roughness, at+136); f(wheel.rolling, at+140);
            }
        };
        fields(true);
        next::finish_impact_physical(body, rig.probes[selector], selector,
            {real(b, 0x7080), real(b, 0x7084), real(b, 0x7088), word(b, 0x708c)},
            {real(b, 0x738c), real(b, 0x7390), real(b, 0x7394)}, real(b, 0x7094),
            {b[0x7060], b[0x7061], b[0x7062], b[0x7063]}, std::bit_cast<std::int32_t>(word(b, 0x7064)), impact, wheel, state);
        fields(false);
        word(b, 0x72a0, std::bit_cast<std::uint32_t>(body.velocity.x));
        word(b, 0x72a4, std::bit_cast<std::uint32_t>(body.velocity.y));
        word(b, 0x72a8, std::bit_cast<std::uint32_t>(body.velocity.z));
        word(b, 0x7140, std::bit_cast<std::uint32_t>(impact.velocity_delta.x));
        word(b, 0x7144, std::bit_cast<std::uint32_t>(impact.velocity_delta.y));
        word(b, 0x7148, std::bit_cast<std::uint32_t>(impact.velocity_delta.z));
        for (unsigned j = 0; j < 16; ++j) word(b, 0x7100+j*4, std::bit_cast<std::uint32_t>(impact.rotation_delta[j]));
        break;
    }
    default: throw std::runtime_error("unknown candidate entry");
    }
    transfer(b, rig, position, matrix, false);
}
void read(std::ifstream& in, void* pointer, std::size_t size) {
    if (!in.read(static_cast<char*>(pointer), static_cast<std::streamsize>(size))) throw std::runtime_error("truncated candidate corpus");
}
std::uint32_t read_word(std::ifstream& in) { std::uint32_t value; read(in, &value, 4); return value; }
}
int main(int argc, char** argv) {
    try {
        if (argc != 2) throw std::runtime_error("expected candidate corpus path");
        std::ifstream in(argv[1], std::ios::binary); char magic[8]; read(in, magic, 8);
        if (std::string(magic, 8) != "SGNCR001") throw std::runtime_error("wrong candidate corpus");
        const auto count = read_word(in); if (count > 100000) throw std::runtime_error("candidate corpus count");
        unsigned errors = 0; std::uint64_t exact_words = 0;
        for (unsigned i = 0; i < count; ++i) {
            const auto entry = read_word(in), selector = read_word(in); Record actual{}, expected{};
            read(in, actual.data(), actual.size()); read(in, expected.data(), expected.size()); apply(actual, entry, selector);
            for (std::size_t at = 0; at < actual.size(); at += 4) {
                if (word(actual, at) == word(expected, at)) { ++exact_words; continue; }
                if (errors++ < 24) std::cerr << "case " << i << " entry " << std::hex << entry << " offset " << at
                    << " expected " << word(expected, at) << " actual " << word(actual, at) << std::dec << '\n';
            }
        }
        if (in.peek() != std::char_traits<char>::eof()) throw std::runtime_error("trailing candidate corpus");
        std::cout << "{\"cases\":" << count << ",\"exact_words\":" << exact_words << ",\"errors\":" << errors << ",\"allowed_ulp\":0}\n";
        return errors ? 1 : 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
