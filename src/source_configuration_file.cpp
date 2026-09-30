#include "sgn/source_configuration_file.hpp"
#include <algorithm>
#include <bit>
#include <cstdlib>
#include <string>

namespace sgn::retail
{
using Word = std::uint32_t;
SourceBootRawAllocation load_source_configuration_file_445ca0(SourceBootConfigState &config,
                                                              SourceBootFilePlatform &files,
                                                              SourceConfigurationFile kind, Word &length)
{
    const bool score = kind == SourceConfigurationFile::scores;
    if (!score && kind != SourceConfigurationFile::gameplay)
        throw SourceBootConfigError(0x445cbcu, "445CA0 unrepresented configuration filename");
    const auto end = std::find(config.base_path.begin(), config.base_path.end(), '\0');
    if (end == config.base_path.end())
        throw SourceBootConfigError(0x445cbcu, "445CA0 configuration base path lacks its bounded terminator");
    // Pinned callers ECX482B30 scores /483408 gameplay; format483F78="%s\\%s".
    // The live48489C root spelling and repeated separator are retained.
    const std::string path =
        std::string(config.base_path.begin(), end) + (score ? "\\Config\\Scores.csv" : "\\Config\\Gameplay.csv");
    if (path.size() >= 256)
        throw SourceBootConfigError(0x445cbcu, "445CA0 configuration path exceeds original local256 destination");
    length = source_boot_file_size_44b3b0(config, files, path); // 445CCA
    if (std::bit_cast<std::int32_t>(length) <= 0)
        throw SourceBootConfigError(0x445b95u, "configuration parser would read the null445CA0 result");
    if (length > 1024u * 1024u)
        throw SourceBootConfigError(0x445ce1u, "445CA0 configuration allocation exceeds bounded CSV domain");
    SourceBootRawAllocation bytes(static_cast<std::uint8_t *>(std::malloc(static_cast<std::size_t>(length) + 1u)));
    if (!bytes)
        throw SourceBootConfigError(0x445cf4u, "445CA0 configuration read would receive null malloc storage");
    std::size_t initialized = 0;
    (void)source_boot_file_read_44b260(config, files, path, length, {bytes.get(), length}, initialized); // 445CF4
    bytes.get()[length] = 0; // 445CF9, requested size regardless of source read return
    if (initialized < length)
        throw SourceBootConfigError(score ? 0x443c21u : 0x445671u,
                                    "445B80 configuration input would include unwritten malloc bytes");
    return bytes;
}

} // namespace sgn::retail
