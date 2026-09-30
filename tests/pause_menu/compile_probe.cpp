// Compile all receiving interfaces together. This does not execute source or
// native gameplay and is not a qualification receiver or a completed test.
#include "bindings.hpp"
#include "input_wire.hpp"
#include "render_boundary.hpp"
#include "audio_boundary.hpp"
#include "native_fixture.hpp"
#include <type_traits>
static_assert(std::is_reference_v<decltype(sgn::retail::PauseMenuRestartInvocation::current_vehicle_output)>);
static_assert(!std::is_abstract_v<pause_menu_test::MemoryTextureDevice>);
static_assert(!std::is_abstract_v<pause_menu_test::RecordingDrawDevice>);
static_assert(!std::is_abstract_v<pause_menu_test::audio_boundary::RecordingDevice>);
