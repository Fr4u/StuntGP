#include "sgn/source_frontend_packed_word.hpp"
#include "sgn/retail_frontend_state.hpp"
#include "source_frontend_packed_word_store.hpp"
namespace sgn::retail
{
void set_frontend_packed_word_43dd10(FrontendSourceStorage &frontend, std::uint32_t ecx) noexcept
{
    detail::frontend_packed_word_store_43dd10(
        ecx, [&](std::uint32_t, std::uint32_t, std::uint32_t value) noexcept { frontend.packed_word_5ff4b8 = value; });
}
} // namespace sgn::retail
