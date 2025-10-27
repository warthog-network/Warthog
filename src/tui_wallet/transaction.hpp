#pragma once

#include <string>
#include <vector>

struct TransactionProperty {
  std::string name;
  std::string value;
};
struct TransactionProperties {
  std::string title;
  std::vector<TransactionProperty> entries;
};
