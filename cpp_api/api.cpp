#include "api.h"

#include <util/cmdline.h>
#include <util/config.h>
#include <util/message.h>
#include <util/options.h>

#include <goto-programs/goto_model.h>
#include <goto-programs/initialize_goto_model.h>

#include <ansi-c/ansi_c_language.h>
#include <langapi/mode.h>

#include <memory>
#include <options.h>

extern configt config;

cprover_api::cprover_api()
  : message_handler(
      util_make_unique<null_message_handlert>(null_message_handlert{})),
    options(util_make_unique<optionst>(optionst{}))
{
  // Needed to initialise the language options correctly
  cmdlinet cmdline;
  // config is global in config.cpp
  config.set(cmdline);
  // Initialise C language mode
  register_language(new_ansi_c_language);
}

cprover_api::cprover_api(const api_optionst &options)
  : message_handler(
      util_make_unique<null_message_handlert>(null_message_handlert{})),
    options(options.to_engine_options())
{
  // Needed to initialise the language options correctly
  cmdlinet cmdline;
  // config is global in config.cpp
  config.set(cmdline);
  // Initialise C language mode
  register_language(new_ansi_c_language);
}

cprover_api::~cprover_api() = default;

void cprover_api::load_model_from_files(const std::vector<std::string> &files)
{
  model = util_make_unique<goto_modelt>(
    initialize_goto_model(files, *message_handler, *options));
}
