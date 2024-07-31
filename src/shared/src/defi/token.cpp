#include "token.hpp"
#include "general/reader.hpp"
#include "general/writer.hpp"
TokenName::TokenName(const uint8_t* data)
{
    std::string s(reinterpret_cast<const char*>(data), 5);
    for (auto iter = s.begin(); iter != s.end(); ++iter) {
        if (*iter == '\0') {
            s.resize(iter - s.begin());
            break;
        }
    }
}

Writer& operator<<(Writer& w, const TokenName& an)
{
    w << an.name;
    for (size_t i = 0; i < 5 - an.name.size(); ++i) {
        w << uint8_t(0);
    }
    return w;
}
