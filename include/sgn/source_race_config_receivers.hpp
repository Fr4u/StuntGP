#pragma once
namespace sgn::retail
{
class FrontendState;
class NativeSourceMenuCrt;
struct SourceBootFilePlatform;
// Actual existing load_selection/current/config/vehicle/CRT owners. No AssetStore,
// parameter snapshot, whole-object assignment, fresh world or CRT is admitted.
void load_source_vehicle_configuration_40bde0(FrontendState &, SourceBootFilePlatform &, NativeSourceMenuCrt &);
void reload_selected_source_vehicle_configurations_40bec0(FrontendState &, SourceBootFilePlatform &,
                                                          NativeSourceMenuCrt &);
// 413676..41368A only: publish slot0 current/template then execute real40BDE0.
// Caller must continue the remaining413670 template/clone/anchor chain afterward.
void initialize_source_vehicle_config_prefix_413676(FrontendState &, SourceBootFilePlatform &, NativeSourceMenuCrt &);
} // namespace sgn::retail
