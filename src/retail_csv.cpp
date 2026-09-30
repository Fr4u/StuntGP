#include "sgn/retail_csv.hpp"
#include "sgn/assets.hpp"
namespace sgn::retail
{
namespace
{
bool space(char c)
{
    return c == ' ' || c == '\t';
}
bool newline(char c)
{
    return c == '\r' || c == '\n';
}
} // namespace
std::string_view read_retail_csv_token(std::string_view text, std::size_t &at, std::size_t capacity)
{
    if (at > text.size())
        throw FormatError("CSV cursor outside input");
    while (at < text.size() && space(text[at]))
        ++at;
    if (at == text.size())
        return {};
    const bool quoted = text[at] == '"';
    if (quoted)
        ++at;
    const auto begin = at;
    if (quoted)
    {
        while (at < text.size() && text[at] != '"')
            ++at;
        if (at == text.size())
            return {};
    }
    else
        while (at < text.size() && text[at] != ',' && !newline(text[at]))
            ++at;
    const auto result = text.substr(begin, at - begin);
    if (result.size() >= capacity)
        throw FormatError("CSV token exceeds original temporary storage");
    if (quoted)
        ++at;
    if (at < text.size() && text[at] == ',')
        ++at;
    return result;
}
void skip_retail_csv_line(std::string_view text, std::size_t &at)
{
    if (at > text.size())
        throw FormatError("CSV cursor outside input");
    while (at < text.size() && text[at] != '\n')
        ++at;
    while (at < text.size() && (space(text[at]) || newline(text[at])))
        ++at;
}
} // namespace sgn::retail
