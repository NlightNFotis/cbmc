// Test file to try loading a GOTO-model into memory and running a sample verification run on it.
#include <iostream>
#include <vector>

#include <ansi-c/ansi_c_language.h>

#include <langapi/mode.h>

#include <goto-programs/goto_model.h>
#include <goto-programs/initialize_goto_model.h>

#include <util/config.h>
#include <util/cmdline.h>
#include <util/message.h>

#include "api.h"

extern api_depst api_deps;

int main(int argc, char *argv[])
{
    try {
        std::cout << "Hello from API stub" << std::endl;

        // Convert argv to vector of strings for initialize_goto_model
        std::vector<std::string> arguments(argv + 1, argv + argc);

        // Initialise API dependencies and global configuration in one step.
        initialize();

        auto model = load_model_from_files(arguments, *api_deps.opts);

        std::cout << "Successfully initialised goto_model" << std::endl;
        return 0;
    } catch (invalid_command_line_argument_exceptiont e) {
        std::cout << e.what() << std::endl;
    }
}


