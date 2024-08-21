#include "token.hpp"
#include "general/reader.hpp"
#include "general/view.hpp"
#include "general/writer.hpp"
TokenName::TokenName(View<5> data)
{
    for (size_t i = 0; i < data.size(); ++i)
        name[i] = data[i];
}

TokenName::TokenName(Reader& r)
    : TokenName(r.view<5>()) {};

Writer& operator<<(Writer& w, const TokenName& an)
{
    for (size_t i = 0; i < 5; ++i) {
        w << uint8_t(an.name[i]);
    }
    return w;
}
