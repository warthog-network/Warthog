#include "defi/pool.hpp"
#include "defi/matching.hpp"
#include <iomanip>
#include<iostream>

using namespace std;
using namespace defi;

namespace test {
void print_from_fraction() {
  cout << (PriceRelative::from_fraction(2, 3) < *Price::from_double(0.6))
       << endl;
  cout << (PriceRelative::from_fraction(2, 3) < *Price::from_double(0.7))
       << endl;

  cout << (PriceRelative::from_fraction(3, 2) < *Price::from_double(1.4))
       << endl;
  cout << (PriceRelative::from_fraction(3, 2) < *Price::from_double(1.5))
       << endl;
  cout << (PriceRelative::from_fraction(3, 2) < *Price::from_double(1.6))
       << endl;
}
void multiply_floor() {
  auto p{Price::from_double(0.0991).value()};
  cout << std::setprecision(15) << p.to_double() << endl;
  auto pr = [](uint64_t a, Price p) {
    cout << a << "*" << p.to_double() << " = " << ::multiply_floor(a, p).value()
         << endl;
  };
  pr(100ull, p);
}
void from_fraction() {
  auto p{Price::from_double(0.0991).value()};
  cout << std::setprecision(15) << p.to_double() << endl;
  auto print = [](uint64_t a, uint64_t b) {
    auto pr{PriceRelative::from_fraction(a, b)};
    cout << a << "/" << b << " in [" << pr.price.to_double() << ", "
         << pr.ceil()->to_double() << "]";
  };
  auto a{12311231212313122ull};
  print(a, a + 1);
}
} // namespace test

void print_match(BuySellOrders &bso, Pool &p) {
  auto res{bso.match(p)};
  cout << "to pool: " << res.toPool.amount << " ("
       << (res.toPool.isQuote ? "quote" : "base") << ")\n";
  cout << "Price: (Pool before): " << p.price().price.to_double() << endl;
  if (res.toPool.isQuote) {
    p.buy(res.toPool.amount);
  } else {
    p.sell(res.toPool.amount);
  }
  cout << "Price (Pool after):  " << p.price().price.to_double() << endl;
  auto &nf{res.notFilled};
  if (nf) {
      cout << "Not filled: " << nf->amount << " (" << (nf->isQuote ? "quote" : "base")
          << ")" << endl;
  }
  auto matched{res.filled - res.toPool};
  if (matched.base!= 0 ) {
      cout << "Price (matched):     " << matched.price().price.to_double() << endl;
  }
  cout << "matched (base/quote): (" << matched.base << "/" << matched.quote
       << ")" << endl;
  cout << "filled (base/quote):  (" << res.filled.base << "/"
       << res.filled.quote << ")" << endl;
}
int main() {
  using namespace defi;
  Pool p(1000, 2000);
  cout << "Pool " << p.base_total() << " " << p.quote_total() << endl;
  cout << "Price: " << p.price().price.to_double() << endl;
  BuySellOrders bso;
  bso.insert_base(Order(100, Price::from_double(2).value()));
  bso.insert_base(Order(100, Price::from_double(1).value()));
  bso.insert_quote(Order(200, Price::from_double(10).value()));
  bso.insert_quote(Order(100, Price::from_double(2).value()));
  // bso.insert_quote(Order(301, Price::from_double(30.17).value()));
  print_match(bso, p);
}
