#include <meta>

import std;
import cargparse;

consteval cargparse::ArgumentParser make_parser() {
  auto spec = cargparse::ArgumentParserSpec("cargparse-example", true, false)
                  .add_description("A demonstration of the cargparse library")
                  .add_epilog(
                      "For more info, visit "
                      "https://github.com/nickelpro/cargparse");
  spec.add_argument("input").help("Input file to process").required();
  spec.add_argument("-o", "--output")
      .help("Output file")
      .metavar("FILE")
      .default_value("a.out");
  spec.add_argument("-v", "--verbose").help("Enable verbose output").flag();
  spec.add_argument("-n", "--count").help("Number of iterations").metavar("N");
  spec.add_argument("--mode")
      .help("Operation mode")
      .choices({"read", "write", "append"})
      .default_value("read");
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

  if(result.version_requested()) {
    std::println("{}", parser.version());
    return 0;
  }

  std::println("Input:    {}", result.get("input"));
  std::println("Output:   {}", result.get("--output"));
  std::println("Verbose:  {}", result.get<bool>("--verbose") ? "yes" : "no");
  std::println("Mode:     {}", result.get("--mode"));

  if(result.is_used("--count"))
    std::println("Count:    {}", result.get<int>("--count"));
}
