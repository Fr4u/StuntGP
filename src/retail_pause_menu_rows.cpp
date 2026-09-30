#include "sgn/retail_pause_menu.hpp"

namespace sgn::retail
{
std::array<PauseMenuRow, 8> initial_pause_menu_rows()
{
    return {{{1, 8, 12, 51, 0x446b40, 0},
             {1, 9, 12, 71, 0x446af0, 0},
             {1, 10, 12, 91, 0x446b20, 0},
             {1, 12, 12, 131, 0x446a90, 0},
             {1, 13, 12, 151, 0x446ac0, 0},
             {1, 14, 12, 171, 0x446a70, 0},
             {1, 15, 12, 191, 0x446bb0, 54},
             {0, 0, 0, 0, 0, 0}}};
}
} // namespace sgn::retail
