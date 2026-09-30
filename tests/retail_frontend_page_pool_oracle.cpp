#include "sgn/retail_frontend_page_pool.hpp"
#include <array>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
using namespace sgn::retail;
namespace {
using Image = std::array<std::byte, 0x20D0>;
void put(Image& image, std::size_t at, std::uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) image[at + i] = static_cast<std::byte>((value >> (i * 8)) & 255u);
}
void atomic_rejections() {
    for (unsigned test = 0; test < 7; ++test) {
        Image image{}; initialize_frontend_page_pool_43d430(image);
        std::uint32_t value = 24; bool release = false;
        switch (test) {
        case 0: put(image, 0x2080, 0); break;
        case 1: put(image, 0x2080, 17); break;
        case 2: value = 513; break;
        case 3: put(image, 0x2088 + 15 * 4, 0x5FD259); break;
        case 4: release = true; value = 0x5FD260; break;
        case 5: release = true; value = 0; break;
        case 6: release = true; value = allocate_frontend_page_object_43d470(image, 24);
                put(image, value - 0x5FD258 - 4, 513); break;
        }
        const auto before = image; bool rejected = false;
        try {
            if (release) release_frontend_page_object_43d4c0(image, value);
            else (void)allocate_frontend_page_object_43d470(image, value);
        } catch (const std::invalid_argument&) { rejected = true; }
        if (!rejected || before != image) throw std::runtime_error("page pool host rejection was not atomic");
    }
}
}
int main(int argc, char** argv) try {
    if (argc != 2) return 2;
    atomic_rejections();
    std::ifstream in(argv[1], std::ios::binary); if (!in) throw std::runtime_error("page pool corpus missing");
    const auto read = [&](void* out, std::size_t bytes) {
        if (!in.read(static_cast<char*>(out), static_cast<std::streamsize>(bytes)))
            throw std::runtime_error("truncated page pool corpus");
    };
    const auto word = [&] { std::uint32_t v; read(&v, 4); return v; };
    std::array<char, 8> header{}; read(header.data(), header.size());
    if (std::string_view(header.data(), header.size()) != "SGNFP001") throw std::runtime_error("page pool corpus schema");
    const auto suites = word(); if (!suites || suites > 10000) throw std::runtime_error("page pool corpus count");
    std::uint64_t steps = 0, compared = 0;
    for (std::uint32_t suite = 0; suite < suites; ++suite) {
        const auto count = word(); if (!count || count > 1000) throw std::runtime_error("page pool step count");
        Image actual{}, expected{}; read(actual.data(), actual.size());
        for (std::uint32_t step = 0; step < count; ++step) {
            const auto op = word(), value = word(), returned = word(); read(expected.data(), expected.size());
            std::uint32_t observed = 0;
            if (op == 1) initialize_frontend_page_pool_43d430(actual);
            else if (op == 2) observed = allocate_frontend_page_object_43d470(actual, value);
            else if (op == 3) release_frontend_page_object_43d4c0(actual, value);
            else throw std::runtime_error("page pool corpus operation");
            if (observed != returned || actual != expected)
                throw std::runtime_error("page pool mismatch suite=" + std::to_string(suite) + " step=" + std::to_string(step));
            ++steps; compared += actual.size() + 4;
        }
    }
    if (in.peek() != std::char_traits<char>::eof()) throw std::runtime_error("trailing page pool corpus");
    std::cout << "{\"passed\":true,\"errors\":0,\"suites\":" << suites << ",\"steps\":" << steps
              << ",\"exact_compared_bytes\":" << compared << ",\"host_atomic_rejections\":7}\n";
    return 0;
} catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
