/******************************************************************\

Module: goto_harness_parse_options

Author: Diffblue Ltd.

\******************************************************************/

#include <cstddef>
#include <iostream>
#include <string>
#include <set>
#include <utility>

#include <util/invariant.h>
#include <util/exit_codes.h>
#include <util/version.h>
#include <util/exception_utils.h>

#include <goto-programs/read_goto_binary.h>
#include <goto-programs/write_goto_binary.h>

#include "goto_harness_parse_options.h"
#include "goto_harness_generator_factory.h"

int goto_harness_parse_optionst::doit()
{
  if (cmdline.args.size() < 2)
  {
    // TODO:
    throw invalid_command_line_argument_exceptiont{
      "Expected two command line arguments",
      "",
      ""
    };
  }

  if(cmdline.isset("version"))
  {
    std::cout << CBMC_VERSION << '\n';
    return CPROVER_EXIT_SUCCESS;
  }

  if(!cmdline.isset(GOTO_HARNESS_GENERATOR_TYPE_OPT)) {
    help();
    throw invalid_command_line_argument_exceptiont{
      "required option not set",
      "--" GOTO_HARNESS_GENERATOR_TYPE_OPT
        };
  }
  
  auto harness_generator = goto_harness_generator_factory(cmdline.get_value(GOTO_HARNESS_GENERATOR_TYPE_OPT));
  CHECK_RETURN(harness_generator != nullptr);
  
  auto const common_options = std::set<irep_idt> {
    irep_idt{"version"},
    irep_idt{GOTO_HARNESS_GENERATOR_TYPE_OPT}
  };
  
  for(auto const &option : cmdline.option_names()) {
    auto const option_name_as_id = irep_idt{option};
    if(common_options.find(option_name_as_id) == common_options.end()) {
      harness_generator->handle_option(option_name_as_id, cmdline);
    }
  }

  // Read the input goto_binary
  auto result = read_goto_binary(cmdline.args[0], get_message_handler());
  if (!result.has_value()) {
    // TODO:
    throw invalid_command_line_argument_exceptiont{"", "", ""};
  }

  goto_model = std::move(result.value());
  
  harness_generator->generate(goto_model);

  if(write_goto_binary(cmdline.args[1], goto_model, get_message_handler()))
    return CPROVER_EXIT_CONVERSION_FAILED;  
  else
    return CPROVER_EXIT_SUCCESS;
}

void goto_harness_parse_optionst::help()
{
  auto align_center_with_border = [](const std::string &text) {
    auto const total_length = std::size_t{63};
    auto const border = std::string{"* *"};
    auto const fill = total_length - 2 * border.size() - text.size();
    auto const fill_left = fill / 2;
    auto const fill_right = fill - fill_left;
    return border + std::string(fill_left, ' ') + text +
           std::string(fill_right, ' ') + border;
  };
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
            << " goto-harness --version           show version\n"
            << DEFAULT_GOTO_HARNESS_GENERATOR_HELP;
}
