#pragma once
#include "tabs_fwd.hpp"

#include "wrt/optional.hpp"
#include <string>

struct AssetNameHash {
  std::string name;
  std::string hash;
  std::string market() const { return name + "/WART"; }
  std::string liquidity_name() const { return name + "-LIQUIDITY"; }
  std::string to_string() const { return name + " (" + hash + ")"; }
  AssetNameHash(std::string name, std::string hash)
      : name(std::move(name)), hash(std::move(hash)) {}
  static AssetNameHash demo() {
    return {"DEMO", "0xDEADBEEFDEADBEEFDEADBEEFDEADBEEFDEADBEEF"};
  }
};

namespace ui {
struct SelectedAsset {
private:
  friend AssetTab;
  wrt::optional<AssetNameHash> nameHash;

public:
  const wrt::optional<AssetNameHash> &name_hash() const { return nameHash; };
};
} // namespace ui
