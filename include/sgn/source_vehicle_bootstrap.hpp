#pragma once
#include <cstdint>
namespace sgn::retail
{
class FrontendState;
struct RaceSessionState;
struct SourceBootFilePlatform;
class NativeSourceMenuCrt;
// Actual413670 on the existing Platform/world/files/CRT and source PMD bank.
// Prerequisites: original cold vehicle and resource/controller producers.
// Includes40BDE0, all six-record construction, bank strip, anchors and4130D0.
// Source726C non-null remains an explicit external-object identity boundary.
void initialize_source_vehicles_413670(FrontendState &, SourceBootFilePlatform &, NativeSourceMenuCrt &);
// The REP MOVSD at413744 always reads physical slot0, writes slot1..5.
// Kept separate only to make its complete source-field map reviewable.
void clone_source_template_vehicle_413744(RaceSessionState &, unsigned target);
} // namespace sgn::retail
