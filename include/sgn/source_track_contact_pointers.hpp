#pragma once
namespace sgn::retail
{
class RaceSessionPlatform;
// Actual401D40: scene2 returns before the track lookup; other scenes capture
// the current descriptor44 and copy part34/2C/30 to the retained contact owner.
// No logical part-count gate, pointed-array read, copy or allocation.
void initialize_source_track_contact_pointers_401d40(RaceSessionPlatform &);
} // namespace sgn::retail
