#include "sgn/source_application_literals.hpp"

namespace sgn::retail
{
SourceWindowStartupLiterals SourceApplicationLiteralRef::window_startup_literals() const &
{
    const auto identity = identity_.lock();
    if (!identity)
        throw SourceLiteralReadError(0x44e8e4, 0x486474, "expired application window literals");
    if (std::this_thread::get_id() != identity->thread)
        throw SourceLiteralReadError(0x44e8e4, 0x486474, "window literals belong to another thread");
    const auto *base = identity->owner->window_literals_486474_.data();
    // 44E8E4/44E8EB publish class registration pointers;
    // 44E907/44E90C supply title/class to422700. No copy or string scan.
    return {base, base + 8, base + 20, base + 32};
}
} // namespace sgn::retail
