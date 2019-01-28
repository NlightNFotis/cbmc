/******************************************************************\

Module: default_goto_harness_generator

Author: Diffblue Ltd.

\******************************************************************/

#ifndef CRPOVER_GOTO_HARNESS_DEFAULT_GOTO_HARNESS_GENERATOR_H
#define CRPOVER_GOTO_HARNESS_DEFAULT_GOTO_HARNESS_GENERATOR_H

#include "goto_harness_generator_factory.h"

#include <string>

#include <goto-programs/goto_model.h>

#include <util/cmdline.h>

struct default_goto_harness_generatort
  : public goto_harness_generatort
{
  static constexpr const char* name = "default";

  bool got_greeted = false;
  void handle_option(const irep_idt &option, const cmdlinet &cmdline) override;
  void generate(goto_modelt &goto_model) override;
};

#endif // CRPOVER_GOTO_HARNESS_DEFAULT_GOTO_HARNESS_GENERATOR_H
