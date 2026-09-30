#include "sgn/source_application_literals.hpp"
#include "sgn/source_462890_address.hpp"
#include <exception>
#include <charconv>

namespace sgn::retail
{
std::span<const std::uint8_t> SourceApplicationLiteralRef::vehicle_configuration_literal(std::uint32_t address) const &
{
    const auto identity = identity_.lock();
    if (!identity)
        throw std::logic_error("vehicle configuration App owner expired");
    const auto &owner = *identity->owner;
    owner.require_owner_thread();
    if (address - 0x4734b0u < owner.vehicle_config_literals_4734b0_.size())
        return std::span(owner.vehicle_config_literals_4734b0_).subspan(address - 0x4734b0u);
    if (address - 0x478b5cu < owner.camera_equals_478b5c_.size())
        return std::span(owner.camera_equals_478b5c_).subspan(address - 0x478b5cu);
    char digits[8];
    const auto converted = std::to_chars(digits, digits + sizeof(digits), address, 16);
    throw std::out_of_range("source configuration literal 0x" + std::string(digits, converted.ptr) +
                            " outside canonical App regions");
}
SourceApplicationLiteralData::SourceApplicationLiteralData()
    : identity_(std::make_shared<const detail::SourceApplicationLiteralIdentity>(
          detail::SourceApplicationLiteralIdentity{this, std::this_thread::get_id()})),
      // Pinned EXE4a1d234c..., PE offset489468, VA4777FC..477809.
      // Exact14 bytes SHA25608eb3d665d33fedf1afb44eb5365e54ee0b99864803ccdcc0571baf0caa628a8.
      bytes_4777fc_{{0x41, 0x6e, 0x69, 0x6d, 0x53, 0x6b, 0x79, 0x47, 0x72, 0x6f, 0x75, 0x6e, 0x64, 0x00}},
      // Pinned EXE4a1d234c..., PE offset489836, VA47796C..477982.
      // Exact23 bytes SHA2564abebfc5ff2247c643ab66e44a244afe9a8bbbd70a265a29b2221f020c77bdd2.
      bytes_47796c_{{0x25, 0x73, 0x5c, 0x43, 0x6f, 0x6e, 0x66, 0x69, 0x67, 0x5c, 0x4c, 0x76,
                     0x53, 0x63, 0x72, 0x69, 0x70, 0x74, 0x2e, 0x63, 0x66, 0x67, 0x00}},
      // Pinned EXE4a1d234c..., PE offset460200, VA4705A8..470600 (.rdata).
      // Exact89 bytes SHA256dc1a434091c9556591738b4d80d697c4e4d915c53fc9c26667938b7cb1f48f17.
      // clang-format off: exact .rdata byte dump 4705A8, 16 bytes per row
     bytes_4705a8_{{
         0x06,0x00,0x00,0x06,0x00,0x01,0x00,0x00,0x10,0x00,0x03,0x06,0x00,0x06,0x02,0x10,
         0x04,0x45,0x45,0x45,0x05,0x05,0x05,0x05,0x05,0x35,0x30,0x00,0x50,0x00,0x00,0x00,
         0x00,0x20,0x28,0x38,0x50,0x58,0x07,0x08,0x00,0x37,0x30,0x30,0x57,0x50,0x07,0x00,
         0x00,0x20,0x20,0x08,0x00,0x00,0x00,0x00,0x08,0x60,0x68,0x60,0x60,0x60,0x60,0x00,
         0x00,0x70,0x70,0x78,0x78,0x78,0x78,0x08,0x07,0x08,0x00,0x00,0x07,0x00,0x08,0x08,
         0x08,0x00,0x00,0x08,0x00,0x08,0x00,0x07,0x08}},
      // clang-format on
      dispatch_464e60_{{0xde, 0x48, 0x46, 0x00, 0xb4, 0x47, 0x46, 0x00, 0xcf, 0x47, 0x46,
                        0x00, 0x1b, 0x48, 0x46, 0x00, 0x52, 0x48, 0x46, 0x00, 0x5a, 0x48,
                        0x46, 0x00, 0x8f, 0x48, 0x46, 0x00, 0x22, 0x49, 0x46, 0x00}},
      null_string_470614_{{0x28, 0x6e, 0x75, 0x6c, 0x6c, 0x29, 0x00}},
      formatter_locale_{
          0x495dba,
          {{0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00,
            0x28, 0x00, 0x28, 0x00, 0x28, 0x00, 0x28, 0x00, 0x28, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00,
            0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00,
            0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x48, 0x00, 0x10, 0x00, 0x10, 0x00, 0x10, 0x00,
            0x10, 0x00, 0x10, 0x00, 0x10, 0x00, 0x10, 0x00, 0x10, 0x00, 0x10, 0x00, 0x10, 0x00, 0x10, 0x00, 0x10, 0x00,
            0x10, 0x00, 0x10, 0x00, 0x10, 0x00, 0x84, 0x00, 0x84, 0x00, 0x84, 0x00, 0x84, 0x00, 0x84, 0x00, 0x84, 0x00,
            0x84, 0x00, 0x84, 0x00, 0x84, 0x00, 0x84, 0x00, 0x10, 0x00, 0x10, 0x00, 0x10, 0x00, 0x10, 0x00, 0x10, 0x00,
            0x10, 0x00, 0x10, 0x00, 0x81, 0x00, 0x81, 0x00, 0x81, 0x00, 0x81, 0x00, 0x81, 0x00, 0x81, 0x00, 0x01, 0x00,
            0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00,
            0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00,
            0x01, 0x00, 0x10, 0x00, 0x10, 0x00, 0x10, 0x00, 0x10, 0x00, 0x10, 0x00, 0x10, 0x00, 0x82, 0x00, 0x82, 0x00,
            0x82, 0x00, 0x82, 0x00, 0x82, 0x00, 0x82, 0x00, 0x02, 0x00, 0x02, 0x00, 0x02, 0x00, 0x02, 0x00, 0x02, 0x00,
            0x02, 0x00, 0x02, 0x00, 0x02, 0x00, 0x02, 0x00, 0x02, 0x00, 0x02, 0x00, 0x02, 0x00, 0x02, 0x00, 0x02, 0x00,
            0x02, 0x00, 0x02, 0x00, 0x02, 0x00, 0x02, 0x00, 0x02, 0x00, 0x02, 0x00, 0x10, 0x00, 0x10, 0x00, 0x10, 0x00,
            0x10, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
          0x470614,
          1},
      // PE.data482B88..482F33, pointer and text bytes; not an index clamp.
      catalog_bytes_482b88_{
          {0xb8, 0x2c, 0x48, 0x00, 0xc0, 0x2c, 0x48, 0x00, 0xc8, 0x2c, 0x48, 0x00, 0xd0, 0x2c, 0x48, 0x00, 0xd8, 0x2c,
           0x48, 0x00, 0xe0, 0x2c, 0x48, 0x00, 0xe8, 0x2c, 0x48, 0x00, 0xf0, 0x2c, 0x48, 0x00, 0xf8, 0x2c, 0x48, 0x00,
           0x00, 0x2d, 0x48, 0x00, 0x08, 0x2d, 0x48, 0x00, 0x10, 0x2d, 0x48, 0x00, 0x18, 0x2d, 0x48, 0x00, 0x20, 0x2d,
           0x48, 0x00, 0x28, 0x2d, 0x48, 0x00, 0x30, 0x2d, 0x48, 0x00, 0x38, 0x2d, 0x48, 0x00, 0x40, 0x2d, 0x48, 0x00,
           0x48, 0x2d, 0x48, 0x00, 0x50, 0x2d, 0x48, 0x00, 0x58, 0x2d, 0x48, 0x00, 0x60, 0x2d, 0x48, 0x00, 0x68, 0x2d,
           0x48, 0x00, 0x70, 0x2d, 0x48, 0x00, 0x78, 0x2d, 0x48, 0x00, 0x80, 0x2d, 0x48, 0x00, 0x8c, 0x2d, 0x48, 0x00,
           0x98, 0x2d, 0x48, 0x00, 0xa4, 0x2d, 0x48, 0x00, 0xb0, 0x2d, 0x48, 0x00, 0xbc, 0x2d, 0x48, 0x00, 0xc8, 0x2d,
           0x48, 0x00, 0xd4, 0x2d, 0x48, 0x00, 0xdc, 0x2d, 0x48, 0x00, 0xe4, 0x2d, 0x48, 0x00, 0xec, 0x2d, 0x48, 0x00,
           0xf4, 0x2d, 0x48, 0x00, 0xfc, 0x2d, 0x48, 0x00, 0x04, 0x2e, 0x48, 0x00, 0x0c, 0x2e, 0x48, 0x00, 0x14, 0x2e,
           0x48, 0x00, 0x1c, 0x2e, 0x48, 0x00, 0x24, 0x2e, 0x48, 0x00, 0x2c, 0x2e, 0x48, 0x00, 0x34, 0x2e, 0x48, 0x00,
           0x3c, 0x2e, 0x48, 0x00, 0x44, 0x2e, 0x48, 0x00, 0x4c, 0x2e, 0x48, 0x00, 0x54, 0x2e, 0x48, 0x00, 0x5c, 0x2e,
           0x48, 0x00, 0x64, 0x2e, 0x48, 0x00, 0x6c, 0x2e, 0x48, 0x00, 0x74, 0x2e, 0x48, 0x00, 0x7c, 0x2e, 0x48, 0x00,
           0x84, 0x2e, 0x48, 0x00, 0x8c, 0x2e, 0x48, 0x00, 0x94, 0x2e, 0x48, 0x00, 0x9c, 0x2e, 0x48, 0x00, 0xa4, 0x2e,
           0x48, 0x00, 0xac, 0x2e, 0x48, 0x00, 0xb4, 0x2e, 0x48, 0x00, 0xbc, 0x2e, 0x48, 0x00, 0xc4, 0x2e, 0x48, 0x00,
           0xcc, 0x2e, 0x48, 0x00, 0xd4, 0x2e, 0x48, 0x00, 0xdc, 0x2e, 0x48, 0x00, 0xe4, 0x2e, 0x48, 0x00, 0xec, 0x2e,
           0x48, 0x00, 0xf4, 0x2e, 0x48, 0x00, 0xfc, 0x2e, 0x48, 0x00, 0x04, 0x2f, 0x48, 0x00, 0x0c, 0x2f, 0x48, 0x00,
           0x14, 0x2f, 0x48, 0x00, 0x1c, 0x2f, 0x48, 0x00, 0x24, 0x2f, 0x48, 0x00, 0x2c, 0x2f, 0x48, 0x00, 0x54, 0x52,
           0x41, 0x43, 0x4b, 0x31, 0x00, 0x00, 0x54, 0x52, 0x41, 0x43, 0x4b, 0x32, 0x00, 0x00, 0x54, 0x52, 0x41, 0x43,
           0x4b, 0x33, 0x00, 0x00, 0x54, 0x52, 0x41, 0x43, 0x4b, 0x34, 0x00, 0x00, 0x54, 0x52, 0x41, 0x43, 0x4b, 0x35,
           0x00, 0x00, 0x54, 0x52, 0x41, 0x43, 0x4b, 0x36, 0x00, 0x00, 0x54, 0x52, 0x41, 0x43, 0x4b, 0x37, 0x00, 0x00,
           0x54, 0x52, 0x41, 0x43, 0x4b, 0x38, 0x00, 0x00, 0x54, 0x52, 0x41, 0x43, 0x4b, 0x39, 0x00, 0x00, 0x54, 0x52,
           0x41, 0x43, 0x4b, 0x31, 0x30, 0x00, 0x54, 0x52, 0x41, 0x43, 0x4b, 0x31, 0x31, 0x00, 0x54, 0x52, 0x41, 0x43,
           0x4b, 0x31, 0x32, 0x00, 0x54, 0x52, 0x41, 0x43, 0x4b, 0x31, 0x33, 0x00, 0x54, 0x52, 0x41, 0x43, 0x4b, 0x31,
           0x34, 0x00, 0x54, 0x52, 0x41, 0x43, 0x4b, 0x31, 0x35, 0x00, 0x54, 0x52, 0x41, 0x43, 0x4b, 0x31, 0x36, 0x00,
           0x54, 0x52, 0x41, 0x43, 0x4b, 0x31, 0x37, 0x00, 0x54, 0x52, 0x41, 0x43, 0x4b, 0x31, 0x38, 0x00, 0x54, 0x52,
           0x41, 0x43, 0x4b, 0x31, 0x39, 0x00, 0x54, 0x52, 0x41, 0x43, 0x4b, 0x32, 0x30, 0x00, 0x54, 0x52, 0x41, 0x43,
           0x4b, 0x32, 0x31, 0x00, 0x54, 0x52, 0x41, 0x43, 0x4b, 0x32, 0x32, 0x00, 0x54, 0x52, 0x41, 0x43, 0x4b, 0x32,
           0x33, 0x00, 0x54, 0x52, 0x41, 0x43, 0x4b, 0x32, 0x34, 0x00, 0x43, 0x48, 0x41, 0x4c, 0x4c, 0x31, 0x00, 0x00,
           0x54, 0x52, 0x41, 0x43, 0x4b, 0x53, 0x45, 0x54, 0x30, 0x30, 0x00, 0x00, 0x54, 0x52, 0x41, 0x43, 0x4b, 0x53,
           0x45, 0x54, 0x30, 0x31, 0x00, 0x00, 0x54, 0x52, 0x41, 0x43, 0x4b, 0x53, 0x45, 0x54, 0x30, 0x32, 0x00, 0x00,
           0x54, 0x52, 0x41, 0x43, 0x4b, 0x53, 0x45, 0x54, 0x30, 0x33, 0x00, 0x00, 0x54, 0x52, 0x41, 0x43, 0x4b, 0x53,
           0x45, 0x54, 0x30, 0x34, 0x00, 0x00, 0x54, 0x52, 0x41, 0x43, 0x4b, 0x53, 0x45, 0x54, 0x30, 0x35, 0x00, 0x00,
           0x54, 0x52, 0x41, 0x43, 0x4b, 0x53, 0x45, 0x54, 0x30, 0x36, 0x00, 0x00, 0x44, 0x55, 0x4d, 0x4d, 0x59, 0x00,
           0x00, 0x00, 0x44, 0x55, 0x4d, 0x4d, 0x59, 0x00, 0x00, 0x00, 0x43, 0x61, 0x72, 0x31, 0x00, 0x00, 0x00, 0x00,
           0x43, 0x61, 0x72, 0x31, 0x73, 0x68, 0x00, 0x00, 0x43, 0x61, 0x72, 0x32, 0x00, 0x00, 0x00, 0x00, 0x43, 0x61,
           0x72, 0x32, 0x73, 0x68, 0x00, 0x00, 0x43, 0x61, 0x72, 0x33, 0x00, 0x00, 0x00, 0x00, 0x43, 0x61, 0x72, 0x33,
           0x73, 0x68, 0x00, 0x00, 0x43, 0x61, 0x72, 0x34, 0x00, 0x00, 0x00, 0x00, 0x43, 0x61, 0x72, 0x34, 0x73, 0x68,
           0x00, 0x00, 0x43, 0x61, 0x72, 0x35, 0x00, 0x00, 0x00, 0x00, 0x43, 0x61, 0x72, 0x35, 0x73, 0x68, 0x00, 0x00,
           0x43, 0x61, 0x72, 0x36, 0x00, 0x00, 0x00, 0x00, 0x43, 0x61, 0x72, 0x36, 0x73, 0x68, 0x00, 0x00, 0x43, 0x61,
           0x72, 0x37, 0x00, 0x00, 0x00, 0x00, 0x43, 0x61, 0x72, 0x37, 0x73, 0x68, 0x00, 0x00, 0x43, 0x61, 0x72, 0x38,
           0x00, 0x00, 0x00, 0x00, 0x43, 0x61, 0x72, 0x38, 0x73, 0x68, 0x00, 0x00, 0x43, 0x61, 0x72, 0x39, 0x00, 0x00,
           0x00, 0x00, 0x43, 0x61, 0x72, 0x39, 0x73, 0x68, 0x00, 0x00, 0x43, 0x61, 0x72, 0x31, 0x30, 0x00, 0x00, 0x00,
           0x43, 0x61, 0x72, 0x31, 0x30, 0x73, 0x68, 0x00, 0x43, 0x61, 0x72, 0x31, 0x31, 0x00, 0x00, 0x00, 0x43, 0x61,
           0x72, 0x31, 0x31, 0x73, 0x68, 0x00, 0x43, 0x61, 0x72, 0x31, 0x32, 0x00, 0x00, 0x00, 0x43, 0x61, 0x72, 0x31,
           0x32, 0x73, 0x68, 0x00, 0x43, 0x61, 0x72, 0x31, 0x33, 0x00, 0x00, 0x00, 0x43, 0x61, 0x72, 0x31, 0x33, 0x73,
           0x68, 0x00, 0x43, 0x61, 0x72, 0x31, 0x34, 0x00, 0x00, 0x00, 0x43, 0x61, 0x72, 0x31, 0x34, 0x73, 0x68, 0x00,
           0x43, 0x61, 0x72, 0x31, 0x35, 0x00, 0x00, 0x00, 0x43, 0x61, 0x72, 0x31, 0x35, 0x73, 0x68, 0x00, 0x43, 0x61,
           0x72, 0x31, 0x36, 0x00, 0x00, 0x00, 0x43, 0x61, 0x72, 0x31, 0x36, 0x73, 0x68, 0x00, 0x43, 0x61, 0x72, 0x31,
           0x37, 0x00, 0x00, 0x00, 0x43, 0x61, 0x72, 0x31, 0x37, 0x73, 0x68, 0x00, 0x43, 0x61, 0x72, 0x31, 0x38, 0x00,
           0x00, 0x00, 0x43, 0x61, 0x72, 0x31, 0x38, 0x73, 0x68, 0x00, 0x43, 0x61, 0x72, 0x31, 0x39, 0x00, 0x00, 0x00,
           0x43, 0x61, 0x72, 0x31, 0x39, 0x73, 0x68, 0x00, 0x43, 0x61, 0x72, 0x32, 0x30, 0x00, 0x00, 0x00, 0x43, 0x61,
           0x72, 0x32, 0x30, 0x73, 0x68, 0x00, 0x43, 0x61, 0x72, 0x34, 0x00, 0x00, 0x00, 0x00, 0x43, 0x61, 0x72, 0x34,
           0x73, 0x68, 0x00, 0x00}},
      // Exact mutable PE.data47782C..47788B: six16-byte car key formats.
      car_key_formats_47782c_{{0x4c, 0x5f, 0x43, 0x41, 0x52, 0x25, 0x64, 0x5f, 0x43, 0x4f, 0x4e, 0x46, 0x49, 0x47,
                               0x00, 0x00, 0x4c, 0x5f, 0x43, 0x41, 0x52, 0x25, 0x64, 0x5f, 0x54, 0x59, 0x52, 0x45,
                               0x00, 0x00, 0x00, 0x00, 0x4c, 0x5f, 0x43, 0x41, 0x52, 0x25, 0x64, 0x5f, 0x4c, 0x49,
                               0x56, 0x45, 0x52, 0x59, 0x00, 0x00, 0x4c, 0x5f, 0x43, 0x41, 0x52, 0x25, 0x64, 0x5f,
                               0x52, 0x45, 0x50, 0x4c, 0x41, 0x59, 0x00, 0x00, 0x4c, 0x5f, 0x43, 0x41, 0x52, 0x25,
                               0x64, 0x5f, 0x41, 0x49, 0x53, 0x4b, 0x49, 0x4c, 0x4c, 0x00, 0x4c, 0x5f, 0x43, 0x41,
                               0x52, 0x25, 0x64, 0x5f, 0x41, 0x49, 0x4d, 0x4f, 0x44, 0x45, 0x00, 0x00}},
      level_equals_4777f0_{{0x3d, 0x00}}, level_end_477990_{{0x2d, 0x45, 0x4e, 0x44, 0x00}},
      version_inputs_482698_{{0x31, 0x36, 0x3a, 0x32, 0x36, 0x3a, 0x35, 0x30, 0x00, 0x00, 0x00, 0x00, 0x4e, 0x6f, 0x76,
                              0x20, 0x32, 0x31, 0x20, 0x32, 0x30, 0x30, 0x30, 0x00, 0x56, 0x65, 0x72, 0x73, 0x69, 0x6f,
                              0x6e, 0x3a, 0x20, 0x25, 0x73, 0x20, 0x2d, 0x20, 0x25, 0x73, 0x00, 0x00, 0x00, 0x00}},
      // PE .data virtual zero-fill, before the independently written612CF8.
      version_bytes_612bf8_{}, rolling_demo_{},
      rolling_literals_477944_{{0x2d, 0x52, 0x4f, 0x4c, 0x4c, 0x49, 0x4e, 0x47, 0x44, 0x45, 0x4d, 0x4f, 0x4c, 0x49,
                                0x53, 0x54, 0x00, 0x00, 0x00, 0x00, 0x2d, 0x45, 0x4e, 0x44, 0x00, 0x00, 0x00, 0x00,
                                0x44, 0x45, 0x4d, 0x4f, 0x53, 0x43, 0x52, 0x49, 0x50, 0x54, 0x00, 0x00}},
      level_sky_4777f4_{{0x4c, 0x5f, 0x53, 0x4b, 0x59, 0x00}},
      level_track_47780c_{{0x4c, 0x5f, 0x54, 0x52, 0x41, 0x43, 0x4b, 0x00}},
      level_setting_literals_47788c_{
          {0x54, 0x52, 0x41, 0x43, 0x4b, 0x54, 0x45, 0x58, 0x53, 0x45, 0x54, 0x00, 0x4d, 0x55, 0x53, 0x5f, 0x54,
           0x52, 0x41, 0x43, 0x4b, 0x00, 0x00, 0x00, 0x47, 0x52, 0x4f, 0x55, 0x4e, 0x44, 0x4c, 0x45, 0x56, 0x45,
           0x4c, 0x00, 0x47, 0x41, 0x4d, 0x45, 0x54, 0x59, 0x50, 0x45, 0x00, 0x00, 0x00, 0x00, 0x52, 0x41, 0x43,
           0x45, 0x5f, 0x54, 0x59, 0x50, 0x45, 0x5f, 0x31, 0x00, 0x43, 0x48, 0x41, 0x4c, 0x4c, 0x45, 0x4e, 0x47,
           0x45, 0x5f, 0x54, 0x59, 0x50, 0x45, 0x5f, 0x31, 0x00, 0x00, 0x00, 0x00, 0x54, 0x49, 0x4d, 0x45, 0x54,
           0x52, 0x49, 0x41, 0x4c, 0x5f, 0x54, 0x59, 0x50, 0x45, 0x5f, 0x31, 0x00, 0x00, 0x00, 0x00, 0x54, 0x49,
           0x4d, 0x45, 0x00, 0x00, 0x00, 0x00, 0x4c, 0x41, 0x50, 0x43, 0x4f, 0x55, 0x4e, 0x54, 0x00, 0x00, 0x00,
           0x00, 0x4c, 0x41, 0x50, 0x52, 0x45, 0x43, 0x4f, 0x52, 0x44, 0x00, 0x00, 0x00, 0x52, 0x4f, 0x4c, 0x53,
           0x54, 0x41, 0x52, 0x54, 0x4f, 0x46, 0x46, 0x53, 0x45, 0x54, 0x00, 0x00, 0x4f, 0x56, 0x45, 0x52, 0x4c,
           0x41, 0x59, 0x44, 0x45, 0x4d, 0x4f, 0x00, 0x43, 0x41, 0x4d, 0x45, 0x52, 0x41, 0x4d, 0x4f, 0x44, 0x45,
           0x00, 0x00, 0x52, 0x5f, 0x44, 0x45, 0x4d, 0x4f, 0x4c, 0x4f, 0x4f, 0x50, 0x00, 0x00}},
      level_scale_46e2f0_{{0x00, 0x00, 0xc8, 0x42}}
{
}
SourceApplicationLiteralData::~SourceApplicationLiteralData()
{
    if (std::this_thread::get_id() != identity_->thread)
        std::terminate();
}
void SourceApplicationLiteralData::require_owner_thread() const
{
    if (std::this_thread::get_id() != identity_->thread)
        throw std::logic_error("application source literals belong to the creating thread");
}
SourceApplicationLiteralRef SourceApplicationLiteralData::reference() const &
{
    require_owner_thread();
    return SourceApplicationLiteralRef(identity_);
}
std::span<std::uint8_t, 14> SourceApplicationLiteralData::mutable_bytes_4777fc() &
{
    require_owner_thread();
    return bytes_4777fc_;
}
std::span<std::uint8_t, 23> SourceApplicationLiteralData::mutable_bytes_47796c() &
{
    require_owner_thread();
    return bytes_47796c_;
}
std::uint8_t SourceApplicationLiteralRef::read_u8_464750(std::uint32_t address) const
{
    constexpr std::uint32_t site = 0x464750;
    const auto identity = identity_.lock();
    if (!identity)
        throw SourceLiteralReadError(site, address, "expired application source literals");
    if (std::this_thread::get_id() != identity->thread)
        throw SourceLiteralReadError(site, address, "application source literals belong to another thread");
    const auto offset = static_cast<std::uint32_t>(address - SourceApplicationLiteralData::address_47796c);
    if (offset >= SourceApplicationLiteralData::extent_47796c)
        throw SourceLiteralReadError(site, address, "reached read outside admitted47796C..477982 storage");
    return identity->owner->bytes_47796c_[offset];
}
std::uint8_t SourceApplicationLiteralRef::read_u8_464789(std::uint32_t address) const
{
    constexpr std::uint32_t site = 0x464789;
    const auto identity = identity_.lock();
    if (!identity)
        throw SourceLiteralReadError(site, address, "expired application source literals");
    if (std::this_thread::get_id() != identity->thread)
        throw SourceLiteralReadError(site, address, "application source literals belong to another thread");
    const auto offset = static_cast<std::uint32_t>(address - SourceApplicationLiteralData::address_4705a8);
    if (offset >= SourceApplicationLiteralData::extent_4705a8)
        throw SourceLiteralReadError(site, address, "reached read outside admitted4705A8..470600 storage");
    return identity->owner->bytes_4705a8_[offset];
}
std::uint32_t SourceApplicationLiteralRef::classify_u8_46477c(std::uint8_t bits) const
{
    // Explicit signed BL interpretation, without implementation-defined int8 casts.
    const auto character = bits < 0x80u ? static_cast<std::int32_t>(bits) : static_cast<std::int32_t>(bits) - 0x100;
    if (character < 0x20)
        return 0; // 46477C/46477F ->464794, no owner read.
    if (character > 0x78)
        return 0;                                                           // 464781/464784 ->464794, no owner read.
    const auto address = 0x470588u + static_cast<std::uint32_t>(character); // 464786
    return static_cast<std::uint32_t>(read_u8_464789(address)) & 0xfu;      // 464789/46478F
}
std::uint8_t SourceApplicationLiteralRef::read_u8_464796(std::uint32_t address) const
{
    constexpr std::uint32_t site = 0x464796;
    const auto identity = identity_.lock();
    if (!identity)
        throw SourceLiteralReadError(site, address, "expired application source literals");
    if (std::this_thread::get_id() != identity->thread)
        throw SourceLiteralReadError(site, address, "application source literals belong to another thread");
    const auto offset = static_cast<std::uint32_t>(address - SourceApplicationLiteralData::address_4705a8);
    if (offset >= SourceApplicationLiteralData::extent_4705a8)
        throw SourceLiteralReadError(site, address, "reached read outside admitted4705A8..470600 storage");
    return identity->owner->bytes_4705a8_[offset];
}
source_46473f::StateBranch SourceApplicationLiteralRef::consume_state_464796(
    std::uint32_t esi_bits, std::uint32_t &eax_bits, std::uint32_t &local_state_ebp_minus30) const
{
    const auto address = static_cast<std::uint32_t>(0x4705a8u + esi_bits + eax_bits * 8u);
    const auto raw = read_u8_464796(address); // 464796 read must succeed before EAX changes.
    eax_bits = source_46473f::movsx_u8_464796(raw);
    return source_46473f::state_tail_46479e(eax_bits, local_state_ebp_minus30);
}
SourceFormatterLocaleData &SourceApplicationLiteralData::formatter_locale() &
{
    require_owner_thread();
    return formatter_locale_;
}
std::uint8_t SourceApplicationLiteralRef::read_formatter_byte(std::uint32_t address, std::uint32_t site) const
{
    const auto identity = identity_.lock();
    if (!identity)
        throw SourceLiteralReadError(site, address, "expired formatter App owner");
    if (std::this_thread::get_id() != identity->thread)
        throw SourceLiteralReadError(site, address, "formatter App belongs to another thread");
    const auto &owner = *identity->owner;
    if (site == 0x464750 || site == 0x464e4a || site == 0x464901 || site == 0x464ba8 || site == 0x464efc)
    {
        const auto root_offset = static_cast<std::uint32_t>(address - 0x477c34u);
        if (root_offset < owner.graphics_root_format_477c34_.size())
            return owner.graphics_root_format_477c34_[root_offset];
        const auto version_offset = static_cast<std::uint32_t>(address - 0x482698u);
        if (version_offset < owner.version_inputs_482698_.size())
            return owner.version_inputs_482698_[version_offset];
    }
    if (site == 0x464750 || site == 0x464e4a || site == 0x464901)
    {
        const auto notice_473a1c = address - 0x473a1cu;
        if (notice_473a1c < owner.low_energy_notice_473a1c_.size())
            return owner.low_energy_notice_473a1c_[notice_473a1c];
        const auto notice_474668 = address - 0x474668u;
        if (notice_474668 < owner.respawn_notice_474668_.size())
            return owner.respawn_notice_474668_[notice_474668];
        const auto notice_47468c = address - 0x47468cu;
        if (notice_47468c < owner.respawn_notice_47468c_.size())
            return owner.respawn_notice_47468c_[notice_47468c];
        const auto notice_474684 = address - 0x474684u; // R17 rail 419110 format
        if (notice_474684 < owner.respawn_notice_474684_.size())
            return owner.respawn_notice_474684_[notice_474684];
        const auto score_format_offset = static_cast<std::uint32_t>(address - 0x47220cu);
        if (score_format_offset < owner.score_format_47220c_.size())
            return owner.score_format_47220c_[score_format_offset];
        const auto result_format_offset = static_cast<std::uint32_t>(address - 0x482b44u);
        if (result_format_offset < owner.race_result_formats_482b44_.size())
            return owner.race_result_formats_482b44_[result_format_offset];
        const auto replay_name_offset = static_cast<std::uint32_t>(address - 0x482714u);
        if (replay_name_offset < owner.replay_name_format_482714_.size())
            return owner.replay_name_format_482714_[replay_name_offset];
        const auto replay_path_offset = static_cast<std::uint32_t>(address - 0x474698u);
        if (replay_path_offset < owner.replay_path_format_474698_.size())
            return owner.replay_path_format_474698_[replay_path_offset];
        const auto offset = static_cast<std::uint32_t>(address - 0x47796cu);
        if (offset < 23)
            return owner.bytes_47796c_[offset];
        const auto key_offset = static_cast<std::uint32_t>(address - 0x47782cu);
        if (key_offset < owner.car_key_formats_47782c_.size())
            return owner.car_key_formats_47782c_[key_offset];
        const auto catalog_offset = static_cast<std::uint32_t>(address - 0x482b88u);
        if (catalog_offset < owner.catalog_bytes_482b88_.size())
            return owner.catalog_bytes_482b88_[catalog_offset];
    }
    if (site == 0x4648ea)
    {
        const auto offset = static_cast<std::uint32_t>(address - 0x495dbau);
        if (offset < 512)
            return owner.formatter_locale_.ctype_bytes_495dba[offset];
    }
    if (site == 0x464ba8 || site == 0x464efc)
    {
        const auto offset = static_cast<std::uint32_t>(address - 0x470614u);
        if (offset < 7)
            return owner.null_string_470614_[offset];
    }
    throw SourceLiteralReadError(site, address, "unowned reached formatter byte");
}
std::uint32_t SourceApplicationLiteralRef::read_formatter_word(std::uint32_t address, std::uint32_t site) const
{
    const auto identity = identity_.lock();
    if (!identity)
        throw SourceLiteralReadError(site, address, "expired formatter App owner");
    if (std::this_thread::get_id() != identity->thread)
        throw SourceLiteralReadError(site, address, "formatter App belongs to another thread");
    const auto &owner = *identity->owner;
    if (site == 0x4648de && address == 0x495db0)
        return owner.formatter_locale_.ctype_pointer_495db0;
    if (site == 0x464b96 && address == 0x4959f8)
        return owner.formatter_locale_.null_string_pointer_4959f8;
    const auto offset = static_cast<std::uint32_t>(address - 0x464e60u);
    if (site == 0x4647ad && offset <= 28)
    {
        const auto &bytes = owner.dispatch_464e60_;
        return static_cast<std::uint32_t>(bytes[offset]) | (static_cast<std::uint32_t>(bytes[offset + 1]) << 8u) |
               (static_cast<std::uint32_t>(bytes[offset + 2]) << 16u) |
               (static_cast<std::uint32_t>(bytes[offset + 3]) << 24u);
    }
    throw SourceLiteralReadError(site, address, "unowned reached formatter DWORD");
}
std::uint8_t SourceApplicationLiteralRef::read_u8_462890(std::uint32_t address, std::uint32_t site) const
{
    if (!source_462890::needle_byte_site(site))
        throw SourceLiteralReadError(site, address, "not a reached462890 needle byte site");
    const auto identity = identity_.lock();
    if (!identity)
        throw SourceLiteralReadError(site, address, "expired application source literals");
    if (std::this_thread::get_id() != identity->thread)
        throw SourceLiteralReadError(site, address, "application source literals belong to another thread");
    const auto offset = source_462890::difference(address, SourceApplicationLiteralData::address_4777fc);
    if (offset >= SourceApplicationLiteralData::extent_4777fc)
        throw SourceLiteralReadError(site, address, "reached read outside admitted4777FC..477809 storage");
    return identity->owner->bytes_4777fc_[offset];
}
} // namespace sgn::retail

namespace sgn::retail
{
std::span<std::uint8_t, 940> SourceApplicationLiteralData::mutable_catalog_bytes_482b88() &
{
    require_owner_thread();
    return catalog_bytes_482b88_;
}
std::uint32_t SourceApplicationLiteralRef::read_selected_script_pointer_4452f9(std::uint32_t address) const
{
    constexpr std::uint32_t site = 0x4452f9;
    const auto identity = identity_.lock();
    if (!identity)
        throw SourceLiteralReadError(site, address, "expired selected-script App owner");
    if (std::this_thread::get_id() != identity->thread)
        throw SourceLiteralReadError(site, address, "selected-script App belongs to another thread");
    const auto offset = static_cast<std::uint32_t>(address - 0x482b88u);
    const auto &bytes = identity->owner->catalog_bytes_482b88_;
    if (offset > bytes.size() - 4)
        throw SourceLiteralReadError(site, address, "selected-script DWORD reaches unowned PE storage");
    return static_cast<std::uint32_t>(bytes[offset]) | (static_cast<std::uint32_t>(bytes[offset + 1]) << 8u) |
           (static_cast<std::uint32_t>(bytes[offset + 2]) << 16u) |
           (static_cast<std::uint32_t>(bytes[offset + 3]) << 24u);
}
} // namespace sgn::retail

namespace sgn::retail
{
std::span<std::uint8_t, 96> SourceApplicationLiteralData::mutable_car_key_formats_47782c() &
{
    require_owner_thread();
    return car_key_formats_47782c_;
}
std::uint32_t SourceApplicationLiteralRef::classify_number_4631f4(std::uint8_t character, std::uint32_t mask) const
{
    const bool multibyte = number_uses_multibyte_4631f4(mask);
    return classify_number_4631f4(character, mask, multibyte);
}
bool SourceApplicationLiteralRef::number_uses_multibyte_4631f4(std::uint32_t mask) const
{
    if (mask != 4u && mask != 8u)
        throw std::invalid_argument("4631F4 calls classification with mask4 or8 only");
    const auto site = mask == 8u ? 0x4631fcu : 0x463240u;
    const auto identity = identity_.lock();
    if (!identity)
        throw SourceLiteralReadError(site, 0x495fbc, "expired numeric width App owner");
    if (std::this_thread::get_id() != identity->thread)
        throw SourceLiteralReadError(site, 0x495fbc, "numeric width App belongs to another thread");
    const auto width = identity->owner->formatter_locale_.multibyte_width_495fbc;
    return width > 1u && (width & 0x80000000u) == 0; // Actual signed CMP/JLE1.
}
std::uint32_t SourceApplicationLiteralRef::classify_number_4631f4(std::uint8_t character, std::uint32_t mask,
                                                                  bool multibyte) const
{
    if (mask != 4u && mask != 8u)
        throw std::invalid_argument("4631F4 calls classification with mask4 or8 only");
    const auto pointer_site = multibyte ? 0x4682ddu : (mask == 8u ? 0x463217u : 0x463255u);
    const auto identity = identity_.lock();
    if (!identity)
        throw SourceLiteralReadError(pointer_site, 0x495db0, "expired numeric ctype App owner");
    if (std::this_thread::get_id() != identity->thread)
        throw SourceLiteralReadError(pointer_site, 0x495db0, "numeric ctype App belongs to another thread");
    const auto &locale = identity->owner->formatter_locale_;
    // 463217/463255 or4682DD load the SAME current495DB0 pointer.
    // For4682CB, byte+1 is1..256, so4682DB JA is never taken.
    const auto address = locale.ctype_pointer_495db0 + static_cast<std::uint32_t>(character) * 2u;
    const auto offset = static_cast<std::uint32_t>(address - 0x495dbau);
    const auto site = multibyte ? 0x4682e3u : (mask == 8u ? 0x46321du : 0x46325au);
    const auto &bytes = locale.ctype_bytes_495dba;
    if (offset >= bytes.size() || (multibyte && offset >= bytes.size() - 1))
        throw SourceLiteralReadError(site, address, "reached unowned numeric ctype read");
    auto classification = static_cast<std::uint32_t>(bytes[offset]);
    if (multibyte)
        classification |= static_cast<std::uint32_t>(bytes[offset + 1]) << 8u;
    // 463220/46325D or46833B. Only low masks4/8 are actually supplied.
    return classification & mask;
}
} // namespace sgn::retail

namespace sgn::retail
{
std::span<std::uint8_t, 2> SourceApplicationLiteralData::mutable_level_equals_4777f0() &
{
    require_owner_thread();
    return level_equals_4777f0_;
}
std::span<std::uint8_t, 5> SourceApplicationLiteralData::mutable_level_end_477990() &
{
    require_owner_thread();
    return level_end_477990_;
}
std::uint8_t SourceApplicationLiteralRef::read_level_script_byte(std::uint32_t address, std::uint32_t site) const
{
    if (!source_462890::needle_byte_site(site))
        throw SourceLiteralReadError(site, address, "not a reached462890 needle BYTE site");
    const auto identity = identity_.lock();
    if (!identity)
        throw SourceLiteralReadError(site, address, "expired level-script literal App owner");
    if (std::this_thread::get_id() != identity->thread)
        throw SourceLiteralReadError(site, address, "level-script literal App belongs to another thread");
    const auto &owner = *identity->owner;
    const auto settings_offset = static_cast<std::uint32_t>(address - 0x47788cu);
    if (settings_offset < owner.level_setting_literals_47788c_.size())
        return owner.level_setting_literals_47788c_[settings_offset];
    const auto rolling_offset = static_cast<std::uint32_t>(address - 0x477944u);
    if (rolling_offset < owner.rolling_literals_477944_.size())
        return owner.rolling_literals_477944_[rolling_offset];
    const auto sky_offset = static_cast<std::uint32_t>(address - 0x4777f4u);
    if (sky_offset < owner.level_sky_4777f4_.size())
        return owner.level_sky_4777f4_[sky_offset];
    const auto track_offset = static_cast<std::uint32_t>(address - 0x47780cu);
    if (track_offset < owner.level_track_47780c_.size())
        return owner.level_track_47780c_[track_offset];
    const auto equals_offset = static_cast<std::uint32_t>(address - 0x4777f0u);
    if (equals_offset < owner.level_equals_4777f0_.size())
        return owner.level_equals_4777f0_[equals_offset];
    const auto end_offset = static_cast<std::uint32_t>(address - 0x477990u);
    if (end_offset < owner.level_end_477990_.size())
        return owner.level_end_477990_[end_offset];
    throw SourceLiteralReadError(site, address, "reached unowned level-script needle BYTE");
}
} // namespace sgn::retail

namespace sgn::retail
{
std::span<std::uint8_t, 44> SourceApplicationLiteralData::mutable_version_inputs_482698() &
{
    require_owner_thread();
    return version_inputs_482698_;
}
std::span<std::uint8_t, 256> SourceApplicationLiteralData::mutable_version_bytes_612bf8() &
{
    require_owner_thread();
    return version_bytes_612bf8_;
}
std::span<std::uint8_t, 256> SourceApplicationLiteralRef::version_destination_44201f() const &
{
    constexpr std::uint32_t site = 0x44201f, address = 0x612bf8;
    const auto identity = identity_.lock();
    if (!identity)
        throw SourceLiteralReadError(site, address, "expired version destination App owner");
    if (std::this_thread::get_id() != identity->thread)
        throw SourceLiteralReadError(site, address, "version destination App belongs to another thread");
    return identity->owner->version_bytes_612bf8_;
}
} // namespace sgn::retail

namespace sgn::retail
{
SourceRollingDemoStorage &SourceApplicationLiteralData::mutable_rolling_demo_storage() &
{
    require_owner_thread();
    return rolling_demo_;
}
std::span<std::uint8_t, 40> SourceApplicationLiteralData::mutable_rolling_literals_477944() &
{
    require_owner_thread();
    return rolling_literals_477944_;
}
std::span<std::uint8_t, 6> SourceApplicationLiteralData::mutable_level_sky_4777f4() &
{
    require_owner_thread();
    return level_sky_4777f4_;
}
std::span<std::uint8_t, 8> SourceApplicationLiteralData::mutable_level_track_47780c() &
{
    require_owner_thread();
    return level_track_47780c_;
}
SourceRollingDemoStorage &SourceApplicationLiteralRef::rolling_demo_storage() const &
{
    constexpr std::uint32_t address = 0x6129a4;
    const auto identity = identity_.lock();
    if (!identity)
        throw SourceLiteralReadError(0, address, "expired rolling-demo App owner");
    if (std::this_thread::get_id() != identity->thread)
        throw SourceLiteralReadError(0, address, "rolling-demo App belongs to another thread");
    // NEW host borrow, not a synthetic source load/store site or successful init.
    return identity->owner->rolling_demo_;
}
} // namespace sgn::retail

namespace sgn::retail
{
std::span<std::uint8_t, 184> SourceApplicationLiteralData::mutable_level_setting_literals_47788c() &
{
    require_owner_thread();
    return level_setting_literals_47788c_;
}
std::uint32_t &SourceApplicationLiteralData::mutable_compare_locale_636830() &
{
    require_owner_thread();
    return compare_locale_636830_;
}
std::uint32_t SourceApplicationLiteralRef::read_compare_locale_46dda2() const
{
    const auto identity = identity_.lock();
    if (!identity)
        throw SourceLiteralReadError(0x46dda2, 0x636830, "expired comparison App owner");
    if (std::this_thread::get_id() != identity->thread)
        throw SourceLiteralReadError(0x46dda2, 0x636830, "comparison App belongs to another thread");
    return identity->owner->compare_locale_636830_;
}
std::uint8_t SourceApplicationLiteralRef::read_level_kind_byte_46dd90(std::uint32_t address, std::uint32_t site) const
{
    if (site != 0x46ddb0 && site != 0x46ddf0)
        throw SourceLiteralReadError(site, address, "not a46DD90 second-input BYTE read");
    const auto identity = identity_.lock();
    if (!identity)
        throw SourceLiteralReadError(site, address, "expired level-kind App owner");
    if (std::this_thread::get_id() != identity->thread)
        throw SourceLiteralReadError(site, address, "level-kind App belongs to another thread");
    // R17 rail: 418B3B pushes PE474674 as the same 46DD90 second input.
    const auto tube = static_cast<std::uint32_t>(address - 0x474674u);
    if (tube < identity->owner->tube_glass_474674_.size())
        return identity->owner->tube_glass_474674_[tube];
    const auto offset = static_cast<std::uint32_t>(address - 0x47788cu);
    const auto &bytes = identity->owner->level_setting_literals_47788c_;
    if (offset >= bytes.size())
        throw SourceLiteralReadError(site, address, "reached unowned level-kind literal byte");
    return bytes[offset];
}
} // namespace sgn::retail

namespace sgn::retail
{
std::uint32_t SourceApplicationLiteralRef::read_level_scale_46e2f0(std::uint32_t site) const
{
    constexpr std::uint32_t address = 0x46e2f0;
    if (site != 0x42ccefu && site != 0x42cecfu)
        throw SourceLiteralReadError(site, address, "not a42CC20 scale FMUL read");
    const auto identity = identity_.lock();
    if (!identity)
        throw SourceLiteralReadError(site, address, "expired level-scale App owner");
    if (std::this_thread::get_id() != identity->thread)
        throw SourceLiteralReadError(site, address, "level-scale App belongs to another thread");
    const auto &bytes = identity->owner->level_scale_46e2f0_;
    return static_cast<std::uint32_t>(bytes[0]) | (static_cast<std::uint32_t>(bytes[1]) << 8u) |
           (static_cast<std::uint32_t>(bytes[2]) << 16u) | (static_cast<std::uint32_t>(bytes[3]) << 24u);
}
} // namespace sgn::retail

namespace sgn::retail
{
std::uint32_t &SourceApplicationLiteralRef::demo_index_627fdc() const &
{
    const auto identity = identity_.lock();
    if (!identity)
        throw SourceLiteralReadError(0x45c16c, 0x627fdc, "expired demo App owner");
    if (std::this_thread::get_id() != identity->thread)
        throw SourceLiteralReadError(0x45c16c, 0x627fdc, "demo index belongs to another thread");
    return identity->owner->demo_index_627fdc_;
}
std::uint32_t SourceApplicationLiteralRef::demo_scan_word_43b0e6(std::uint32_t index) const
{
    const auto identity = identity_.lock();
    if (!identity)
        throw SourceLiteralReadError(0x43b0e6, 0x4793f4, "expired demo App owner");
    if (std::this_thread::get_id() != identity->thread || index >= 7)
        throw SourceLiteralReadError(0x43b10a, 0x4793f4, "invalid demo scan borrow");
    return identity->owner->demo_scan_4793f4_[index];
}
} // namespace sgn::retail

namespace sgn::retail
{
std::uint32_t SourceApplicationLiteralRef::read_score_factory_word(std::uint32_t address, std::uint32_t site) const
{
    const auto identity = identity_.lock();
    if (!identity || std::this_thread::get_id() != identity->thread)
        throw SourceLiteralReadError(site, address, "score descriptor App identity/thread");
    const auto &b = identity->owner->score_factory_472030_;
    const auto at = address - 0x472030u;
    if (at > b.size() || b.size() - at < 4)
        throw SourceLiteralReadError(site, address, "score descriptor outside source six-row bank");
    return std::uint32_t(b[at]) | (std::uint32_t(b[at + 1]) << 8) | (std::uint32_t(b[at + 2]) << 16) |
           (std::uint32_t(b[at + 3]) << 24);
}
std::span<const std::uint8_t> SourceApplicationLiteralRef::result_initial_text(std::uint32_t address,
                                                                               std::uint32_t site) const &
{
    const auto identity = identity_.lock();
    if (!identity || std::this_thread::get_id() != identity->thread)
        throw SourceLiteralReadError(site, address, "results text App identity/thread");
    const auto at = address - 0x4857b8u;
    const auto &bytes = identity->owner->race_result_initial_text_4857b8_;
    if (at >= bytes.size())
        throw SourceLiteralReadError(site, address, "results text outside initial source literals");
    return {bytes.data() + at, bytes.size() - at};
}
std::uint8_t SourceApplicationLiteralRef::read_result_initial_text_byte(std::uint32_t address, std::uint32_t site) const
{
    const auto identity = identity_.lock();
    if (!identity || std::this_thread::get_id() != identity->thread)
        throw SourceLiteralReadError(site, address, "results text App identity/thread");
    const auto at = address - 0x4857b8u;
    const auto &b = identity->owner->race_result_initial_text_4857b8_;
    if (at >= b.size())
        throw SourceLiteralReadError(site, address, "results text outside initial source literals");
    return b[at];
}
} // namespace sgn::retail

namespace sgn::retail
{
std::uint32_t SourceApplicationLiteralRef::read_initial_grid_offset_word(std::uint32_t address,
                                                                         std::uint32_t site) const
{
    const auto identity = identity_.lock();
    if (!identity || std::this_thread::get_id() != identity->thread)
        throw SourceLiteralReadError(site, address, "initial-grid App identity/thread");
    const auto load = [&](const auto &bytes, std::uint32_t base) -> std::uint32_t {
        const auto at = address - base;
        if (at > bytes.size() || bytes.size() - at < 4)
            throw SourceLiteralReadError(site, address, "initial-grid row outside retained PE bank");
        return std::uint32_t(bytes[at]) | (std::uint32_t(bytes[at + 1]) << 8) | (std::uint32_t(bytes[at + 2]) << 16) |
               (std::uint32_t(bytes[at + 3]) << 24);
    };
    if (address - 0x4743f0u < 96u)
        return load(identity->owner->initial_grid_4743f0_, 0x4743f0u);
    if (address - 0x4744f0u < 96u)
        return load(identity->owner->initial_grid_4744f0_, 0x4744f0u);
    throw SourceLiteralReadError(site, address, "unowned reached initial-grid offset word");
}
} // namespace sgn::retail
