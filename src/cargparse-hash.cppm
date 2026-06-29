export module cargparse:hash;

import std;

namespace cargparse {

// ---- primitives ----------------------------------------------------------

std::uint32_t runtime_pext(std::uint32_t val, std::uint32_t m);

constexpr std::uint32_t bit_basement(std::uint32_t x) {
  return x & -x;
}

constexpr std::uint32_t fnv1a32(std::string_view s) {
  std::uint32_t h = 0x811c9dc5u;
  for(unsigned char c : s)
    h = (h ^ c) * 0x01000193u;
  return h;
}

// Mirrored from cargparse.cpp for consteval contexts. The builtin folds better
// than the software version but is only available when actually targeting BMI2.
#if defined(__GNUC__) && __BMI2__
consteval std::uint32_t ce_pext(std::uint32_t val, std::uint32_t m) {
  return __builtin_ia32_pext_si(val, m);
}
#else
consteval std::uint32_t ce_pext(std::uint32_t val, std::uint32_t m) {
  std::uint32_t res = 0;
  for(std::uint32_t bit = 0; m; ++bit) {
    std::uint32_t low = bit_basement(m);
    if(val & low)
      res |= std::uint32_t {1} << bit;
    m ^= low;
  }
  return res;
}
#endif

// ---- switched storage ----------------------------------------------------

enum class index_width : std::uint8_t {
  u8,
  u16,
  u32,
  u64,
};

constexpr index_width width_for_max_value(std::size_t max_value) {
  if(max_value <= std::numeric_limits<std::uint8_t>::max())
    return index_width::u8;

  if(max_value <= std::numeric_limits<std::uint16_t>::max())
    return index_width::u16;

  if(max_value <= std::numeric_limits<std::uint32_t>::max())
    return index_width::u32;

  return index_width::u64;
}

template <class T>
inline constexpr index_width index_width_v = index_width::u64;

template <>
inline constexpr index_width index_width_v<std::uint8_t> = index_width::u8;

template <>
inline constexpr index_width index_width_v<std::uint16_t> = index_width::u16;

template <>
inline constexpr index_width index_width_v<std::uint32_t> = index_width::u32;

template <>
inline constexpr index_width index_width_v<std::uint64_t> = index_width::u64;

// ---- mask selection ------------------------------------------------------

struct mask_result {
  std::uint32_t mask {};
  bool ok {};
};

consteval mask_result greedy_set_cover_mask(
    std::span<const std::uint32_t> keys) {
  const std::size_t n = keys.size();

  if(n <= 1)
    return {.mask = 0, .ok = true};

  const std::size_t pair_count = n * (n - 1) / 2;
  const std::size_t word_count = (pair_count + 63) / 64;

  // coverage[bit * word_count + word] is a bitset of key-pairs
  // distinguished by that hash bit.
  std::vector<std::uint64_t> coverage(32 * word_count, 0);

  std::uint32_t candidate_bits = 0;
  std::size_t pair_index = 0;

  for(std::size_t i = 0; i < n; ++i) {
    for(std::size_t j = i + 1; j < n; ++j) {
      const std::uint32_t d = keys[i] ^ keys[j];

      // Two keys have the same discriminator value. No mask can separate them.
      if(d == 0)
        return {.mask = 0, .ok = false};

      candidate_bits |= d;

      const std::size_t word = pair_index >> 6;
      const std::size_t offset = pair_index & 63;
      const std::uint64_t pair_bit = std::uint64_t {1} << offset;

      for(std::uint32_t todo = d; todo;) {
        const std::uint32_t bit = bit_basement(todo);
        todo ^= bit;

        const unsigned bit_index = std::countr_zero(bit);
        coverage[bit_index * word_count + word] |= pair_bit;
      }

      ++pair_index;
    }
  }

  std::vector<std::uint64_t> uncovered(word_count, ~std::uint64_t {0});

  // Clear padding bits in the final word.
  if(const std::size_t used = pair_count & 63; used != 0)
    uncovered.back() = (std::uint64_t {1} << used) - 1;

  std::uint32_t selected = 0;

  while(std::ranges::any_of(uncovered,
      [](std::uint64_t word) { return word != 0; })) {
    const std::uint32_t remaining = candidate_bits & ~selected;

    std::uint32_t best_bit = 0;
    std::size_t best_score = 0;

    for(std::uint32_t todo = remaining; todo;) {
      const std::uint32_t bit = bit_basement(todo);
      todo ^= bit;

      const unsigned bit_index = std::countr_zero(bit);
      const std::size_t base = bit_index * word_count;

      std::size_t score = 0;

      for(std::size_t word = 0; word < word_count; ++word)
        score += std::popcount(coverage[base + word] & uncovered[word]);

      // Strict > gives deterministic tie-breaking in favor of the lower bit,
      // because todo enumerates bits from low to high.
      if(score > best_score) {
        best_score = score;
        best_bit = bit;
      }
    }

    // Should only happen if construction was impossible or the candidate bit
    // universe was inconsistent.
    if(best_bit == 0)
      return {.mask = 0, .ok = false};

    selected |= best_bit;

    const unsigned best_index = std::countr_zero(best_bit);
    const std::size_t base = best_index * word_count;

    for(std::size_t word = 0; word < word_count; ++word)
      uncovered[word] &= ~coverage[base + word];
  }

  return {.mask = selected, .ok = true};
}

// ---- PerfectHash ---------------------------------------------------------

struct PerfectHash {
  const std::size_t logical_slots = 0;
  const std::uint64_t* occupied = nullptr;
  const void* rank_words = nullptr;
  const void* values = nullptr;
  const std::uint32_t mask = 0;

  const index_width rank_width = index_width::u32;
  const index_width value_width = index_width::u32;

  [[nodiscard]]
  static constexpr std::size_t table_sentinel() {
    return std::numeric_limits<std::size_t>::max();
  }

  [[nodiscard]]
  constexpr std::size_t rank_at(std::size_t i) const {
    switch(rank_width) {
      case index_width::u8:
        return static_cast<const std::uint8_t*>(rank_words)[i];

      case index_width::u16:
        return static_cast<const std::uint16_t*>(rank_words)[i];

      case index_width::u32:
        return static_cast<const std::uint32_t*>(rank_words)[i];

      case index_width::u64:
        return static_cast<const std::uint64_t*>(rank_words)[i];
    }

    std::unreachable();
  }

  [[nodiscard]]
  constexpr std::size_t value_at(std::size_t i) const {
    switch(value_width) {
      case index_width::u8:
        return static_cast<const std::uint8_t*>(values)[i];

      case index_width::u16:
        return static_cast<const std::uint16_t*>(values)[i];

      case index_width::u32:
        return static_cast<const std::uint32_t*>(values)[i];

      case index_width::u64:
        return static_cast<const std::uint64_t*>(values)[i];
    }

    std::unreachable();
  }

  [[nodiscard]]
  constexpr std::size_t lookup(std::string_view name) const {
    if(!values)
      return table_sentinel();

    auto hash = fnv1a32(name);
    auto slot = static_cast<std::size_t>(runtime_pext(hash, mask));
    auto wi = slot >> 6;
    auto bi = slot & 63;

    std::size_t occupied_words = (logical_slots + 63) / 64;
    if(wi >= occupied_words)
      return table_sentinel();

    std::uint64_t bits = occupied[wi];
    std::uint64_t bit = std::uint64_t {1} << bi;

    if((bits & bit) == 0)
      return table_sentinel();

    auto dense_index = rank_at(wi) + std::popcount(bits & (bit - 1));
    return value_at(dense_index);
  }
};

// ---- builder internals ---------------------------------------------------

template <class T>
constexpr void checked_store(std::vector<T>& vec, std::size_t index,
    std::size_t value) {
  if(value > std::numeric_limits<T>::max())
    throw std::invalid_argument("perfect hash value outside storage width");

  vec[index] = static_cast<T>(value);
}

template <class Rank, class Value>
consteval PerfectHash make_perfect_hash_as(std::span<const char* const> names,
    std::span<const std::size_t> indices, std::size_t index_bound) {
  if(names.size() != indices.size())
    throw std::invalid_argument("name/index size mismatch");

  if(names.empty())
    return {};

  if(index_bound == 0)
    throw std::invalid_argument("perfect hash index bound is zero");

  std::size_t n = names.size();

  std::vector<std::uint32_t> keys(n);
  for(std::size_t i = 0; i < n; ++i)
    keys[i] = fnv1a32(names[i]);

  auto [mask, mask_ok] = greedy_set_cover_mask(keys);

  if(!mask_ok)
    throw std::invalid_argument("hash collision in perfect hash");

  auto pop = std::popcount(mask);

  if(pop >= std::numeric_limits<std::size_t>::digits)
    throw std::invalid_argument("perfect hash logical slot count too large");

  std::size_t logical_slots = std::size_t {1} << pop;

  std::size_t word_count = (logical_slots + 63) / 64;
  std::vector<std::uint64_t> occupied(word_count);
  std::vector<std::size_t> slots(n);

  for(std::size_t i = 0; i < n; ++i) {
    auto slot = static_cast<std::size_t>(ce_pext(keys[i], mask));

    auto wi = slot >> 6;
    auto bi = slot & 63;
    std::uint64_t bit = std::uint64_t {1} << bi;

    if(occupied[wi] & bit)
      throw std::invalid_argument("slot collision in perfect hash");

    occupied[wi] |= bit;
    slots[i] = slot;
  }

  std::vector<Rank> rank_words(word_count);

  std::size_t count = 0;
  for(std::size_t i = 0; i < word_count; ++i) {
    checked_store(rank_words, i, count);
    count += static_cast<std::size_t>(std::popcount(occupied[i]));
  }

  std::vector<Value> values(count);

  for(std::size_t i = 0; i < n; ++i) {
    if(indices[i] >= index_bound)
      throw std::invalid_argument("perfect hash index outside index bound");

    auto slot = slots[i];
    auto wi = slot >> 6;
    auto bi = slot & 63;
    std::uint64_t bit = std::uint64_t {1} << bi;

    std::uint64_t before_bits = occupied[wi] & (bit - 1);

    auto dense_index =
        static_cast<std::size_t>(rank_words[wi]) + std::popcount(before_bits);

    checked_store(values, dense_index, indices[i]);
  }

  auto occupied_span = std::define_static_array(occupied);
  auto rank_span = std::define_static_array(rank_words);
  auto values_span = std::define_static_array(values);

  return PerfectHash {
      .logical_slots = logical_slots,
      .occupied = occupied_span.data(),
      .rank_words = rank_span.data(),
      .values = values_span.data(),
      .mask = mask,
      .rank_width = index_width_v<Rank>,
      .value_width = index_width_v<Value>,
  };
}

template <class Value>
consteval PerfectHash make_perfect_hash_rank_dispatch(index_width rank_width,
    std::span<const char* const> names, std::span<const std::size_t> indices,
    std::size_t index_bound) {
  switch(rank_width) {
    case index_width::u8:
      return make_perfect_hash_as<std::uint8_t, Value>(names, indices,
          index_bound);

    case index_width::u16:
      return make_perfect_hash_as<std::uint16_t, Value>(names, indices,
          index_bound);

    case index_width::u32:
      return make_perfect_hash_as<std::uint32_t, Value>(names, indices,
          index_bound);

    case index_width::u64:
      return make_perfect_hash_as<std::uint64_t, Value>(names, indices,
          index_bound);
  }

  std::unreachable();
}

// ---- builder -------------------------------------------------------------

consteval PerfectHash make_perfect_hash(std::span<const char* const> names,
    std::span<const std::size_t> indices, std::size_t index_bound) {
  if(names.size() != indices.size())
    throw std::invalid_argument("name/index size mismatch");

  if(names.empty())
    return {};

  if(index_bound == 0)
    throw std::invalid_argument("perfect hash index bound is zero");

  // Stored values are known to be in [0, index_bound).
  auto value_width = width_for_max_value(index_bound - 1);

  // rank_words can conservatively need to represent [0, names.size()).
  auto rank_width = width_for_max_value(names.size() - 1);

  switch(value_width) {
    case index_width::u8:
      return make_perfect_hash_rank_dispatch<std::uint8_t>(rank_width, names,
          indices, index_bound);

    case index_width::u16:
      return make_perfect_hash_rank_dispatch<std::uint16_t>(rank_width, names,
          indices, index_bound);

    case index_width::u32:
      return make_perfect_hash_rank_dispatch<std::uint32_t>(rank_width, names,
          indices, index_bound);

    case index_width::u64:
      return make_perfect_hash_rank_dispatch<std::uint64_t>(rank_width, names,
          indices, index_bound);
  }

  std::unreachable();
}

} // namespace cargparse
