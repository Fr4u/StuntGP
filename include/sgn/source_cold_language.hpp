#pragma once
#include <cstdint>

namespace sgn
{
class AssetStore;
namespace retail
{
struct GameTextState;
class RaceSessionPlatform;
struct SourceBootFilePlatform;

// 445F70 writes the existing4835E0 owner verbatim. No language normalization.
void set_source_language_445f70(std::uint32_t &requested_language, std::uint32_t language) noexcept;

// 445F80 loads4835E0 once, compares612944 and tail-calls the existing445D70
// loader only on inequality. The loader owns every text/current-language write.
// Equality bypasses AssetStore and all input-domain checks, even for0/FFFFFFFF.
// Errors retain the loader's reached writes; there is no rollback or retry here.
void refresh_source_language_445f80(const AssetStore &, const std::uint32_t &requested_language, GameTextState &);

// Explicit adapters on the persistent platform world. They can be invoked
// before a level Session exists; they neither construct nor bootstrap a Session.
// Both require the actual owner thread before reading or writing source state.
void set_session_source_language_445f70(RaceSessionPlatform &, std::uint32_t language);
void refresh_session_source_language_445f80(RaceSessionPlatform &);
// Source file route on the SAME platform boot_config/game_text owners.
// Equality performs no file/heap operation; no implicit backend preparation.
void refresh_session_source_language_445f80(RaceSessionPlatform &, SourceBootFilePlatform &);
} // namespace retail
} // namespace sgn
