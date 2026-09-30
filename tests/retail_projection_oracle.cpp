#include "sgn/retail_projection.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
std::uint32_t word(std::istream &in)
{
    std::array<unsigned char, 4> b{};
    if (!in.read(reinterpret_cast<char *>(b.data()), 4))
        throw std::runtime_error("truncated projection corpus");
    return b[0] | (std::uint32_t(b[1]) << 8) | (std::uint32_t(b[2]) << 16) | (std::uint32_t(b[3]) << 24);
}
float real(std::istream &in)
{
    return std::bit_cast<float>(word(in));
}
void require(bool ok, const char *text)
{
    if (!ok)
        throw std::runtime_error(text);
}
} // namespace
int main(int argc, char **argv)
{
    try
    {
        if (argc != 2)
            throw std::runtime_error("expected projection corpus");
        std::ifstream in(argv[1], std::ios::binary);
        char magic[8]{};
        in.read(magic, 8);
        require(std::string(magic, 8) == "SGNPRJ01", "wrong projection corpus version");
        const auto count = word(in), edge_count = word(in);
        require(count > 0 && edge_count > 0 && count <= 100000 && edge_count <= 100000,
                "empty or oversized projection corpus");
        std::size_t compared = 0;
        for (std::uint32_t i = 0; i < count; ++i)
        {
            std::array<float, 16> matrix{};
            for (auto &value : matrix)
                value = real(in);
            sgn::retail::ProjectionParameters parameters{real(in), real(in), real(in), real(in), real(in)};
            auto width = word(in), height = word(in);
            sgn::Vec3 vertex{real(in), real(in), real(in)};
            auto point = sgn::retail::project_retail_vertex(vertex, matrix, parameters);
            auto view = sgn::retail::retail_viewport(point, width, height);
            const std::array actual{
                std::bit_cast<std::uint32_t>(point.x),        std::bit_cast<std::uint32_t>(point.y),
                std::bit_cast<std::uint32_t>(point.z),        std::uint32_t(point.inside),
                std::bit_cast<std::uint32_t>(point.camera_z), std::bit_cast<std::uint32_t>(view.x),
                std::bit_cast<std::uint32_t>(view.y),         std::bit_cast<std::uint32_t>(view.reciprocal_depth)};
            for (unsigned k = 0; k < actual.size(); ++k)
            {
                auto expected = word(in);
                ++compared;
                if (expected != actual[k])
                    throw std::runtime_error("projection record " + std::to_string(i) + " word " + std::to_string(k) +
                                             " expected " + std::to_string(expected) + " actual " +
                                             std::to_string(actual[k]));
            }
        }
        for (std::uint32_t i = 0; i < edge_count; ++i)
        {
            auto plane = word(in);
            auto near = real(in);
            std::array<std::uint32_t, 12> a{}, b{}, expected{}, actual{};
            for (auto &value : a)
                value = word(in);
            for (auto &value : b)
                value = word(in);
            for (auto &value : expected)
                value = word(in);
            auto corner = [](const auto &value) {
                auto f = [&](unsigned j) { return std::bit_cast<float>(value[j]); };
                return sgn::retail::ClipCorner{f(0), f(1), value[3], {f(4), f(5), f(6), f(11), 0}};
            };
            auto out = sgn::retail::retail_edge_intersection(corner(a), corner(b), plane, near);
            actual.fill(0xcdcdcdcd);
            actual[0] = std::bit_cast<std::uint32_t>(out.u);
            actual[1] = std::bit_cast<std::uint32_t>(out.v);
            actual[3] = out.color;
            actual[4] = std::bit_cast<std::uint32_t>(out.point.x);
            actual[5] = std::bit_cast<std::uint32_t>(out.point.y);
            actual[6] = std::bit_cast<std::uint32_t>(out.point.z);
            actual[11] = std::bit_cast<std::uint32_t>(out.point.camera_z);
            actual[8] = (actual[8] & 0xffff0000u) | out.point.inside;
            for (unsigned k = 0; k < actual.size(); ++k)
            {
                ++compared;
                if (expected[k] != actual[k])
                    throw std::runtime_error("edge record " + std::to_string(i) + " word " + std::to_string(k) +
                                             " expected " + std::to_string(expected[k]) + " actual " +
                                             std::to_string(actual[k]));
            }
        }
        require(in.peek() == std::char_traits<char>::eof(), "trailing projection corpus");
        std::cout << "{\"passed\":true,\"points\":" << count << ",\"edges\":" << edge_count
                  << ",\"compared_words\":" << compared << ",\"tolerance_ulp\":0}\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
