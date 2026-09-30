#include "sgn/clock.hpp"
#include "sgn/scene.hpp"
#include "sgn/telemetry.hpp"
#include <array>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace
{
void require(bool condition, const char *text)
{
    if (!condition)
        throw std::runtime_error(text);
}
template <class F> void rejects(F operation)
{
    bool rejected = false;
    try
    {
        operation();
    }
    catch (const std::invalid_argument &)
    {
        rejected = true;
    }
    require(rejected, "invalid platform input was accepted");
}
} // namespace
int main()
{
    try
    {
        sgn::SampleWindow<64> samples;
        require(samples.summarize().total_samples == 0, "empty telemetry has samples");
        for (unsigned i = 0; i < 100000; ++i)
            samples.record(i);
        auto summary = samples.summarize();
        require(summary.total_samples == 100000 && summary.window_samples == 64, "telemetry did not stay bounded");
        require(summary.p50 == 99967 && summary.p95 == 99995 && summary.p99 == 99998 && summary.maximum == 99999,
                "telemetry rolling quantiles incorrect");
        rejects([&] { samples.record(std::numeric_limits<double>::infinity()); });
        rejects([&] { samples.record(std::numeric_limits<double>::quiet_NaN()); });
        rejects([&] { samples.record(-1); });
        require(samples.summarize().total_samples == 100000, "rejected telemetry mutated state");

        std::array<sgn::SceneInstance, 2> scene{};
        scene[1].mesh = 1;
        scene[1].pose.right = {0, 0, -1};
        scene[1].pose.forward = {1, 0, 0};
        scene[1].pose.position = {17000, -250, 20};
        sgn::validate_instances(scene, 2);
        rejects([&] { sgn::validate_instances(scene, 1); });
        auto pose = scene[1].pose;
        pose.up = pose.right;
        require(sgn::valid_pose(pose), "finite singular pose rejected");
        pose = {};
        pose.right = {-1, 0, 0};
        require(sgn::valid_pose(pose) && sgn::mirrored_pose(pose), "mirrored retail basis lost");
        pose = {};
        pose.position.x = std::numeric_limits<float>::infinity();
        require(!sgn::valid_pose(pose), "infinite pose accepted");
        pose = {};
        pose.scale = 0;
        require(sgn::valid_pose(pose), "finite zero scale rejected");
        pose.scale = -1;
        require(sgn::valid_pose(pose) && sgn::mirrored_pose(pose), "negative scalar scale lost");
        // A flattened XY triangle still has area one. Its lighting normal is
        // finite even though the inverse-transpose does not exist.
        pose = {};
        pose.forward = {};
        std::array<sgn::SceneInstance::Part, 1> flat_parts{{{pose, true}}};
        sgn::SceneInstance flat{0, {}, flat_parts};
        sgn::validate_instances(std::span(&flat, 1), 1);
        auto edge1 = sgn::transform_direction(pose, {2, 0, 0});
        auto edge2 = sgn::transform_direction(pose, {0, 1, 0});
        require(sgn::cross(edge1, edge2).z == 2 && sgn::lighting_normal_basis(pose)[2].z == 1,
                "rank-two surface lost area or normal");
        pose.right = {}; pose.up = {};
        require(sgn::valid_pose(pose) && sgn::lighting_normal_basis(pose)[2].z == 0,
                "rank-zero transform must remain finite");
        pose = {};
        pose.right = {2, 0, 0};
        pose.up = {1, 3, 0};
        pose.forward = {0, 0, 4};
        pose.scale = 2;
        require(sgn::valid_pose(pose), "finite nonsingular affine basis rejected");
        const auto normals = sgn::normal_basis(pose).value();
        require(std::abs(sgn::dot(normals[0], pose.right * pose.scale) - 1) < 1e-6f &&
                    std::abs(sgn::dot(normals[0], pose.up * pose.scale)) < 1e-6f,
                "inverse-transpose did not preserve normal orthogonality");

        // The accepted portrait aspect and wheel range must not move the whole
        // mesh beyond the far plane. Exercise both ends of the viewport range.
        for (unsigned width : {320u, 720u, 3840u, 16384u})
            for (unsigned height : {240u, 2160u, 16384u})
                for (float radius : {0.1f, 1.f, 20000.f})
                    for (float zoom : {2.7f, 30.f})
                    {
                        const auto clip = sgn::inspection_frustum(radius, radius * zoom, width, height);
                        require(clip.far_plane > clip.distance + radius * 2,
                                "portrait or maximum zoom clips the inspection scene");
                    }
        rejects([] { sgn::inspection_frustum(1, 1, 0, 240); });
        rejects([] { sgn::inspection_frustum(-1, 1, 320, 240); });
        rejects([] { sgn::inspection_frustum(1, std::numeric_limits<float>::infinity(), 320, 240); });

        // The fixed clock must retain its phase across changes in display cadence,
        // not just produce the right count at a constant integer refresh rate.
        sgn::TickClock clock;
        std::int64_t elapsed = 0;
        std::uint64_t sum = 0;
        for (unsigned i = 0; i < 100000; ++i)
        {
            const std::int64_t delta = 1'000'000 + static_cast<std::int64_t>((i * 7919u) % 37'000'000u);
            elapsed += delta;
            clock.advance(std::chrono::nanoseconds(delta), [&](auto tick) { sum += tick; });
            require(clock.ticks() == static_cast<std::uint64_t>(elapsed * 60 / 1'000'000'000),
                    "variable display cadence changed simulation ticks");
        }
        const auto ticks = clock.ticks();
        require(sum == ticks * (ticks - 1) / 2, "tick order or multiplicity changed");
        rejects([&] { clock.advance(std::chrono::nanoseconds(-1), [](auto) {}); });
        std::cout
            << "platform contracts passed: bounded telemetry, affine scene poses, 100000 changing display intervals\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
