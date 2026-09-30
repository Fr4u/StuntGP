#include "sgn/source_application_literals.hpp"
namespace sgn::retail
{
SourceSaveCardStorage &SourceApplicationLiteralRef::save_card_storage() const &
{
    const auto identity = identity_.lock();
    if (!identity)
        throw SourceLiteralReadError(0x4402f0, 0x4798bc, "expired save-card application owner");
    if (std::this_thread::get_id() != identity->thread)
        throw SourceLiteralReadError(0x4402f0, 0x4798bc, "save-card data belongs to another thread");
    return identity->owner->save_card_data_;
}
} // namespace sgn::retail
