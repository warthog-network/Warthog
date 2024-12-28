#include "matching.hpp"
#include "nlohmann/json.hpp"
#include "order.hpp"
#include "price.hpp"
#include "src/pool.hpp"
#include <charconv>
#include <emscripten.h>
#include <iostream>
#include <stdio.h>
using namespace std;

// global variables
std::string returnString;
defi::Pool p{1000, 2000};
defi::BuySellOrders bso;

using json = nlohmann::json;
json pool_json(defi::Pool &pool) {
  return {{"base", pool.base_total()},
          {"quote", pool.quote_total()},
          {"price", double(pool.quote_total()) / double(pool.base_total())}};
}

json match_result() {
  auto pTmp{p};
  auto match_res{bso.match(pTmp)};
  json buys(json::array());
  for (size_t i = 0; i < bso.quote_desc_buy().size(); ++i) {
    bool matched{i < match_res.quoteBound};
    auto &elem{bso.quote_desc_buy()[i]};
    uint64_t filled{0};
    if (matched) {
      filled = elem.amount;
    }
    if (i == match_res.quoteBound && match_res.notFilled &&
        match_res.notFilled->isQuote) {
      filled = elem.amount - match_res.notFilled->amount;
    }
    buys.push_back({{"amount", elem.amount},
                    {"filled", filled},
                    {"limit", elem.limit.to_double()}});
  }
  json sells(json::array());
  auto J{bso.base_asc_sell().size()};
  cout << "match_res.baseBound" << match_res.baseBound << endl;
  if (match_res.notFilled) {
    cout << "match_res.notFilled->amount" << match_res.notFilled->amount
         << endl;
    ;
  }
  for (size_t j = 0; j < J; ++j) {
    bool matched{j < match_res.baseBound};
    auto &elem{bso.base_asc_sell()[j]};
    uint64_t filled{0};
    if (matched) {
      filled = elem.amount;
    }
    if (j == match_res.baseBound && match_res.notFilled &&
        !match_res.notFilled->isQuote) {
      filled = elem.amount - match_res.notFilled->amount;
    }
    sells.push_back({{"amount", elem.amount},
                     {"filled", filled},
                     {"limit", elem.limit.to_double()}});
  }
  std::reverse(sells.begin(), sells.end());

  auto &toPool{match_res.toPool};
  auto poolSwapped{[&]() {
    if (toPool.isQuote)
      return pTmp.buy(toPool.amount, 0);
    else
      return pTmp.sell(toPool.amount, 0);
  }()};
  auto matched{match_res.filled-match_res.toPool};

  return {{"buys", buys},
          {"sells", sells},
          {"poolBefore", pool_json(p)},
          {"toPool",{
           {"isQuote", toPool.isQuote},
           {"in", toPool.amount},
           {"out", poolSwapped}}},
          {"matched",{{"base",matched.base}, {"quote", matched.quote}}},
          {"poolAfter", pool_json(pTmp)}};
}

template <typename callable>
  requires std::is_invocable_r_v<json, callable, json>
const char *wrap_fun(const callable &fun, const char *c) {
  returnString = [&]() {
    try {
      return fun(json::parse(std::string_view(c))).dump();
    } catch (std::runtime_error &e) {
      return json{{"error", e.what()}}.dump();
    }
  }();
  return returnString.c_str();
}

defi::Order parse_order(json j) {
  auto price{[&]() {
    try {
      return Price::from_string(j["price"].get<std::string>()).value();
    } catch (...) {
      throw std::runtime_error("Cannot get price");
    }
  }()};
  auto amount{[&]() {
    try {
      std::string s{j["amount"].get<std::string>()};
      uint64_t res;
      auto r{std::from_chars(s.data(), s.data() + s.size(), res)};
      if (r.ec == std::errc() && r.ptr == s.data() + s.size())
        return res;
    } catch (...) {
    }
    throw std::runtime_error("Cannot get amount (must be a 64 bit integer)");
  }()};
  return {amount, price};
}

json add_buy(json j) {
  auto order{parse_order(j)};
  bso.insert_quote(order);
  return match_result();
}

json add_sell(json j) {
  auto order{parse_order(j)};
  bso.insert_base(order);
  return match_result();
}

extern "C" {
EMSCRIPTEN_KEEPALIVE
void sayHi(char *a) {
  printf("Hi!\n");
  cout << a << endl;
}

EMSCRIPTEN_KEEPALIVE
const char *addBuy(const char *json) { return wrap_fun(add_buy, json); }

EMSCRIPTEN_KEEPALIVE
const char *addSell(const char *json) { return wrap_fun(add_sell, json); }

EMSCRIPTEN_KEEPALIVE
int daysInWeek() { return 7; }
}
