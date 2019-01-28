/******************************************************************\

Module: goto_harness_main

Author: Diffblue Ltd.

\******************************************************************/

#include "goto_harness_parse_options.h"

int main(int argc, const char *argv[])
{
  goto_harness_parse_optionst parse_options(argc, argv);
  return parse_options.main();
}
