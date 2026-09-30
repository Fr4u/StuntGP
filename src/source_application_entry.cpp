#include "sgn/source_application_entry.hpp"

namespace sgn::retail
{

SourceApplicationEntryResult run_source_application_44ea60(SourceApplicationEntryWorld &world,
                                                           const SourceApplicationEntryArguments &arguments,
                                                           SourceApplicationEntryServices &services,
                                                           SourceApplicationMessages &messages)
{
    // 44EA68..44EA86: these are source startup words, not a modern monitor mode.
    world.boot.display_width = 640;
    world.boot.display_height = 480;
    world.boot.display_depth = 8;
    services.show_cursor(false);
    services.invoke(SourceApplicationCall::initialize_42d660);
    services.invoke(SourceApplicationCall::initialize_442010);
    if (services.initialize_window_44e890(arguments) == 0)
    {
        // 44EAAD: direct return. The successful-start cleanup is not reached.
        return {std::uint32_t{0}};
    }
    world.scene = 2;
    world.target_scene = 2;
    services.invoke(SourceApplicationCall::initialize_scene_442120);
    services.invoke(SourceApplicationCall::initialize_window_clock_44e5d0);

    std::optional<std::uint32_t> last_wparam;
    do
    {
        if (messages.peek_removed())
        {
            last_wparam = messages.source_wparam();
            if (messages.message_id() == 0x12)
            { // WM_QUIT, 44EAF0
                break;
            }
            messages.translate();
            messages.dispatch();
        }
        else
        {
            services.invoke(SourceApplicationCall::idle_44e9d0);
            // Read after the idle child, and preserve the short-circuit order.
            if (world.window_word_61c374 != 0 && world.window_word_61c364 == 0)
            {
                const auto scene_at_branch = world.scene;
                if (scene_at_branch == 1)
                {
                    services.invoke(SourceApplicationCall::update_race_442810);
                    // The update may change scene. The original still draws.
                    services.invoke(SourceApplicationCall::draw_race_4427b0);
                }
                else if (scene_at_branch == 2)
                {
                    services.invoke(SourceApplicationCall::update_menu_441f00);
                    services.invoke(SourceApplicationCall::draw_menu_441d70);
                }
                // 44EB40 also executes for scene values other than 1 and 2.
                services.invoke(SourceApplicationCall::finish_frame_44e6b0);
            }
        }
        // 44EB45 is exactly ==1; a different nonzero DWORD does not exit.
    } while (!world.outer.source_loop_exit_requested_44eb45());

    services.invoke(SourceApplicationCall::sample_end_time_44e6c0);
    services.invoke(SourceApplicationCall::shutdown_442180);
    // 44EA50, called at 44EB58, is a complete single RET with no side effects.
    return {last_wparam};
}

} // namespace sgn::retail
