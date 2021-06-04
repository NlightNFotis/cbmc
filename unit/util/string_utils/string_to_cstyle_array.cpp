/// \file string_to_cstr_array Unit Tests
/// \author Diffblue Ltd.

#include <testing-utils/use_catch.h>
#include <util/string_utils.h>

TEST_CASE(
  "Split a simple string into three parts",
  "[core][utils][string_utils][string_to_c_style_array]")
{
  const std::string test_string = "The Jabberwocky bites";
  char **result = string_to_cstr_array(test_string, " ");

  REQUIRE(strcmp(result[0], "The") == 0);
  REQUIRE(strcmp(result[1], "Jabberwocky") == 0);
  REQUIRE(strcmp(result[2], "bites") == 0);
  REQUIRE(result[3] == 0);
}

TEST_CASE(
  "Split an empty string",
  "[core][utils][string_utils][string_to_c_style_array]")
{
  const std::string test_string = "";
  char **result = string_to_cstr_array(test_string, " ");

  REQUIRE(result[0] == 0);
}

TEST_CASE(
  "Split a simple string into three parts with more spaces",
  "[core][utils][string_utils][string_to_c_style_array]")
{
  const std::string test_string = "The   Jabberwocky bites";
  char **result = string_to_cstr_array(test_string, " ");

  REQUIRE(strcmp(result[0], "The") == 0);
  REQUIRE(strcmp(result[1], "Jabberwocky") == 0);
  REQUIRE(strcmp(result[2], "bites") == 0);
  REQUIRE(result[3] == 0);
}

TEST_CASE(
  "Split a simple string into three parts with more spaces and tabs",
  "[core][utils][string_utils][string_to_c_style_array]")
{
  const std::string test_string = "The \tJabberwocky \tbites";
  char **result = string_to_cstr_array(test_string, " \t");

  REQUIRE(strcmp(result[0], "The") == 0);
  REQUIRE(strcmp(result[1], "Jabberwocky") == 0);
  REQUIRE(strcmp(result[2], "bites") == 0);
  REQUIRE(result[3] == 0);
}
