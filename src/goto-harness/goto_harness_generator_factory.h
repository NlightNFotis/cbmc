/******************************************************************\

Module: goto_harness_generator_factory

Author: Diffblue Ltd.

\******************************************************************/

#ifndef CRPOVER_GOTO_HARNESS_GOTO_HARNESS_GENERATOR_FACTORY_H
#define CRPOVER_GOTO_HARNESS_GOTO_HARNESS_GENERATOR_FACTORY_H

#include <memory>
#include <sstream>

#include <goto-programs/goto_model.h>

#include <util/cmdline.h>
#include <util/irep.h>
#include <util/invariant.h>

#define GOTO_HARNESS_GENERATOR_TYPE_OPT "harness-type"

#define GOTO_HARNESS_FACTORY_OPTIONS \
  "(" GOTO_HARNESS_GENERATOR_TYPE_OPT "):"

class goto_harness_generatort {
public:
  /// Handle a command line argument. Should throw an exception if the option
  /// doesn't apply to this generator. Should only set options rather than
  /// immediately performing work
  virtual void handle_option(const irep_idt& option, const cmdlinet& cmdline) = 0;

  /// Generate a harness according to the set options
  virtual void generate(goto_modelt &model) = 0;
};

std::unique_ptr<goto_harness_generatort> goto_harness_generator_factory(const irep_idt &generator_name);

#endif // CRPOVER_GOTO_HARNESS_GOTO_HARNESS_GENERATOR_FACTORY_H
