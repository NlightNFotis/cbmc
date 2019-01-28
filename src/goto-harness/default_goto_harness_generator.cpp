#include "default_goto_harness_generator.h"
#include "default_goto_harness_generator_options.h"

#include <iostream>

#include <goto-programs/goto_model.h>

#include <util/exception_utils.h>

void default_goto_harness_generatort::handle_option(const irep_idt &option, const cmdlinet &cmdline)
{
  if(option == DEFAULT_GOTO_HARNESS_GENERATOR_HELLO_OPT) {
    got_greeted = true;
  } else {
    throw invalid_command_line_argument_exceptiont("default generator can not handle this option",
                                                   "--" + id2string(option));
  }
}

void default_goto_harness_generatort::generate(goto_modelt &goto_model)
{
  if(got_greeted) {
    std::cout << "Hello to you too!\n";
  } else {
    std::cout << "You can greet me with --hello\n";
  }
}
