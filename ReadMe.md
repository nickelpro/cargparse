# cargparse

A constant expression-based command-line argument parser for C++26. Inspired by
[p-ranav/argparse](https://github.com/p-ranav/argparse).

## Import

```cpp
import cargparse;
```

All public types reside in `cargparse::`.

## ArgumentParserSpec

A consteval builder. Constructed at compile time, configured via fluent methods.

### Construction

```cpp
explicit ArgumentParserSpec(
    std::string program_name,
    bool add_help = true,
    bool add_version = true,
    std::string version = "1.0"
);
```

| Parameter | Default | Description |
|---|---|---|
| `program_name` | (required) | Name displayed in usage and help output. |
| `add_help` | `true` | Auto-add `-h`/`--help`. |
| `add_version` | `true` | Auto-add `-v`/`--version`. |
| `version` | `"1.0"` | Returned by `version()`. |

### Adding arguments

```cpp
template <typename... Names>
ArgSpec& add_argument(Names&&... names);
```

At least one name is required. Names starting with `-` are optional arguments;
all others are positional. The two cannot be mixed in one call.

Returns an `ArgSpec&` for fluent configuration.

### Metadata

```cpp
ArgumentParserSpec& add_description(std::string desc);
ArgumentParserSpec& add_epilog(std::string epi);
```

Returns an `ArgumentParserSpec&` for fluent configuration.

### Conversion

```cpp
consteval ArgumentParser bake() const;
consteval operator ArgumentParser() const;
```

`bake()` produces the runtime `ArgumentParser`. The conversion operator allows
`return spec;` in a function returning `ArgumentParser`.

## ArgSpec

Returned by `add_argument()`. Supports fluent configuration.

| Method | Description |
|---|---|
| `.help(std::string)` | Help text for the argument. |
| `.metavar(std::string)` | Display name in usage. |
| `.default_value(std::string v)` | Default when not supplied. Sets nargs min to 0. |
| `.implicit_value(std::string v)` | Value when present, no explicit value. Sets nargs to `{0,0}`. |
| `.flag()` | Shorthand: `default_value("false")` + `implicit_value("true")`. |
| `.required(bool = true)` | Mark as required. |
| `.nargs(std::size_t n)` | Exact number of values. |
| `.nargs(std::size_t min, std::size_t max)` | Value range. |
| `.nargs(NArgsPattern p)` | Predefined pattern. |
| `.append()` | Repeated occurrence; values accumulate. |
| `.hidden(bool = true)` | Exclude from help and usage. |
| `.choices(std::vector<std::string>)` | Restrict to allowed values. |

### NArgsPattern

| Value | Range |
|---|---|
| `NArgsPattern::optional` | 0..1 |
| `NArgsPattern::any` | 0..∞ |
| `NArgsPattern::at_least_one` | 1..∞ |

## ArgumentParser

The baked runtime parser. Constructed only via `ArgumentParserSpec::bake()` or
the implicit conversion operator.

All data is backed by static storage — zero heap allocation at construction.
Storable in a `constexpr` variable.

### Parsing

All parse methods are `[[nodiscard]]` and return a `ParseResult`.

```cpp
ParseResult parse(int argc, const char* const argv[]) const;
ParseResult parse(std::span<const char* const> arguments) const;
ParseResult parse(std::initializer_list<const char*> args) const;
```

All overloads skip the first element (program name).

### Help

```cpp
constexpr std::string_view usage() const;
constexpr std::string_view help() const;
```

Both return a `std::string_view` pointing to static binary data. `usage()`
produces a single-line usage string. `help()` produces full help output
including usage, description, argument sections, and epilog.

### Queries

```cpp
constexpr std::string_view program_name() const;
constexpr std::string_view version() const;
```

## ParseResult

Returned by `ArgumentParser::parse()`. Stores parsed values as strings.

### Value access

```cpp
template <typename T = std::string>
T get(std::string_view name) const;

template <typename T = std::string>
std::optional<T> present(std::string_view name) const;
```

`get` throws if the argument has no value. `present` returns `std::nullopt`
for unused arguments.

An argument can be looked up by any of its registered names (aliases are
resolved to the primary name).

### Supported types for get\<T\>

| `T` | Behaviour |
|---|---|
| `std::string` | As-is from the stored value string. |
| `bool` | `true`/`false`, `1`/`0`, `yes`/`no` (case-insensitive). |
| `int`, `long`, `long long` | Via `std::from_chars` (base-10). |
| `unsigned int`, `unsigned long`, `unsigned long long` | Via `std::from_chars` (base-10). |
| `float`, `double` | Via `std::from_chars` (general format). |
| `std::vector<T>` | Each stored value converted to `T`. |

All numeric conversions are strict — trailing text (`"123abc"`) is rejected.

### Queries

```cpp
bool is_used(std::string_view name) const;
bool help_requested() const;
bool version_requested() const;
explicit operator bool() const;
```

`is_used` means explicitly supplied on the command line. A default value does
not count as used. `operator bool` returns `true` if any argument was used.

## Auto-generated arguments

When `add_help` is `true`, `-h`/`--help` is added. When `add_version` is `true`,
`-v`/`--version` is added. When used, parsing succeeds immediately and
`ParseResult::help_requested()` or `version_requested()` returns `true`.
Required-argument validation is skipped.

## Perfect hash

Argument name lookup uses a compile-time-generated perfect hash table
(PEXT-based minimal distinguishing-bit mask). The table is constructed during
`bake()` via `make_perfect_hash` and stored in the `ArgumentParser`. At
runtime, lookup is a single PEXT operation plus array index.

## Build requirements

- C++26 compiler with C++20 modules support (`import std;`).
- C++26 standard library.
- CMake 4.4+.

In practice this means GCC 17 is the only supported compiler right now.

## Example

```cpp
#include <meta>

import std;
import cargparse;

consteval cargparse::ArgumentParser make_parser() {
  auto spec = cargparse::ArgumentParserSpec(
      "example", true, false, "1.0")
      .add_description("A demonstration.");
  spec.add_argument("input").help("Input file").required();
  spec.add_argument("-o", "--output")
      .help("Output file")
      .metavar("FILE")
      .default_value("a.out");
  spec.add_argument("-v", "--verbose")
      .help("Verbose output")
      .flag();
  spec.add_argument("-n", "--count")
      .help("Iterations")
      .metavar("N");
  return spec;
}

int main(int argc, const char* argv[]) {
  using namespace cargparse;

  constexpr auto parser = make_parser();

  auto result = parser.parse(argc, argv);

  if(result.help_requested()) {
    std::println("{}", parser.help());
    return 0;
  }

  std::println("Input:    {}", result.get("input"));
  std::println("Output:   {}", result.get("--output"));
  std::println("Verbose:  {}",
               result.get<bool>("--verbose") ? "yes" : "no");

  if(result.is_used("--count"))
    std::println("Count:    {}", result.get<int>("--count"));
}
```
