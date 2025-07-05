#include "token.hpp"
#include "general/reader.hpp"
#include "general/view.hpp"
AssetName::AssetName(View<5> data)
{
    for (size_t i = 0; i < data.size(); ++i)
        name[i] = data[i];
}

AssetName::AssetName(Reader& r)
    : AssetName(r.view<5>()) {};

