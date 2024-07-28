#pragma once
#include <optional>
#include <string>

class AssetName {
public:
    static std::optional<std::string> from_string(std::string s)
    {
        if (s.size() > 5)
            return {};
        for (auto c : s) {
            if (!(c >= 'A' && c <= 'Z') && !(c >= '0' && c <= '9'))
                return {};
        }
        return std::move(s);
    }
    auto& str() const { return name; }

private:
    std::string name;
};
