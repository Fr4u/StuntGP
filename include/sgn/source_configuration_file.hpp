#pragma once
#include "sgn/source_boot_config.hpp"

namespace sgn::retail
{
// Two recovered caller literals:482B30 scores,483408 gameplay.
enum class SourceConfigurationFile
{
    scores,
    gameplay
};
// Shared445CA0 through the existing44B3B0/44B260 cache. Caller owns clear,
// parsing, free and pending publication. Returns actual malloc storage with
// its requested-length NUL; incomplete initialized prefix is an explicit gap.
// NEW bounded input: positive length<=1MiB, complete local256 path.
SourceBootRawAllocation load_source_configuration_file_445ca0(SourceBootConfigState &, SourceBootFilePlatform &,
                                                              SourceConfigurationFile, std::uint32_t &requested_length);
} // namespace sgn::retail
