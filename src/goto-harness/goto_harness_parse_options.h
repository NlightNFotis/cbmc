/******************************************************************\

Module: goto_harness_parse_options

Author: Diffblue Ltd.

\******************************************************************/

#ifndef CRPOVER_GOTO_HARNESS_GOTO_HARNESS_PARSE_OPTIONS_H
#define CRPOVER_GOTO_HARNESS_GOTO_HARNESS_PARSE_OPTIONS_H

#include <util/parse_options.h>
#include <util/message.h>
#include <util/ui_message.h>

#include <goto-programs/goto_model.h>

#include "goto_harness_generator_factory.h"
#include "default_goto_harness_generator_options.h"

#define GOTO_HARNESS_OPTIONS \
  "(version)" \
  GOTO_HARNESS_FACTORY_OPTIONS \
  DEFAULT_GOTO_HARNESS_GENERATOR_OPTIONS \
  // end GOTO_HARNESS_OPTIONS

class goto_harness_parse_optionst : 
  public parse_options_baset,
  public messaget
{
public:
  int doit() override;
  void help() override;

  goto_harness_parse_optionst(int argc, const char **argv):
    parse_options_baset(GOTO_HARNESS_OPTIONS, argc, argv),
    messaget(ui_message_handler),
    ui_message_handler(cmdline, "goto-harness")
  { 
  }

protected:
  ui_message_handlert ui_message_handler;

  goto_modelt goto_model;
};

#endif // CRPOVER_GOTO_HARNESS_GOTO_HARNESS_PARSE_OPTIONS_H
