#include <ansi-c/ansi_c_language.h>

#include <goto-programs/goto_model.h>
#include <goto-programs/initialize_goto_model.h>

#include <langapi/mode.h>

#include <util/cmdline.h>
#include <util/message.h>
#include <util/options.h>

#include "api.h"

api_depst api_deps;
extern configt config;

void initialize() {
    // Initialise a null-message handler (we don't print anything in the API)
    api_deps.msg_handler = new null_message_handlert();
    // Initialise default options
    api_deps.opts = new optionst();
    // Needed to initialise the language options correctly
    cmdlinet cmdline;
    // config is global in config.cpp
    config.set(cmdline);
    // Initialise C language mode
    register_language(new_ansi_c_language);
}

goto_modelt load_model_from_files(
    const std::vector<std::string> &files,
    const optionst &options)
{
    // This could be nested inside a try/catch, but for now let's leave it alone.
    return initialize_goto_model(files, *api_deps.msg_handler, options);
}
