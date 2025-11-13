#include "asset.hpp"
#include "general/reader.hpp"
#include "general/view.hpp"
AssetName::AssetName(View<5> data)
    : AssetName([&data]() {
        size_t end { data.size() };
        for (size_t i = 0; i < data.size(); ++i) {
            auto c { data[i] };
            if (end < data.size() && c != 0) // when we found the zero byte, only zero-bytes can follow
                throw Error(EASSETNAME);
            if (c == 0)
                end = i;
        }
        return std::string((char*)data.data(), end);
    }())
{
}

AssetName::AssetName(Reader& r)
    : AssetName(r.view<5>()) { };
