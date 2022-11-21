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

extern configt config;

// Default initialise the api_dependencies object
cbmc_api::api_dependenciest::api_dependenciest()
  : // We don't print anything to the console as part of the API
    message_handler(new null_message_handlert()),
    // We create a default options object which we are going to
    // customise later.
    options(new optionst())
{
}

cbmc_api::api_dependenciest::~api_dependenciest() = default;

cbmc_api::cbmc_api() : dependencies(new api_dependenciest())
{
  // Needed to initialise the language options correctly
  cmdlinet cmdline;
  // config is global in config.cpp
  config.set(cmdline);
  // Initialise C language mode
  register_language(new_ansi_c_language);
}

cbmc_api::~cbmc_api() = default;

void cbmc_api::load_model_from_files(const std::vector<std::string> &files)
{
  model = util_make_unique<goto_modelt>(initialize_goto_model(
    files, *dependencies->message_handler, *dependencies->options));
}
