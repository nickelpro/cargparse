#include <catch2/catch_test_macros.hpp>
#include <meta>

import cargparse;

using namespace cargparse;

// ================================================================
// Positional arguments
// ================================================================

TEST_CASE("Single positional argument", "[positional]") {
  constexpr auto parser = []() consteval {
    auto spec = ArgumentParserSpec("test", false, false);
    spec.add_argument("input");
    return spec.bake();
  }();
  auto result = parser.parse({"test", "file.txt"});
  REQUIRE(result.get<std::string>("input") == "file.txt");
  REQUIRE(result.is_used("input"));
  REQUIRE(result);
}

TEST_CASE("Multiple positional arguments", "[positional]") {
  constexpr auto parser = []() consteval {
    auto spec = ArgumentParserSpec("test", false, false);
    spec.add_argument("src");
    spec.add_argument("dst");
    return spec.bake();
  }();
  auto result = parser.parse({"test", "source.txt", "dest.txt"});
  REQUIRE(result.get<std::string>("src") == "source.txt");
  REQUIRE(result.get<std::string>("dst") == "dest.txt");
}

TEST_CASE("Positional with nargs=2", "[positional][nargs]") {
  constexpr auto parser = []() consteval {
    auto spec = ArgumentParserSpec("test", false, false);
    spec.add_argument("files").nargs(2);
    return spec.bake();
  }();
  auto result = parser.parse({"test", "a.txt", "b.txt"});
  auto files = result.get<std::vector<std::string>>("files");
  REQUIRE(files.size() == 2);
}

TEST_CASE("Positional with nargs=*", "[positional][nargs]") {
  constexpr auto parser = []() consteval {
    auto spec = ArgumentParserSpec("test", false, false);
    spec.add_argument("files").nargs(NArgsPattern::any);
    return spec.bake();
  }();
  auto result = parser.parse({"test", "a", "b", "c"});
  auto files = result.get<std::vector<std::string>>("files");
  REQUIRE(files.size() == 3);
}

TEST_CASE("Positional with default value", "[positional]") {
  constexpr auto parser = []() consteval {
    auto spec = ArgumentParserSpec("test", false, false);
    spec.add_argument("input").default_value("default.txt");
    return spec.bake();
  }();
  auto result = parser.parse({"test"});
  REQUIRE(result.get<std::string>("input") == "default.txt");
}

TEST_CASE("Positional required throws", "[positional][error]") {
  constexpr auto parser = []() consteval {
    auto spec = ArgumentParserSpec("test", false, false);
    spec.add_argument("input").required();
    return spec.bake();
  }();
  REQUIRE_THROWS_AS(parser.parse({"test"}), std::exception);
}

// ================================================================
// Optional arguments
// ================================================================

TEST_CASE("Optional argument with value", "[optional]") {
  constexpr auto parser = []() consteval {
    auto spec = ArgumentParserSpec("test", false, false);
    spec.add_argument("-o", "--output");
    return spec.bake();
  }();
  auto result = parser.parse({"test", "-o", "out.txt"});
  REQUIRE(result.get<std::string>("--output") == "out.txt");
}

TEST_CASE("Optional --name=value", "[optional]") {
  constexpr auto parser = []() consteval {
    auto spec = ArgumentParserSpec("test", false, false);
    spec.add_argument("-o", "--output");
    return spec.bake();
  }();
  auto result = parser.parse({"test", "--output=result.txt"});
  REQUIRE(result.get<std::string>("--output") == "result.txt");
}

TEST_CASE("Optional with default value", "[optional]") {
  constexpr auto parser = []() consteval {
    auto spec = ArgumentParserSpec("test", false, false);
    spec.add_argument("-o", "--output").default_value("a.out");
    return spec.bake();
  }();
  auto result = parser.parse({"test"});
  REQUIRE(result.get<std::string>("--output") == "a.out");
}

TEST_CASE("Optional required throws", "[optional][error]") {
  constexpr auto parser = []() consteval {
    auto spec = ArgumentParserSpec("test", false, false);
    spec.add_argument("-o", "--output").required();
    return spec.bake();
  }();
  REQUIRE_THROWS_AS(parser.parse({"test"}), std::exception);
}

// ================================================================
// Flag arguments
// ================================================================

TEST_CASE("Flag argument", "[flag]") {
  constexpr auto parser = []() consteval {
    auto spec = ArgumentParserSpec("test", false, false);
    spec.add_argument("-v", "--verbose").flag();
    return spec.bake();
  }();
  auto result = parser.parse({"test", "-v"});
  REQUIRE(result.get<bool>("--verbose") == true);
}

TEST_CASE("Flag not present defaults to false", "[flag]") {
  constexpr auto parser = []() consteval {
    auto spec = ArgumentParserSpec("test", false, false);
    spec.add_argument("-v", "--verbose").flag();
    return spec.bake();
  }();
  auto result = parser.parse({"test"});
  REQUIRE(result.get<bool>("--verbose") == false);
}

// ================================================================
// Compound short options
// ================================================================

TEST_CASE("Compound short flags -abc", "[compound]") {
  constexpr auto parser = []() consteval {
    auto spec = ArgumentParserSpec("test", false, false);
    spec.add_argument("-a").flag();
    spec.add_argument("-b").flag();
    spec.add_argument("-c").flag();
    return spec.bake();
  }();
  auto result = parser.parse({"test", "-abc"});
  REQUIRE(result.get<bool>("-a") == true);
  REQUIRE(result.get<bool>("-b") == true);
  REQUIRE(result.get<bool>("-c") == true);
}

// ================================================================
// -- terminator
// ================================================================

TEST_CASE("Double dash treats following as positional", "[terminator]") {
  constexpr auto parser = []() consteval {
    auto spec = ArgumentParserSpec("test", false, false);
    spec.add_argument("input").nargs(NArgsPattern::any);
    spec.add_argument("-v", "--verbose").flag();
    return spec.bake();
  }();
  auto result = parser.parse({"test", "--", "-v", "file.txt"});
  REQUIRE(result.get<bool>("--verbose") == false);
  auto files = result.get<std::vector<std::string>>("input");
  REQUIRE(files.size() == 2);
  REQUIRE(files[0] == "-v");
}

// ================================================================
// Repeatable
// ================================================================

TEST_CASE("Repeatable argument collects all values", "[repeatable]") {
  constexpr auto parser = []() consteval {
    auto spec = ArgumentParserSpec("test", false, false);
    spec.add_argument("-I", "--include").append();
    return spec.bake();
  }();
  auto result =
      parser.parse({"test", "-I", "path1", "-I", "path2", "-I", "path3"});
  auto includes = result.get<std::vector<std::string>>("--include");
  REQUIRE(includes.size() == 3);
}

// ================================================================
// Choices
// ================================================================

TEST_CASE("Choices validation passes", "[choices]") {
  constexpr auto parser = []() consteval {
    auto spec = ArgumentParserSpec("test", false, false);
    spec.add_argument("color").choices({"red", "green", "blue"});
    return spec.bake();
  }();
  auto result = parser.parse({"test", "red"});
  REQUIRE(result.get<std::string>("color") == "red");
}

TEST_CASE("Choices validation throws", "[choices][error]") {
  constexpr auto parser = []() consteval {
    auto spec = ArgumentParserSpec("test", false, false);
    spec.add_argument("color").choices({"red", "green", "blue"});
    return spec.bake();
  }();
  REQUIRE_THROWS_AS(parser.parse({"test", "yellow"}), std::exception);
}

// ================================================================
// Type conversion
// ================================================================

TEST_CASE("get<int>", "[types]") {
  constexpr auto parser = []() consteval {
    auto spec = ArgumentParserSpec("test", false, false);
    spec.add_argument("-n", "--count");
    return spec.bake();
  }();
  auto result = parser.parse({"test", "-n", "42"});
  REQUIRE(result.get<int>("--count") == 42);
}

TEST_CASE("get<vector<int>>", "[types]") {
  constexpr auto parser = []() consteval {
    auto spec = ArgumentParserSpec("test", false, false);
    spec.add_argument("numbers").nargs(3);
    return spec.bake();
  }();
  auto result = parser.parse({"test", "10", "20", "30"});
  auto nums = result.get<std::vector<int>>("numbers");
  REQUIRE(nums == std::vector<int> {10, 20, 30});
}

// ================================================================
// present()
// ================================================================

TEST_CASE("present returns nullopt when not used", "[present]") {
  constexpr auto parser = []() consteval {
    auto spec = ArgumentParserSpec("test", false, false);
    spec.add_argument("-o", "--output");
    return spec.bake();
  }();
  auto result = parser.parse({"test"});
  auto val = result.present<std::string>("--output");
  REQUIRE(!val.has_value());
}

TEST_CASE("present returns value when used", "[present]") {
  constexpr auto parser = []() consteval {
    auto spec = ArgumentParserSpec("test", false, false);
    spec.add_argument("-o", "--output");
    return spec.bake();
  }();
  auto result = parser.parse({"test", "-o", "file.txt"});
  auto val = result.present<std::string>("--output");
  REQUIRE(val.has_value());
  REQUIRE(*val == "file.txt");
}

// ================================================================
// Help text
// ================================================================

TEST_CASE("Help includes usage and sections", "[help]") {
  constexpr auto parser = []() consteval {
    auto spec = ArgumentParserSpec("mytool");
    spec.add_argument("input").help("Input file");
    spec.add_argument("-o", "--output").help("Output file");
    return spec.bake();
  }();
  auto h = parser.help();
  REQUIRE(h.contains("Usage:"));
  REQUIRE(h.contains("mytool"));
  REQUIRE(h.contains("Positional arguments:"));
  REQUIRE(h.contains("input"));
  REQUIRE(h.contains("Optional arguments:"));
  REQUIRE(h.contains("--output"));
}

TEST_CASE("Help includes description and epilog", "[help]") {
  constexpr auto parser = []() consteval {
    auto spec = ArgumentParserSpec("test");
    spec.add_description("Desc.");
    spec.add_epilog("Epilog.");
    spec.add_argument("input").help("File");
    return spec.bake();
  }();
  auto h = parser.help();
  REQUIRE(h.contains("Desc."));
  REQUIRE(h.contains("Epilog."));
}

TEST_CASE("Help includes markers", "[help]") {
  constexpr auto parser = []() consteval {
    auto spec = ArgumentParserSpec("test");
    spec.add_argument("input").required().help("Required input");
    spec.add_argument("-o", "--output")
        .default_value("a.out")
        .help("Output file");
    spec.add_argument("files").nargs(3).help("Input files");
    spec.add_argument("-I", "--include").append().help("Include path");
    return spec.bake();
  }();
  auto h = parser.help();
  REQUIRE(h.contains("[required]"));
  REQUIRE(h.contains("[default: a.out]"));
  REQUIRE(h.contains("[nargs: 3]"));
  REQUIRE(h.contains("[may be repeated]"));
}

TEST_CASE("usage() returns usage string", "[help]") {
  constexpr auto parser = []() consteval {
    auto spec = ArgumentParserSpec("prog", false, false);
    spec.add_argument("input");
    spec.add_argument("-v", "--verbose").flag();
    return spec.bake();
  }();
  auto u = parser.usage();
  REQUIRE(u.contains("Usage: prog"));
  REQUIRE(u.contains("--verbose"));
}

// ================================================================
// Auto help/version
// ================================================================

TEST_CASE("Auto help flag by default", "[autohelp]") {
  constexpr auto parser = []() consteval {
    auto spec = ArgumentParserSpec("test");
    return spec.bake();
  }();
  auto result = parser.parse({"test", "-h"});
  REQUIRE(result.help_requested());
}

// ================================================================
// Edge cases
// ================================================================

TEST_CASE("Empty args uses defaults", "[edge]") {
  constexpr auto parser = []() consteval {
    auto spec = ArgumentParserSpec("test", false, false);
    spec.add_argument("input").default_value("stdin");
    spec.add_argument("-v", "--verbose").flag();
    return spec.bake();
  }();
  auto result = parser.parse({"test"});
  REQUIRE(result.get<std::string>("input") == "stdin");
  REQUIRE(result.get<bool>("--verbose") == false);
}

TEST_CASE("Operator bool on result", "[edge]") {
  constexpr auto parser = []() consteval {
    auto spec = ArgumentParserSpec("test", false, false);
    spec.add_argument("-v", "--verbose").flag();
    return spec.bake();
  }();
  auto r1 = parser.parse({"test"});
  REQUIRE(!r1);
  auto r2 = parser.parse({"test", "-v"});
  REQUIRE(r2);
}

TEST_CASE("Required argument with nargs=2", "[nargs]") {
  constexpr auto parser = []() consteval {
    auto spec = ArgumentParserSpec("test", false, false);
    spec.add_argument("coords").required().nargs(2);
    return spec.bake();
  }();
  auto result = parser.parse({"test", "10", "20"});
  auto vals = result.get<std::vector<std::string>>("coords");
  REQUIRE(vals.size() == 2);
}

// ================================================================
// Other
// ================================================================

TEST_CASE("get<int> rejects trailing text", "[types][error]") {
  constexpr auto parser = []() consteval {
    auto spec = ArgumentParserSpec("test", false, false);
    spec.add_argument("-n", "--count");
    return spec.bake();
  }();
  auto result = parser.parse({"test", "-n", "123abc"});
  REQUIRE_THROWS_AS(result.get<int>("--count"), std::exception);
}

TEST_CASE("get<double> rejects trailing text", "[types][error]") {
  constexpr auto parser = []() consteval {
    auto spec = ArgumentParserSpec("test", false, false);
    spec.add_argument("-p", "--price");
    return spec.bake();
  }();
  auto result = parser.parse({"test", "-p", "3.14xyz"});
  REQUIRE_THROWS_AS(result.get<double>("--price"), std::exception);
}

TEST_CASE("get<int> accepts valid integer", "[types]") {
  constexpr auto parser = []() consteval {
    auto spec = ArgumentParserSpec("test", false, false);
    spec.add_argument("-n", "--count");
    return spec.bake();
  }();
  auto result = parser.parse({"test", "-n", "42"});
  REQUIRE(result.get<int>("--count") == 42);
}

TEST_CASE("Negative float as positional value", "[positional]") {
  constexpr auto parser = []() consteval {
    auto spec = ArgumentParserSpec("test", false, false);
    spec.add_argument("value").nargs(NArgsPattern::any);
    return spec.bake();
  }();
  auto result = parser.parse({"test", "-1.5", "-0.25", "-1e5"});
  auto vals = result.get<std::vector<double>>("value");
  REQUIRE(vals.size() == 3);
  REQUIRE(vals[0] < -1.4);
  REQUIRE(vals[0] > -1.6);
  REQUIRE(vals[2] == -100000.0);
}

TEST_CASE("Lone dash is positional", "[positional]") {
  constexpr auto parser = []() consteval {
    auto spec = ArgumentParserSpec("test", false, false);
    spec.add_argument("file").nargs(NArgsPattern::any);
    return spec.bake();
  }();
  auto result = parser.parse({"test", "-", "file.txt"});
  auto vals = result.get<std::vector<std::string>>("file");
  REQUIRE(vals.size() == 2);
  REQUIRE(vals[0] == "-");
  REQUIRE(vals[1] == "file.txt");
}

TEST_CASE("Suggestions include aliases", "[error]") {
  constexpr auto parser = []() consteval {
    auto spec = ArgumentParserSpec("test", false, false);
    spec.add_argument("-o", "--output");
    return spec.bake();
  }();
  try {
    (void) parser.parse({"test", "--outpt"});
    FAIL("expected exception");
  } catch(const std::exception& e) {
    std::string msg(e.what());
    REQUIRE(msg.find("did you mean") != std::string::npos);
  }
}

TEST_CASE("Repeatable flag accumulates count", "[repeatable][flag]") {
  constexpr auto parser = []() consteval {
    auto spec = ArgumentParserSpec("test", false, false);
    spec.add_argument("-v", "--verbose").flag().append();
    return spec.bake();
  }();
  auto result = parser.parse({"test", "-v", "-v", "-v"});
  auto vals = result.get<std::vector<bool>>("--verbose");
  REQUIRE(vals.size() == 3);
}

TEST_CASE("Compound flags respect repeatable", "[compound][repeatable]") {
  constexpr auto parser = []() consteval {
    auto spec = ArgumentParserSpec("test", false, false);
    spec.add_argument("-a").flag().append();
    spec.add_argument("-b").flag().append();
    return spec.bake();
  }();
  auto r2 = parser.parse({"test", "-aba"});
  REQUIRE(r2.get<std::vector<bool>>("-a").size() == 2);
}

