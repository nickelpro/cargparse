export module cargparse;

import std;
export import :hash;

// ================================================================
// Exported: core types
// ================================================================

export namespace cargparse {

enum class NArgsPattern { optional, any, at_least_one };

} // namespace cargparse

namespace cargparse {

enum class ArgumentType { positional, optional };

struct NArgsRange {
  std::size_t min_ = 1;
  std::size_t max_ = 1;

  constexpr NArgsRange() = default;
  constexpr NArgsRange(std::size_t mn, std::size_t mx) : min_(mn), max_(mx) {
    if(mn > mx)
      throw std::invalid_argument(
          std::format("nargs min ({}) exceeds max ({})", mn, mx));
  }

  [[nodiscard]]
  constexpr bool contains(std::size_t n) const {
    return n >= min_ && n <= max_;
  }
  [[nodiscard]]
  constexpr bool is_exact() const {
    return min_ == max_;
  }
  [[nodiscard]]
  constexpr bool is_right_bounded() const {
    return max_ != std::numeric_limits<std::size_t>::max();
  }

  [[nodiscard]]
  constexpr std::string to_string() const {
    if(min_ == 0 && max_ == 1)
      return "?";
    if(!is_right_bounded()) {
      if(min_ == 0)
        return "*";
      if(min_ == 1)
        return "+";
      return std::format("{} or more", min_);
    }
    if(is_exact()) {
      if(min_ == 1)
        return {};
      return std::format("{}", min_);
    }
    return std::format("{}..{}", min_, max_);
  }
  constexpr auto operator<=>(const NArgsRange&) const = default;
};

inline constexpr NArgsRange nargs_pattern_to_range(NArgsPattern p) {
  using limits = std::numeric_limits<std::size_t>;
  switch(p) {
    case NArgsPattern::optional:
      return {0, 1};
    case NArgsPattern::any:
      return {0, limits::max()};
    case NArgsPattern::at_least_one:
      return {1, limits::max()};
  }
  return {1, 1};
}

struct RuntimeArg {
  const char* primary_name = nullptr;
  const char* const* aliases = nullptr;
  std::size_t alias_count = 0;
  NArgsRange nargs {1, 1};
  const char* default_value = nullptr;
  const char* implicit_value = nullptr;
  const char* const* choices = nullptr;
  std::size_t choice_count = 0;
  bool required = false;
  bool repeatable = false;
  bool is_optional = false;

  [[nodiscard]]
  constexpr bool has_choices() const {
    return choices != nullptr;
  }
  [[nodiscard]]
  constexpr bool choice_contains(std::string_view v) const {
    for(std::size_t i = 0; i < choice_count; ++i)
      if(choices[i] == v)
        return true;
    return false;
  }
  [[nodiscard]]
  constexpr bool is_flag() const {
    return nargs.max_ == 0 && implicit_value != nullptr;
  }
  [[nodiscard]]
  constexpr bool check(std::string_view name) const {
    if(primary_name == name)
      return true;
    for(std::size_t i = 0; i < alias_count; ++i)
      if(aliases[i] == name)
        return true;
    return false;
  }
  [[nodiscard]]
  constexpr std::string names_csv(char sep = ',') const {
    std::string r(primary_name);
    for(std::size_t i = 0; i < alias_count; ++i)
      r += std::string(1, sep) + " " + aliases[i];
    return r;
  }
};

// Forward declaration: specializations are in the non-exported block.
template <typename T>
constexpr T convert_from_sv(std::string_view sv);

// ================================================================
// ArgSpec — transient builder
// ================================================================

struct ArgSpec {
  std::vector<std::string> names;
  std::string help_, metavar_;
  std::string default_value_, implicit_value_;
  std::vector<std::string> choices_;
  NArgsRange nargs_ {1, 1};
  ArgumentType type_ = ArgumentType::positional;
  bool required_ = false, repeatable_ = false, hidden_ = false;

  constexpr ArgSpec& help(std::string h) {
    help_ = std::move(h);
    return *this;
  }
  constexpr ArgSpec& metavar(std::string m) {
    metavar_ = std::move(m);
    return *this;
  }
  constexpr ArgSpec& default_value(std::string v) {
    default_value_ = std::move(v);
    nargs_.min_ = 0;
    return *this;
  }
  constexpr ArgSpec& implicit_value(std::string v) {
    implicit_value_ = std::move(v);
    nargs_ = {0, 0};
    return *this;
  }
  constexpr ArgSpec& flag() {
    default_value_ = "false";
    implicit_value_ = "true";
    nargs_ = {0, 0};
    return *this;
  }
  constexpr ArgSpec& required(bool r = true) {
    required_ = r;
    return *this;
  }
  constexpr ArgSpec& nargs(std::size_t n) {
    nargs_ = {n, n};
    return *this;
  }
  constexpr ArgSpec& nargs(std::size_t mn, std::size_t mx) {
    nargs_ = {mn, mx};
    return *this;
  }
  constexpr ArgSpec& nargs(NArgsPattern p) {
    nargs_ = nargs_pattern_to_range(p);
    return *this;
  }
  constexpr ArgSpec& append() {
    repeatable_ = true;
    return *this;
  }
  constexpr ArgSpec& hidden(bool h = true) {
    hidden_ = h;
    return *this;
  }
  constexpr ArgSpec& choices(std::vector<std::string> c) {
    choices_ = std::move(c);
    return *this;
  }
};

} // namespace cargparse

// ================================================================
// Exported: ArgumentParser forward decl, ParseResult
// ================================================================

export namespace cargparse {

class ArgumentParser;

class ParseResult {
public:
  template <typename T = std::string>
  [[nodiscard]]
  T get(std::string_view name) const;

  template <typename T = std::string>
  [[nodiscard]]
  std::optional<T> present(std::string_view name) const;

  [[nodiscard]]
  bool is_used(std::string_view name) const;
  [[nodiscard]]
  bool help_requested() const {
    return help_requested_;
  }
  [[nodiscard]]
  bool version_requested() const {
    return version_requested_;
  }
  [[nodiscard]]
  explicit operator bool() const {
    return !entries_.empty();
  }
  [[nodiscard]]
  const std::vector<std::string_view>& get_values(std::string_view name) const;

private:
  friend class ArgumentParser;

  ParseResult(PerfectHash hash, std::span<const RuntimeArg> args)
      : hash_ {hash}, args_ {args} {}

  [[nodiscard]]
  constexpr const RuntimeArg* find_arg(std::string_view name) const {
    auto idx = hash_.lookup(name);
    if(idx < args_.size() && args_[idx].check(name))
      return &args_[idx];
    return nullptr;
  }

  struct string_hash {
    using is_transparent = void;
    [[nodiscard]]
    std::size_t operator()(const char* key) const {
      return std::hash<std::string_view> {}(key);
    }
    [[nodiscard]]
    std::size_t operator()(std::string_view key) const {
      return std::hash<std::string_view> {}(key);
    }
    [[nodiscard]]
    std::size_t operator()(const std::string& key) const {
      return std::hash<std::string> {}(key);
    }
  };

  std::unordered_map<std::string_view, std::vector<std::string_view>,
      string_hash, std::equal_to<>>
      entries_;
  PerfectHash hash_;
  std::span<const RuntimeArg> args_;
  bool help_requested_ = false;
  bool version_requested_ = false;

  [[nodiscard]]
  constexpr std::string_view resolve_name(std::string_view name) const {
    auto idx = hash_.lookup(name);
    if(idx < args_.size() && args_[idx].check(name))
      return args_[idx].primary_name;
    return name;
  }

  void set_values(std::string_view name, std::vector<std::string_view> vals) {
    entries_[name] = std::move(vals);
  }
  void add_values(std::string_view name,
      const std::vector<std::string_view>& vals) {
    auto& v = entries_[name];
    for(auto& s : vals)
      v.push_back(s);
  }
};

} // namespace cargparse

// ================================================================
// Internal helpers
// ================================================================

namespace cargparse {

constexpr bool is_numeric_literal(std::string_view s) {
  if(s.empty())
    return false;
  std::size_t i = 0;
  if(s[0] == '-') {
    if(s.size() == 1)
      return false;
    i = 1;
  }
  bool has_digit = false, has_dot = false;
  for(; i < s.size(); ++i) {
    char c = s[i];
    if(c >= '0' && c <= '9') {
      has_digit = true;
      continue;
    }
    if(c == '.' && !has_dot) {
      has_dot = true;
      continue;
    }
    if((c == 'e' || c == 'E') && has_digit && i + 1 < s.size()) {
      ++i;
      if(s[i] == '+' || s[i] == '-')
        ++i;
      for(; i < s.size(); ++i)
        if(s[i] < '0' || s[i] > '9')
          return false;
      return true;
    }
    return false;
  }
  return has_digit;
}

constexpr bool is_valid_optional_name(std::string_view name) {
  if(name.empty())
    return false;
  if(name[0] != '-')
    return false;
  if(name.size() >= 2 && name[1] == '-')
    return true;
  std::string_view rest = name.substr(1);
  if(rest.empty())
    return false; // lone '-' is positional (stdin/stdout convention)
  return !is_numeric_literal(rest);
}

constexpr std::size_t levenshtein_distance(std::string_view s1,
    std::string_view s2) {
  std::size_t m = s1.size(), n = s2.size();
  if(m == 0)
    return n;
  if(n == 0)
    return m;
  std::vector<std::size_t> prev(n + 1), curr(n + 1);
  for(std::size_t j = 0; j <= n; ++j)
    prev[j] = j;
  for(std::size_t i = 1; i <= m; ++i) {
    curr[0] = i;
    for(std::size_t j = 1; j <= n; ++j) {
      std::size_t cost = (s1[i - 1] == s2[j - 1]) ? 0 : 1;
      curr[j] = std::min({prev[j] + 1, curr[j - 1] + 1, prev[j - 1] + cost});
    }
    prev.swap(curr);
  }
  return prev[n];
}

template <std::ranges::input_range R>
requires std::convertible_to<std::ranges::range_value_t<R>, std::string_view>
constexpr std::string_view most_similar_string(std::string_view target,
    const R& candidates) {
  if(std::ranges::empty(candidates))
    return {};
  std::string_view best = *std::ranges::begin(candidates);
  std::size_t best_dist = levenshtein_distance(target, best);
  for(auto& c : candidates) {
    std::string_view sv = c;
    std::size_t d = levenshtein_distance(target, sv);
    if(d < best_dist) {
      best_dist = d;
      best = sv;
    }
  }
  return best;
}

constexpr std::string join(const auto& range, std::string_view sep) {
  std::string result;
  bool first = true;
  for(auto& elem : range) {
    if(!first)
      result += sep;
    first = false;
    result += elem;
  }
  return result;
}

constexpr void check_full_parse(std::errc ec, const char* ptr, const char* end,
    std::string_view sv) {
  if(ec != std::errc {} || ptr != end)
    throw std::invalid_argument(std::format("failed to convert '{}'", sv));
}

template <typename T>
constexpr T convert_from_sv(std::string_view sv) {
  static_assert(sizeof(T) == 0, "Unsupported type for conversion");
  return {};
}
template <>
constexpr int convert_from_sv<int>(std::string_view sv) {
  int val = 0;
  auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), val);
  check_full_parse(ec, ptr, sv.data() + sv.size(), sv);
  return val;
}
template <>
constexpr long convert_from_sv<long>(std::string_view sv) {
  long val = 0;
  auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), val);
  check_full_parse(ec, ptr, sv.data() + sv.size(), sv);
  return val;
}
template <>
constexpr long long convert_from_sv<long long>(std::string_view sv) {
  long long val = 0;
  auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), val);
  check_full_parse(ec, ptr, sv.data() + sv.size(), sv);
  return val;
}
template <>
constexpr unsigned int convert_from_sv<unsigned int>(std::string_view sv) {
  unsigned int val = 0;
  auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), val);
  check_full_parse(ec, ptr, sv.data() + sv.size(), sv);
  return val;
}
template <>
constexpr unsigned long convert_from_sv<unsigned long>(std::string_view sv) {
  unsigned long val = 0;
  auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), val);
  check_full_parse(ec, ptr, sv.data() + sv.size(), sv);
  return val;
}
template <>
constexpr unsigned long long convert_from_sv<unsigned long long>(
    std::string_view sv) {
  unsigned long long val = 0;
  auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), val);
  check_full_parse(ec, ptr, sv.data() + sv.size(), sv);
  return val;
}
template <>
constexpr float convert_from_sv<float>(std::string_view sv) {
  float val = 0;
  auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), val,
      std::chars_format::general);
  check_full_parse(ec, ptr, sv.data() + sv.size(), sv);
  return val;
}
template <>
constexpr double convert_from_sv<double>(std::string_view sv) {
  double val = 0;
  auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), val,
      std::chars_format::general);
  check_full_parse(ec, ptr, sv.data() + sv.size(), sv);
  return val;
}
template <>
constexpr bool convert_from_sv<bool>(std::string_view sv) {
  if(sv == "true" || sv == "1" || sv == "TRUE" || sv == "yes" || sv == "True" ||
      sv == "YES")
    return true;
  if(sv == "false" || sv == "0" || sv == "FALSE" || sv == "no" ||
      sv == "False" || sv == "NO")
    return false;
  throw std::invalid_argument(std::format("cannot convert '{}' to bool", sv));
}
template <>
constexpr std::string convert_from_sv<std::string>(std::string_view sv) {
  return std::string(sv);
}

// Bake a single ArgSpec into a RuntimeArg
consteval RuntimeArg bake_arg(const ArgSpec& src) {
  RuntimeArg dst;
  if(!src.names.empty()) {
    // Must bake via define_static_string — define_static_array
    // rejects structs with string-literal const char* members.
    dst.primary_name = std::define_static_string(src.names[0]);

    if(src.names.size() > 1) {
      std::vector<const char*> baked_aliases;
      for(std::size_t i = 1; i < src.names.size(); ++i)
        baked_aliases.push_back(std::define_static_string(src.names[i]));
      auto span = std::define_static_array(std::span(baked_aliases));
      dst.aliases = span.data();
      dst.alias_count = span.size();
    }
  }
  if(!src.default_value_.empty())
    dst.default_value = std::define_static_string(src.default_value_);
  if(!src.implicit_value_.empty())
    dst.implicit_value = std::define_static_string(src.implicit_value_);
  if(!src.choices_.empty()) {
    std::vector<const char*> baked;
    for(auto& c : src.choices_)
      baked.push_back(std::define_static_string(c));
    auto span = std::define_static_array(std::span(baked));
    dst.choices = span.data();
    dst.choice_count = span.size();
  }
  dst.nargs = src.nargs_;
  dst.required = src.required_;
  dst.repeatable = src.repeatable_;
  dst.is_optional = src.type_ == ArgumentType::optional;

  // Validate default value against choices at definition time.
  if(dst.default_value && dst.choices) {
    bool valid = false;
    for(std::size_t i = 0; i < dst.choice_count; ++i)
      if(std::string_view(dst.choices[i]) ==
          std::string_view(dst.default_value)) {
        valid = true;
        break;
      }
    if(!valid)
      throw std::invalid_argument(std::format(
          "default value '{}' is not one of the choices", dst.default_value));
  }

  return dst;
}

constexpr std::string generate_usage(std::string_view program_name,
    std::span<const ArgSpec> positional, std::span<const ArgSpec> optional) {
  std::string result = "Usage:";
  if(!program_name.empty())
    result += std::string(" ") + std::string(program_name);
  for(auto& arg : optional) {
    if(arg.hidden_)
      continue;
    result += " [";
    std::string_view peak;
    for(auto& n : arg.names)
      if(n.size() > peak.size())
        peak = n;
    result += peak;
    bool hm = !arg.metavar_.empty();
    if(hm && arg.nargs_.is_exact() && arg.nargs_.min_ == 1)
      result += std::string(" ") + arg.metavar_;
    else if(!hm && arg.nargs_.is_exact() && arg.nargs_.min_ == 1)
      result += " VAR";
    if(arg.nargs_.min_ > 1)
      result += std::string(" ") + (hm ? arg.metavar_ : "...");
    if(arg.nargs_.min_ == 0 && arg.nargs_.max_ > 0 &&
        arg.nargs_.is_right_bounded())
      result += std::string(" [") + (hm ? arg.metavar_ : "VAR") + "]";
    if(!arg.nargs_.is_right_bounded())
      result += std::string(" ") + (hm ? arg.metavar_ : "...");
    if(arg.repeatable_)
      result += "...";
    result += "]";
  }
  for(auto& arg : positional) {
    if(arg.hidden_)
      continue;
    if(arg.required_) {
      result += " " + (arg.metavar_.empty() ? arg.names[0] : arg.metavar_);
      if(arg.nargs_.max_ > 1 || !arg.nargs_.is_right_bounded())
        result += "...";
    } else {
      result +=
          " [" + (arg.metavar_.empty() ? arg.names[0] : arg.metavar_) + "]";
    }
  }
  return result;
}

constexpr std::string generate_help(std::string_view program_name,
    std::string_view description, std::string_view epilog,
    std::span<const ArgSpec> positional, std::span<const ArgSpec> optional) {
  std::string result = generate_usage(program_name, positional, optional);

  if(!description.empty())
    result += "\n\n" + std::string(description);

  bool has_pos = false, has_opt = false;
  for(auto& a : positional)
    if(!a.hidden_)
      has_pos = true;
  for(auto& a : optional)
    if(!a.hidden_)
      has_opt = true;

  constexpr auto fmt_arg = [](const ArgSpec& arg, std::size_t indent,
                               std::size_t max_width) -> std::string {
    std::string result;
    std::string prefix(indent, ' ');
    if(arg.type_ == ArgumentType::optional)
      result += prefix + join(arg.names, ", ");
    else
      result += prefix + arg.names[0];

    if(!arg.metavar_.empty() && arg.nargs_.is_exact() && arg.nargs_.min_ == 1)
      result += std::string(" ") + arg.metavar_;
    else if(!arg.metavar_.empty())
      result += std::string(" ") + arg.metavar_;

    constexpr std::size_t hc = 24;
    std::size_t names_len = result.size() - indent;

    std::string help_str = arg.help_;
    std::vector<std::string> suffixes;
    if(!arg.nargs_.is_exact() ||
        (arg.nargs_.is_exact() && arg.nargs_.min_ != 1)) {
      std::string ns = arg.nargs_.to_string();
      if(!ns.empty()) {
        if(ns == "?")
          ns = "[?]";
        else if(ns == "*")
          ns = "[*]";
        else if(ns == "+")
          ns = "[+]";
        else
          ns = std::format("[nargs: {}]", ns);
        suffixes.push_back(ns);
      }
    }
    if(!arg.default_value_.empty())
      suffixes.push_back(std::format("[default: {}]", arg.default_value_));
    if(arg.required_)
      suffixes.push_back("[required]");
    if(arg.repeatable_)
      suffixes.push_back("[may be repeated]");
    if(!suffixes.empty()) {
      if(!help_str.empty())
        help_str += " ";
      help_str += join(suffixes, " ");
    }
    if(help_str.empty())
      return result;

    std::size_t help_start;
    if(names_len < hc) {
      result += std::string(hc - names_len, ' ');
      help_start = indent + hc;
    } else {
      result += " ";
      help_start = indent + names_len + 1;
    }
    std::size_t avail = (max_width > help_start) ? max_width - help_start : 40;
    std::size_t pos = 0;
    bool first_line = true;
    while(pos < help_str.size()) {
      if(!first_line)
        result += "\n" + std::string(help_start, ' ');
      first_line = false;
      std::size_t rem = help_str.size() - pos;
      if(rem <= avail) {
        result += help_str.substr(pos);
        break;
      }
      std::size_t take = avail;
      std::size_t sp = help_str.rfind(' ', pos + take);
      if(sp != std::string::npos && sp > pos)
        take = sp - pos;
      result += help_str.substr(pos, take);
      pos += take;
      while(pos < help_str.size() && help_str[pos] == ' ')
        ++pos;
    }
    return result;
  };

  if(has_pos) {
    result += "\n\nPositional arguments:";
    for(auto& arg : positional) {
      if(arg.hidden_)
        continue;
      result += "\n" + fmt_arg(arg, 2, 80);
    }
  }
  if(has_opt) {
    result += "\n\nOptional arguments:";
    for(auto& arg : optional) {
      if(arg.hidden_)
        continue;
      result += "\n" + fmt_arg(arg, 2, 80);
    }
  }
  if(!epilog.empty())
    result += "\n\n" + std::string(epilog);

  return result;
}

} // namespace cargparse

// ================================================================
// Exported: ArgumentParserSpec
// ================================================================

export namespace cargparse {

class ArgumentParserSpec {
public:
  consteval explicit ArgumentParserSpec(std::string program_name,
      bool add_help = true, bool add_version = true,
      std::string version = "1.0")
      : program_name_(std::move(program_name)),
        version_(std::move(version)),
        add_help_(add_help),
        add_version_(add_version) {}

  constexpr ArgumentParserSpec& add_description(std::string d) {
    description_ = std::move(d);
    return *this;
  }
  constexpr ArgumentParserSpec& add_epilog(std::string e) {
    epilog_ = std::move(e);
    return *this;
  }

  template <typename... Names>
  constexpr ArgSpec& add_argument(Names&&... names) {
    return add_argument_impl({std::string_view(names)...});
  }

  [[nodiscard]]
  consteval ArgumentParser bake() const;

  consteval operator ArgumentParser() const;

private:
  std::string program_name_;
  std::string version_;
  std::string description_, epilog_;
  std::vector<ArgSpec> positional_, optional_;
  bool add_help_, add_version_;

  constexpr ArgSpec& add_argument_impl(
      std::initializer_list<std::string_view> names) {
    if(names.size() == 0)
      throw std::invalid_argument("argument must have at least one name");
    ArgSpec arg;
    bool all_opt = true, all_pos = true;
    for(auto n : names) {
      if(is_valid_optional_name(n))
        all_pos = false;
      else
        all_opt = false;
      arg.names.emplace_back(n);
    }
    if(!all_opt && !all_pos)
      throw std::invalid_argument(
          "cannot mix positional and optional argument names");
    arg.type_ = all_opt ? ArgumentType::optional : ArgumentType::positional;
    // duplicate check
    for(auto& v : positional_)
      for(auto& n : v.names)
        for(auto& nn : arg.names)
          if(n == nn)
            throw std::invalid_argument(
                std::format("duplicate argument name: {}", nn));
    for(auto& v : optional_)
      for(auto& n : v.names)
        for(auto& nn : arg.names)
          if(n == nn)
            throw std::invalid_argument(
                std::format("duplicate argument name: {}", nn));

    if(arg.type_ == ArgumentType::positional) {
      positional_.push_back(std::move(arg));
      return positional_.back();
    } else {
      optional_.push_back(std::move(arg));
      return optional_.back();
    }
  }
};

// ================================================================
// Exported: ArgumentParser
// ================================================================

class ArgumentParser {
public:
  consteval ArgumentParser(const char* program_name,
      std::span<const RuntimeArg> args, PerfectHash hash, const char* help_data,
      std::size_t help_size, const char* usage_data, std::size_t usage_size,
      const char* version, bool add_help, bool add_version)
      : program_name_(program_name),
        args_(args),
        hash_(hash),
        help_data_(help_data),
        help_size_(help_size),
        usage_data_(usage_data),
        usage_size_(usage_size),
        version_(version),
        add_help_(add_help),
        add_version_(add_version) {}

  [[nodiscard]]
  constexpr std::string_view help() const {
    return std::string_view(help_data_, help_size_);
  }
  [[nodiscard]]
  constexpr std::string_view usage() const {
    return std::string_view(usage_data_, usage_size_);
  }
  [[nodiscard]]
  constexpr std::string_view program_name() const {
    return program_name_;
  }
  [[nodiscard]]
  constexpr std::string_view version() const {
    return version_ ? version_ : "";
  }

  [[nodiscard]]
  constexpr ParseResult parse(int argc, const char* const argv[]) const {
    return parse_impl(
        std::span(argv, static_cast<std::size_t>(argc)).subspan(1));
  }

  [[nodiscard]]
  constexpr ParseResult parse(std::span<const char* const> args) const {
    return parse_impl(args.subspan(1));
  }

  [[nodiscard]]
  constexpr ParseResult parse(std::initializer_list<const char*> args) const {
    return parse_impl(std::span(args).subspan(1));
  }

private:
  const char* program_name_;
  std::span<const RuntimeArg> args_;
  PerfectHash hash_;
  const char* help_data_;
  std::size_t help_size_;
  const char* usage_data_;
  std::size_t usage_size_;
  const char* version_ = nullptr;
  bool add_help_ = true;
  bool add_version_ = false;

  constexpr const RuntimeArg* find_arg(std::string_view name) const {
    auto idx = hash_.lookup(name);
    if(idx < args_.size()) {
      auto& a = args_[idx];
      if(a.check(name))
        return &a;
    }
    return nullptr;
  }

  // ========== Parsing engine ==========

  [[nodiscard]]
  constexpr ParseResult parse_impl(
      std::span<const char* const> argv_args) const {
    auto args = preprocess_arguments(argv_args);
    ParseResult result {hash_, args_};

    std::size_t positional_idx = 0;
    bool end_of_options = false;

    for(std::size_t i = 0; i < args.size();) {
      auto& token = args[i];

      if(!end_of_options && token == "--") {
        end_of_options = true;
        ++i;
        continue;
      }

      if(!end_of_options && is_valid_optional_name(token)) {
        // Compound short options
        if(token.size() >= 3 && token[0] == '-' && token[1] != '-' &&
            !is_numeric_literal(token.substr(1))) {
          auto* exact = find_arg(token);
          if(exact) {
            i = consume_optional(result, *exact, i, args, token);
          } else {
            bool all_flags = true;
            for(std::size_t ci = 1; ci < token.size(); ++ci) {
              std::string single = "-" + std::string(1, token[ci]);
              auto* ref = find_arg(single);
              if(!ref || !ref->is_flag()) {
                all_flags = false;
                break;
              }
            }
            if(all_flags && token.size() >= 3) {
              for(std::size_t ci = 1; ci < token.size(); ++ci) {
                std::string single = "-" + std::string(1, token[ci]);
                auto* ref = find_arg(single);
                (void) consume_optional(result, *ref, i, args, single);
              }
              ++i;
            } else {
              std::vector<std::string_view> names;
              for(auto& a : args_) {
                names.push_back(a.primary_name);
                for(std::size_t i = 0; i < a.alias_count; ++i)
                  names.push_back(a.aliases[i]);
              }
              auto sug = most_similar_string(token, names);
              auto msg = std::format("unknown argument: {}", token);
              if(!sug.empty())
                msg += std::format(" (did you mean '{}'?)", sug);
              throw std::invalid_argument(msg);
            }
          }
        } else {
          auto* ref = find_arg(token);
          if(ref) {
            i = consume_optional(result, *ref, i, args, token);
          } else if(token[0] == '-' && is_numeric_literal(token.substr(1))) {
            i = consume_positional(result, positional_idx, i, args,
                end_of_options);
            ++positional_idx;
          } else {
            std::vector<std::string_view> names;
            for(auto& a : args_) {
              names.push_back(a.primary_name);
              for(std::size_t i = 0; i < a.alias_count; ++i)
                names.push_back(a.aliases[i]);
            }
            auto sug = most_similar_string(token, names);
            auto msg = std::format("unknown argument: {}", token);
            if(!sug.empty())
              msg += std::format(" (did you mean '{}'?)", sug);
            throw std::invalid_argument(msg);
          }
        }
      } else {
        // Positional argument
        if(positional_idx < positional_count()) {
          i = consume_positional(result, positional_idx, i, args,
              end_of_options);
          ++positional_idx;
        } else {
          throw std::invalid_argument(
              std::format("unexpected positional argument: {}", token));
        }
      }
    }

    if(add_help_ && result.is_used("--help"))
      result.help_requested_ = true;
    if(add_version_ && result.is_used("--version"))
      result.version_requested_ = true;

    if(result.help_requested_ || result.version_requested_)
      return result;

    // Post-parse validation
    for(auto& a : args_) {
      if(!a.is_optional)
        continue;
      if(a.required && !result.is_used(a.primary_name))
        throw std::invalid_argument(
            std::format("required argument not provided: {}", a.names_csv()));
      if(result.is_used(a.primary_name)) {
        auto& vals = result.get_values(a.primary_name);
        bool implicit = a.is_flag();
        if(!implicit && !a.repeatable && !a.nargs.contains(vals.size()))
          throw std::invalid_argument(
              std::format("argument '{}' expects {} value(s), "
                          "got {}",
                  a.names_csv(), a.nargs.to_string(), vals.size()));
      }
    }

    for(auto& a : args_) {
      if(a.is_optional)
        continue;
      if(a.required && !result.is_used(a.primary_name))
        throw std::invalid_argument(
            std::format("required positional argument not "
                        "provided: {}",
                a.primary_name));
      if(result.is_used(a.primary_name) && !a.repeatable) {
        auto& vals = result.get_values(a.primary_name);
        if(!a.nargs.contains(vals.size())) {
          if(!a.required && vals.empty())
            continue;
          throw std::invalid_argument(
              std::format("argument '{}' expects {} value(s), "
                          "got {}",
                  a.primary_name, a.nargs.to_string(), vals.size()));
        }
      }
    }

    return result;
  }

  [[nodiscard]]
  constexpr std::size_t positional_count() const {
    std::size_t n = 0;
    for(auto& a : args_)
      if(!a.is_optional)
        ++n;
    return n;
  }

  [[nodiscard]]
  constexpr std::vector<std::string_view> preprocess_arguments(
      std::span<const char* const> argv) const {
    std::vector<std::string_view> result;
    result.reserve(argv.size());
    for(auto* arg : argv) {
      std::string_view sv(arg);
      if(sv.size() >= 2 && sv[0] == '-' && sv[1] != '-' &&
          is_numeric_literal(sv.substr(1))) {
        result.push_back(sv);
        continue;
      }
      auto eq_pos = sv.find('=');
      if(eq_pos != std::string_view::npos && eq_pos > 0) {
        std::string_view key(sv.data(), eq_pos);
        if(find_arg(key)) {
          result.push_back(key);
          result.push_back(
              std::string_view(sv.data() + eq_pos + 1, sv.size() - eq_pos - 1));
          continue;
        }
      }
      result.push_back(sv);
    }
    return result;
  }

  [[nodiscard]]
  constexpr std::size_t consume_optional(ParseResult& pr, const RuntimeArg& arg,
      std::size_t start_idx, const std::vector<std::string_view>& args,
      std::string_view used_name) const {
    auto& name = arg.primary_name;

    if(!arg.repeatable && pr.is_used(name))
      throw std::invalid_argument(
          std::format("duplicate argument: {}", used_name));

    if(arg.is_flag()) {
      if(arg.repeatable)
        pr.add_values(name, {arg.implicit_value});
      else
        pr.set_values(name, {arg.implicit_value});
      return start_idx + 1;
    }

    std::size_t next_idx = start_idx + 1;
    std::vector<std::string_view> consumed;

    while(next_idx < args.size() && consumed.size() < arg.nargs.max_) {
      auto& nt = args[next_idx];
      if(!is_numeric_literal(nt) && nt.size() >= 1 && nt[0] == '-' &&
          nt != "--" && is_valid_optional_name(nt))
        break;
      consumed.push_back(nt);
      ++next_idx;
    }

    if(consumed.size() < arg.nargs.min_) {
      if(arg.default_value != nullptr) {
        pr.set_values(name, {arg.default_value});
        return start_idx + 1;
      }
      throw std::invalid_argument(
          std::format("argument '{}' requires at least {} "
                      "value(s), got {}",
              arg.names_csv(), arg.nargs.min_, consumed.size()));
    }

    if(arg.has_choices()) {
      for(auto& v : consumed)
        if(!arg.choice_contains(v))
          throw std::invalid_argument(
              std::format("invalid choice '{}' for argument "
                          "'{}' (choose from {})",
                  v, arg.names_csv(),
                  join(std::span(arg.choices, arg.choice_count), ", ")));
    }

    if(arg.repeatable)
      pr.add_values(name, consumed);
    else
      pr.set_values(name, std::move(consumed));

    return next_idx;
  }

  [[nodiscard]]
  constexpr std::size_t consume_positional(ParseResult& pr,
      std::size_t positional_idx, std::size_t start_idx,
      const std::vector<std::string_view>& args, bool greedy) const {
    // Find the positional_idx-th positional arg
    const RuntimeArg* arg = nullptr;
    std::size_t pi = 0;
    for(auto& a : args_) {
      if(a.is_optional)
        continue;
      if(pi == positional_idx) {
        arg = &a;
        break;
      }
      ++pi;
    }
    if(!arg)
      return start_idx;

    auto name = arg->primary_name;
    std::size_t next_idx = start_idx;
    std::vector<std::string_view> consumed;

    while(next_idx < args.size() && consumed.size() < arg->nargs.max_) {
      auto& nt = args[next_idx];
      if(!greedy) {
        if(is_valid_optional_name(nt) && nt != "--")
          break;
      }
      consumed.push_back(nt);
      ++next_idx;
    }

    if(consumed.size() < arg->nargs.min_) {
      if(arg->default_value != nullptr) {
        pr.set_values(name, {arg->default_value});
        return start_idx;
      }
      throw std::invalid_argument(
          std::format("positional argument '{}' requires at "
                      "least {} value(s), got {}",
              name, arg->nargs.min_, consumed.size()));
    }

    if(arg->has_choices()) {
      for(auto& v : consumed)
        if(!arg->choice_contains(v))
          throw std::invalid_argument(std::format(
              "invalid choice '{}' for argument "
              "'{}' (choose from {})",
              v, name, join(std::span(arg->choices, arg->choice_count), ", ")));
    }

    pr.set_values(name, std::move(consumed));
    return next_idx;
  }
};

// ================================================================
// ArgumentParserSpec::bake()
// ================================================================

consteval ArgumentParser ArgumentParserSpec::bake() const {
  // Clone spec for help generation (adds auto-help/version)
  auto spec = *this;
  if(add_help_) {
    auto& a = spec.add_argument_impl({"-h", "--help"});
    a.help("Show this help message and exit");
    a.implicit_value("true");
    a.nargs_ = {0, 0};
  }
  if(add_version_) {
    auto& a = spec.add_argument_impl({"-v", "--version"});
    a.help("Show program version and exit");
    a.implicit_value("true");
    a.nargs_ = {0, 0};
  }

  // Generate and bake help/usage strings
  auto help_str = generate_help(program_name_, description_, epilog_,
      spec.positional_, spec.optional_);
  auto usage_str =
      generate_usage(program_name_, spec.positional_, spec.optional_);

  auto* help_data = std::define_static_string(std::string_view(help_str));
  auto* usage_data = std::define_static_string(std::string_view(usage_str));

  // Bake all arguments
  std::vector<RuntimeArg> baked_args;
  for(auto& a : spec.positional_)
    baked_args.push_back(bake_arg(a));
  for(auto& a : spec.optional_)
    baked_args.push_back(bake_arg(a));

  std::span<const RuntimeArg> arg_span;
  if(!baked_args.empty()) {
    auto span = std::define_static_array(baked_args);
    arg_span = span;
  }

  // Collect all names for perfect hash, with their args_ indices.
  std::vector<const char*> all_names;
  std::vector<std::size_t> arg_indices;
  for(std::size_t ai = 0; ai < baked_args.size(); ++ai) {
    auto& a = baked_args[ai];
    all_names.push_back(a.primary_name);
    arg_indices.push_back(ai);
    for(std::size_t i = 0; i < a.alias_count; ++i) {
      all_names.push_back(a.aliases[i]);
      arg_indices.push_back(ai);
    }
  }
  auto hash = make_perfect_hash(all_names, arg_indices, baked_args.size());

  return ArgumentParser(std::define_static_string(program_name_), arg_span,
      hash, help_data, help_str.size(), usage_data, usage_str.size(),
      std::define_static_string(version_), add_help_, add_version_);
}

consteval ArgumentParserSpec::operator ArgumentParser() const {
  return bake();
}

template <typename T>
T ParseResult::get(std::string_view name) const {
  auto resolved = resolve_name(name);
  auto it = entries_.find(resolved);

  if(it != entries_.end()) {
    if constexpr(!std::is_same_v<T, std::string> &&
        requires(T& c, std::string v) {
          typename T::value_type;
          c.push_back(convert_from_sv<typename T::value_type>(v));
        }) {
      T result;
      for(auto& v : it->second)
        result.push_back(convert_from_sv<typename T::value_type>(v));
      return result;
    } else {
      if(it->second.size() > 1)
        throw std::out_of_range(
            std::format("argument '{}' has multiple values; "
                        "use get<vector<T>>()",
                name));
      return convert_from_sv<T>(it->second[0]);
    }
  } else {
    auto* arg = find_arg(resolved);
    if(arg && arg->default_value) {
      if constexpr(!std::is_same_v<T, std::string> &&
          requires(T& c, std::string v) {
            typename T::value_type;
            c.push_back(convert_from_sv<typename T::value_type>(v));
          }) {
        T result;
        result.push_back(convert_from_sv<typename T::value_type>(
            arg->default_value));
        return result;
      } else {
        return convert_from_sv<T>(arg->default_value);
      }
    }
  }

  throw std::out_of_range(std::format("argument '{}' has no value", name));
}

template <typename T>
std::optional<T> ParseResult::present(std::string_view name) const {
  auto resolved = resolve_name(name);
  auto it = entries_.find(resolved);
  if(it == entries_.end())
    return std::nullopt;
  return get<T>(name);
}

bool ParseResult::is_used(std::string_view name) const {
  auto resolved = resolve_name(name);
  return entries_.find(resolved) != entries_.end();
}

const std::vector<std::string_view>& ParseResult::get_values(
    std::string_view name) const {
  auto resolved = resolve_name(name);
  auto it = entries_.find(resolved);
  if(it == entries_.end())
    throw std::out_of_range(std::format("unknown argument: {}", name));
  return it->second;
}

} // namespace cargparse
