#include "sgn/pmd_scene.hpp"
#include <algorithm>
#include <bit>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace
{
void require(bool condition, const char *reason)
{
    if (!condition)
        throw std::runtime_error(reason);
}
bool near(float a, float b)
{
    return std::abs(double(a) - b) <= 0.003 + 0.000003 * std::max(std::abs(double(a)), std::abs(double(b)));
}
void verify(const sgn::Model &flat, const sgn::PmdRig &rig, std::size_t &hidden, std::size_t &vertices)
{
    const auto parts = sgn::authored_scene_parts(rig);
    require(parts.size() == flat.parts, "part identity lost");
    for (const auto &part : parts)
        if (!part.visible)
            ++hidden;
    for (const auto &target : flat.surfaces)
    {
        std::size_t at = target.first;
        for (const auto &source : rig.geometry.surfaces)
        {
            require(source.part < parts.size(), "surface part out of bounds");
            if (source.texture != target.texture || source.translucent != target.translucent ||
                source.two_sided != target.two_sided || !parts[source.part].visible)
                continue;
            for (std::size_t i = source.first; i < source.first + source.count; ++i)
            {
                require(at < target.first + target.count, "rig duplicates visible geometry");
                const auto &expected = flat.vertices.at(at++);
                const auto &raw = rig.geometry.vertices.at(i);
                const auto &pose = parts[source.part].pose;
                const auto transformed = sgn::transform_direction(pose, raw.position) + pose.position;
                require(near(expected.position.x, transformed.x) && near(expected.position.y, transformed.y) &&
                            near(expected.position.z, transformed.z),
                        "authored GPU pose changed model geometry");
                require(std::bit_cast<unsigned>(expected.u) == std::bit_cast<unsigned>(raw.u) &&
                            std::bit_cast<unsigned>(expected.v) == std::bit_cast<unsigned>(raw.v) &&
                            expected.color == raw.color,
                        "rig changed original UV/color words");
                ++vertices;
            }
        }
        require(at == target.first + target.count, "rig omitted visible geometry");
    }
}
} // namespace
int main(int argc, char **argv)
{
    try
    {
        if (argc != 2)
            throw std::runtime_error("expected retail root");
        sgn::AssetStore assets(argv[1]);
        std::size_t lods = 0, hidden = 0, vertices = 0;
        for (const auto &name : assets.models())
        {
            const auto bytes = sgn::read_file(std::filesystem::path(argv[1]) / "meshdata" / (name + ".PMD"));
            if (std::string(reinterpret_cast<const char *>(bytes.data()), 9) != "PMD V1.83")
                continue;
            try
            {
                auto base = sgn::decode_pmd(bytes);
                for (unsigned lod = 0; lod < base.lod_count; ++lod)
                {
                    verify(sgn::decode_pmd(bytes, lod), sgn::decode_pmd_rig(bytes, lod), hidden, vertices);
                    ++lods;
                }
            }
            catch (const std::exception &e)
            {
                throw std::runtime_error(name + ": " + e.what());
            }
        }
        require(hidden > 0, "hidden authored parts were not exercised");
        // Parent and child nonuniform/mirrored affine bases must compose without
        // discarding the child basis or changing its local translation.
        sgn::Pose parent{{0, 0, -2}, {0, 3, 0}, {-4, 0, 0}, {7, 8, 9}, 2};
        sgn::Pose child{{1, 0, 0}, {1, 2, 0}, {0, 0, 3}, {11, 12, 13}, 3};
        auto combined = sgn::compose_pose(parent, child);
        sgn::Vec3 vertex{5, 6, 7};
        auto direct = sgn::transform_direction(parent, sgn::transform_direction(child, vertex) + child.position) +
                      parent.position;
        auto composed = sgn::transform_direction(combined, vertex) + combined.position;
        require(direct.x == composed.x && direct.y == composed.y && direct.z == composed.z,
                "part pose composition lost affine state");
        std::cout << "PMD raw rig: lods=" << lods << " hidden_parts=" << hidden
                  << " authored_visible_vertices=" << vertices
                  << "; UV/color words exact; position tolerance 0.003+3e-6 relative (GPU affine regrouping)\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
