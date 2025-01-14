// Copyright (C) 2020  Matthew "strager" Glazar
// See end of file for extended copyright information.

// These tests ensure that the parser implements the correct rules for .d.ts
// TypeScript definition files.

#include <gtest/gtest.h>
#include <quick-lint-js/diag-matcher.h>
#include <quick-lint-js/parse-support.h>
#include <quick-lint-js/port/char8.h>

namespace quick_lint_js {
namespace {
TEST(Test_Parse_TypeScript_Definition, const_without_initializer_is_allowed) {
  test_parse_and_visit_statement(u8"export const c;"_sv, no_diags,
                                 typescript_definition_options);
}

TEST(Test_Parse_TypeScript_Definition, variables_must_have_no_initializer) {
  test_parse_and_visit_module(u8"export const x;"_sv,  //
                              no_diags, typescript_definition_options);
  test_parse_and_visit_module(
      u8"export const x = null;"_sv,  //
      u8"               ^ Diag_DTS_Var_Cannot_Have_Initializer.equal\n"_diag
      u8"       ^^^^^ .declaring_token"_diag,
      typescript_definition_options);

  test_parse_and_visit_module(u8"declare const x;"_sv,  //
                              no_diags, typescript_definition_options);
  test_parse_and_visit_module(
      u8"declare const x = null;"_sv,  //
      u8"                ^ Diag_DTS_Var_Cannot_Have_Initializer.equal\n"_diag
      u8"        ^^^^^ .declaring_token"_diag,
      typescript_definition_options);

  test_parse_and_visit_module(
      u8"export let x = null;"_sv,  //
      u8"             ^ Diag_DTS_Var_Cannot_Have_Initializer.equal\n"_diag
      u8"       ^^^ .declaring_token"_diag,
      typescript_definition_options);
  test_parse_and_visit_module(
      u8"export var x = null;"_sv,  //
      u8"             ^ Diag_DTS_Var_Cannot_Have_Initializer.equal\n"_diag
      u8"       ^^^ .declaring_token"_diag,
      typescript_definition_options);
}
}
}

// quick-lint-js finds bugs in JavaScript programs.
// Copyright (C) 2020  Matthew "strager" Glazar
//
// This file is part of quick-lint-js.
//
// quick-lint-js is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// quick-lint-js is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with quick-lint-js.  If not, see <https://www.gnu.org/licenses/>.
