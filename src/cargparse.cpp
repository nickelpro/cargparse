module cargparse;

import std;
import :hash;

namespace cargparse {

// Nothing supports putting [[gnu::target]] in module interfaces yet, so these
// live in this implementation unit

#ifdef __GNUC__
[[gnu::target("bmi2")]]
std::uint32_t runtime_pext(std::uint32_t val, std::uint32_t m) {
  return __builtin_ia32_pext_si(val, m);
}

[[gnu::target("default")]]
#endif
std::uint32_t runtime_pext(std::uint32_t val, std::uint32_t m) {
  std::uint32_t res = 0;
  for(std::uint32_t bit = 0; m; ++bit) {
    std::uint32_t low = bit_basement(m);
    if(val & low)
      res |= std::uint32_t {1} << bit;
    m ^= low;
  }
  return res;
}

} // namespace cargparse
