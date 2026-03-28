#include "../src/options.h"

#include <cassert>
#include <filesystem>
#include <functional>
#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

std::filesystem::path prog;

namespace {

struct Argv {
  std::vector<std::string> args;
  std::vector<char *> ptrs;

  Argv(std::initializer_list<std::string_view> init)
  {
    args.reserve(init.size());
    ptrs.reserve(init.size());
    for (auto arg : init)
      args.emplace_back(arg);
    for (auto &arg : args)
      ptrs.push_back(arg.data());
  }
};

void
expect_error(std::string_view msg, const std::function<void()> &f)
{
  try {
    f();
    assert(false);
  } catch (const Options::Error &e) {
    assert(std::string_view(e.what()) == msg);
  }
}

void
test_parse_argv()
{
  bool a = false;
  bool b = false;
  std::string dir;
  std::string opt = "unset";

  Options o;
  o("-a", [&] { a = true; });
  o("-b", [&] { b = true; });
  o("-d", "--dir", [&](std::string arg) { dir = arg; });
  o("-o", "--opt", [&](std::string arg = "default") { opt = arg; });

  Argv argv{"prog", "-ab", "-dpath", "-o", "file"};
  auto rest = o.parse_argv(argv.ptrs.size(), argv.ptrs.data());

  assert(a);
  assert(b);
  assert(dir == "path");
  assert(opt == "default");
  assert(rest.size() == 1);
  assert(std::string_view(rest.front()) == "file");
}

void
test_parse_stops()
{
  bool seen = false;

  Options o;
  o("-a", [&] { seen = true; });

  {
    Argv argv{"prog", "-a", "file", "--ignored"};
    auto rest = o.parse_argv(argv.ptrs.size(), argv.ptrs.data());
    assert(seen);
    assert(rest.size() == 2);
    assert(std::string_view(rest[0]) == "file");
  }

  {
    Argv argv{"prog", "--", "-a"};
    auto rest = o.parse_argv(argv.ptrs.size(), argv.ptrs.data());
    assert(rest.size() == 1);
    assert(std::string_view(rest[0]) == "-a");
  }
}

void
test_parse_errors()
{
  Options o;
  o("-a", [] {});
  o("--all", [] {});
  o("-d", "--dir", [](std::string) {});

  expect_error("unknown option --bad", [&] {
    Argv argv{"prog", "--bad"};
    o.parse_argv(argv.ptrs.size(), argv.ptrs.data());
  });
  expect_error("option -d requires an argument", [&] {
    Argv argv{"prog", "-d"};
    o.parse_argv(argv.ptrs.size(), argv.ptrs.data());
  });
  expect_error("option --all takes no argument", [&] {
    Argv argv{"prog", "--all=x"};
    o.parse_argspan(std::span{argv.args}.subspan(1));
  });

  bool seen = false;
  Options p;
  p("-a", [&] { seen = true; });
  expect_error("unknown option -b", [&] {
    Argv argv{"prog", "-ab"};
    p.parse_argv(argv.ptrs.size(), argv.ptrs.data());
  });
  assert(seen);
}

void
test_complete_args()
{
  using C = Options::Completions;

  Options o;
  o("-a", [] {});
  o("-b", "--beta", [] {});
  o("-d", "--dir", [](std::string) {});
  o("-o", "--opt", [](std::string arg = {}) { (void)arg; });

  {
    Argv argv{"prog", "-"};
    auto c = o.complete_args(1, argv.ptrs.size(), argv.ptrs.data());
    assert(c.kind == C::kRawCompletions);
    assert((c.vals == std::vector<std::string>{
               "--beta ", "--dir ", "--opt=", "-a ", "-b ", "-d ", "-o"}));
  }
  {
    Argv argv{"prog", "--"};
    auto c = o.complete_args(1, argv.ptrs.size(), argv.ptrs.data());
    assert(c.kind == C::kRawCompletions);
    assert((c.vals ==
            std::vector<std::string>{"--beta ", "--dir ", "--opt="}));
  }
  {
    Argv argv{"prog", "-a"};
    auto c = o.complete_args(1, argv.ptrs.size(), argv.ptrs.data());
    assert(c.kind == C::kRawCompletions);
    assert((c.vals == std::vector<std::string>{"-a "}));
  }
  {
    Argv argv{"prog", "-d"};
    auto c = o.complete_args(1, argv.ptrs.size(), argv.ptrs.data());
    assert(c.kind == C::kRawCompletions);
    assert((c.vals == std::vector<std::string>{"-d "}));
  }
  {
    Argv argv{"prog", "-o"};
    auto c = o.complete_args(1, argv.ptrs.size(), argv.ptrs.data());
    assert(c.kind == C::kArgCompletions);
    assert((c.vals == std::vector<std::string>{"-o", "", "-o"}));
  }
  {
    Argv argv{"prog", "--dir", "fo"};
    auto c = o.complete_args(1, argv.ptrs.size(), argv.ptrs.data());
    assert(c.kind == C::kArgCompletions);
    assert((c.vals == std::vector<std::string>{"--dir", "fo", ""}));
  }
  {
    Argv argv{"prog", "--dir=fo"};
    auto c = o.complete_args(1, argv.ptrs.size(), argv.ptrs.data());
    assert(c.kind == C::kArgCompletions);
    assert((c.vals == std::vector<std::string>{"--dir", "fo", "--dir="}));
  }
  {
    Argv argv{"prog", "-adfo"};
    auto c = o.complete_args(1, argv.ptrs.size(), argv.ptrs.data());
    assert(c.kind == C::kArgCompletions);
    assert((c.vals == std::vector<std::string>{"-d", "fo", "-ad"}));
  }
  {
    Argv argv{"prog", "--opt", "file"};
    auto c = o.complete_args(1, argv.ptrs.size(), argv.ptrs.data());
    assert(c.kind == 2);
    assert(c.vals.empty());
  }
  {
    Argv argv{"prog", "--bad", "-"};
    auto c = o.complete_args(1, argv.ptrs.size(), argv.ptrs.data());
    assert(c.kind == C::kNoCompletions);
  }
}

} // namespace

int
main()
{
  test_parse_argv();
  test_parse_stops();
  test_parse_errors();
  test_complete_args();
}
