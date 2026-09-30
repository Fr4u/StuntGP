#pragma once
#include <memory>
namespace sgn::retail
{
class RaceSessionPlatform;
class NativeSourceMenuCrt;
struct SourceBootFilePlatform;
void load_source_camera_resources_401f00(const std::weak_ptr<RaceSessionPlatform> &, SourceBootFilePlatform &,
                                         NativeSourceMenuCrt &);
void initialize_source_camera_402420(const std::weak_ptr<RaceSessionPlatform> &, SourceBootFilePlatform &,
                                     NativeSourceMenuCrt &);
} // namespace sgn::retail
