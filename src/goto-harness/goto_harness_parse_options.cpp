/******************************************************************\

Module: goto_harness_parse_options

Author: Diffblue Ltd.

\******************************************************************/

#include <cstddef>
#include <iostream>
#include <utility>
#include <string>

#include <util/exit_codes.h>
#include <util/exception_utils.h>
#include <util/version.h>
#include <goto-programs/read_goto_binary.h>
#include <goto-programs/write_goto_binary.h>
#include <goto-programs/goto_model.h>

#include "goto_harness_parse_options.h"


// input binary
// --> harness generator (with some config)
// --> output binary

int goto_harness_parse_optionst::doit()
{
  if(cmdline.isset("version"))
  {
    std::cout << CBMC_VERSION << '\n';
    return CPROVER_EXIT_SUCCESS;
  }
  if(cmdline.args.size() != 2)
  {
    help();
    throw invalid_command_line_argument_exceptiont {
      "need to specify both input and output goto binary file names (may be the same)",
      "<in goto binary> <out goto binary>"
    };
  }
  auto read_goto_binary_result = read_goto_binary(cmdline.args[0], get_message_handler());
  if(!read_goto_binary_result.has_value()) {
    throw deserialization_exceptiont {
      "failed to read goto program from file `" + cmdline.args[0] + "'"
    };
  }
  if(write_goto_binary(cmdline.args[1], read_goto_binary_result.value(), get_message_handler())) {
    throw system_exceptiont {
      "failed to write goto program from file `" + cmdline.args[1] + "'"
    };
  }
  return CPROVER_EXIT_SUCCESS;
}

void goto_harness_parse_optionst::help()
{
  std::cout << '\n'
            << banner_string("Goto-Harness", CBMC_VERSION) << '\n'
            << align_center_with_border("Copyright (C) 2019") << '\n'
            << align_center_with_border("Diffblue Ltd.") << '\n'
            << align_center_with_border("info@diffblue.com")
            // ^--- No idea if this is the right email address
            << '\n'
            << '\n'
            << "Usage:                       Purpose:\n"
            << '\n'
            << " goto-harness [-?] [-h] [--help]  show help\n"
            << " goto-harness --version           show version\n";
}

goto_harness_parse_optionst::goto_harness_parse_optionst(
  int argc,
  const char *argv[])
  : parse_options_baset{GOTO_HARNESS_OPTIONS, argc, argv, ui_message_handler},
    ui_message_handler(cmdline, std::string("GOTO-HARNESS ") + CBMC_VERSION)
{
}
