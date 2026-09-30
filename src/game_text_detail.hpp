#pragma once
#include <cstdint>
#include <string_view>
// Implementation-only functions shared by the existing buffer/AssetStore
// entry and the source file entry. No new parser or persistent owner.
namespace sgn::retail {
struct GameTextState;
namespace game_text_detail {
void begin_game_text_load(GameTextState&);
void validate_game_text_input(std::string_view,std::uint32_t);
void load_prepared_game_text(std::string_view,std::uint32_t,GameTextState&);
}
}
