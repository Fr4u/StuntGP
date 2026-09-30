#include "sgn/source_application_literals.hpp"
namespace sgn::retail
{
SourceCrtErrorData &SourceApplicationLiteralRef::crt_error_storage() const &
{
    const auto identity = identity_.lock();
    if (!identity)
        throw SourceLiteralReadError(0x4699d7, 0x636530, "expired CRT data owner");
    if (std::this_thread::get_id() != identity->thread)
        throw SourceLiteralReadError(0x4699d7, 0x636530, "CRT data belongs to another thread");
    return identity->owner->crt_error_data_;
}
void SourceApplicationLiteralRef::map_os_error_4699d7(std::uint32_t error) const
{
    auto &data = crt_error_storage();
    source_crt_map_os_error_4699d7(error, data.doserrno_636534, data.errno_636530, data.table_495fe8);
}
void SourceApplicationLiteralRef::write_errno_463fdc() const
{
    crt_error_storage().errno_636530 = 34u;
}
} // namespace sgn::retail
