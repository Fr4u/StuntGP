#include "sgn/source_frontend_csv.hpp"
#include "sgn/source_frontend_csv_float.hpp"
#include "sgn/retail_frontend_state.hpp"
#include "sgn/retail_persistence.hpp"
#include "sgn/race_session.hpp"
#include "sgn/source_script_path_formatter.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <stdexcept>
namespace sgn::retail
{
namespace
{
using W = std::uint32_t;
bool horizontal(std::uint8_t c)
{
    return c == 0x20 || c == 0x09;
}
bool newline(std::uint8_t c)
{
    return c == 0x0d || c == 0x0a;
}
std::uint8_t read(const SourceFrontendCsv &c, W at)
{
    if (at >= c.input.size())
        throw SourceFrontendResourceError(0x43c680, "CSV read outside initialized input");
    return c.input[at];
}
void store(std::span<std::uint8_t> out, W at, std::uint8_t value)
{
    if (at >= out.size())
        throw SourceFrontendResourceError(0x43c680, "CSV write outside actual destination owner");
    out[at] = value;
}
} // namespace
SourceFrontendCsv::SourceFrontendCsv(FrontendState &f, std::span<const std::uint8_t> bytes, W &position)
    : frontend(f), input(bytes), cursor(position)
{
    f.shared_platform()->require_owner_thread();
}
W SourceFrontendCsv::skip_43c520()
{
    W seen_newline = 0;
    for (;;)
    {
        const auto c = read(*this, cursor);
        if (!horizontal(c) && !newline(c))
            return seen_newline;
        if (newline(c))
            seen_newline = 1;
        ++cursor;
    }
}
void SourceFrontendCsv::next_line_43c5c0()
{
    while (read(*this, cursor) != 0 && !newline(read(*this, cursor)))
        ++cursor;
    (void)skip_43c520();
}
W SourceFrontendCsv::token_43c680(std::span<std::uint8_t> output)
{
    store(output, 0, 0);
    while (horizontal(read(*this, cursor)))
        ++cursor;
    if (read(*this, cursor) == 0)
        return 0;
    W length = 0;
    if (read(*this, cursor) == 0x22)
    {
        ++cursor;
        while (read(*this, cursor) != 0x22 && read(*this, cursor) != 0)
        {
            store(output, length, read(*this, cursor));
            ++length;
            ++cursor;
        }
        if (read(*this, cursor) != 0x22)
            return 0; // actual early return: no trailing NUL repair
        ++cursor;
    }
    else
    {
        while (read(*this, cursor) != 0x2c && !newline(read(*this, cursor)) && read(*this, cursor) != 0)
        {
            store(output, length, read(*this, cursor));
            ++length;
            ++cursor;
        }
    }
    store(output, length, 0);
    while (length != 0 && output[length - 1] == 0x20)
    {
        --length;
        store(output, length, 0);
    }
    if (read(*this, cursor) == 0x2c)
        ++cursor;
    return length;
}
bool SourceFrontendCsv::label_43c600(std::string_view expected)
{
    std::array<std::uint8_t, 64> local;
    const auto length = token_43c680(local);
    if (length == 0)
        return false;
    return std::string_view(reinterpret_cast<const char *>(local.data()), length) == expected;
}
W SourceFrontendCsv::number_43c770(W &destination)
{
    const auto length = token_43c680(frontend.source.csv_token_scratch);
    std::int32_t value;
    if (!scan_persistence_decimal(frontend.source.csv_token_scratch, value))
        return 0;
    destination = std::bit_cast<W>(value);
    return length;
}
std::span<const std::uint8_t> SourceFrontendCsv::literal(W address) const
{
    const auto app = frontend.shared_platform()->source_application_literals();
    return app.frontend_csv_literal(address);
}
W SourceFrontendCsv::format_key(W address, std::span<const W> args, std::span<std::uint8_t> output, W site)
{
    // Each actual call has a fixed format and signature. No pointer/value heuristic.
    W expected_format = 0, count = 0, string_slot = 0;
    switch (site)
    {
    case 0x434e3c:
        expected_format = 0x478ca0;
        count = 1;
        break;
    case 0x434ee3:
        expected_format = 0x478cc4;
        count = 2;
        string_slot = 2;
        break;
    case 0x434f90:
        expected_format = 0x478cd0;
        count = 1;
        break;
    case 0x43577a:
        expected_format = 0x479140;
        count = 1;
        break;
    case 0x4357de:
        expected_format = 0x47914c;
        count = 2;
        break;
    case 0x4358a0:
        expected_format = 0x479170;
        count = 1;
        break;
    case 0x4358e5:
        expected_format = 0x479178;
        count = 2;
        string_slot = 1;
        break;
    case 0x4359ac:
        expected_format = 0x479184;
        count = 1;
        break;
    case 0x435a51:
        expected_format = 0x4791a0;
        count = 1;
        break;
    case 0x435ad0:
        expected_format = 0x4791a8;
        count = 2;
        break;
    case 0x435e5a:
        expected_format = 0x479248;
        count = 1;
        break;
    case 0x435fc9:
        expected_format = 0x4792a8;
        count = 2;
        string_slot = 1;
        break;
    default:
        throw SourceFrontendResourceError(site, "unbound CSV key formatter return site");
    }
    if (address != expected_format || args.size() != count || output.size() != 64)
        throw SourceFrontendResourceError(site, "CSV key formatter caller contract mismatch");
    const auto platform = frontend.shared_platform();
    const auto app = platform->source_application_literals();
    SourceSaveStringBorrow text;
    if (string_slot != 0)
        text = {string_slot * 4, literal(args[string_slot - 1])};
    SourceScriptPathCall call;
    return format_save_ui_46280c(platform->state().boot_config, app, literal(address), args, output, site, call, text);
}
} // namespace sgn::retail

namespace sgn::retail
{
std::uint32_t SourceFrontendCsv::floating_43c7b0(std::uint32_t &destination)
{
    const auto length = token_43c680(frontend.source.csv_float_scratch);
    const auto app = frontend.shared_platform()->source_application_literals();
    return scan_frontend_float_46348e(app, frontend.source.csv_float_scratch, destination) ? length : 0;
}
} // namespace sgn::retail
